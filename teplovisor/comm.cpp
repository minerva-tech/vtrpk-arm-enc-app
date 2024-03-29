#include "ext_headers.h"

#include "pchbarrier.h"

#include <linux/serial.h>

//#include "logview.h" // todo: as comm.cpp will be used everywhere, qt dependencies should be removed from log.
#include "log_to_file.h"
#include "comm.h"

using std::min;

#ifdef __GNUC__
const int Comm::mss; // this line is necessary for g++ compilation as standard (9.4.2/4) requires that static const integral member of class still should be defined somewhere. Why cl doesn't know it?
#endif
#if INTERPROCESS_SINGLETON
Comm* Comm::m_this = NULL;
const char SHARED_MEM_NAME[] = "8e7rUzAf";
#endif

Log::PrintLine& operator << (Log& log, const Comm::Pkt& pkt)
{
	return log << Log::Dump << "Packet transmitted, addr = " << pkt.address << 
		", payload : " << std::hex << pkt.payload[0] << pkt.payload[1] << pkt.payload[2] << pkt.payload[3] << std::dec;
}

Comm::Pkt::Pkt()
{
}

Comm::Pkt::Pkt(uint8_t addr, int size, const uint8_t* p) :
	preamble(PREAMBLE),
	address(addr)
{
	assert(size <= mss);

	memcpy(payload, p, size);
	if (size<mss)
		memset(payload+size, 0, mss-size);

	boost::crc_optimal<8, crc_polynom>  crc_comp;
	crc_comp = std::for_each(payload, payload + mss, crc_comp);
	crc_comp(address);

	assert(crc_comp() < 256);

	crc = crc_comp();
}

uint8_t* Comm::Pkt::buf()
{
	return reinterpret_cast<uint8_t*>(this);
}

bool Comm::Pkt::crc_valid() const
{
	boost::crc_optimal<8, crc_polynom>  crc_comp;
	crc_comp = std::for_each(payload, payload + mss, crc_comp);
	crc_comp(address);

	assert(crc_comp() < 256);

	return crc_comp() == crc;
}

int Comm::Pkt::count_lsb() const
{
	return address & 0x0f;
}

int Comm::Pkt::port() const
{
	return address >> 4 & 0x3;
}

int Comm::Pkt::camera() const
{
	return address >> 6;
}

#if INTERPROCESS_SINGLETON
Comm& Comm::instance() {
	if (!m_this) {
		const int NAME_SIZE = 1024;
		interprocess::shared_memory_object::remove(SHARED_MEM_NAME);
		interprocess::managed_shared_memory seg(interprocess::open_or_create, SHARED_MEM_NAME, sizeof(Comm)+NAME_SIZE);
		Comm* tmp = seg.find_or_construct<Comm>(interprocess::unique_instance)();

//		m_this = seg.get_address_from_handle(tmp);
	}
	return *m_this;
}
#endif // #if INTERPROCESS_SINGLETON

int Comm::out_buf_size = 1000;

Comm::Comm() :
	m_port(m_io_service),
	m_camera_id(0),
//	m_work(m_io_service),
#if INTERPROCESS_SINGLETON
	m_owners(0),
#endif
	m_out_buf(out_buf_size),
	m_start(boost::chrono::steady_clock::now()),
	m_sending_in_progress(false),
	m_no_actual_transmitting(false),
	m_sending_time_us(0),
	m_send_size(0),
	m_in_ff_idx(0),
	m_cur(m_in_buf[m_in_ff_idx].begin()),
	m_remains(0)
{
//	asio::io_service::work work(m_io_service);

//	fout = fopen("dump.dat", "wb");

	memset(m_out_count_lsb, 0, sizeof(m_out_count_lsb));
	memset(m_in_count_lsb, 0, sizeof(m_in_count_lsb));

#if INTERPROCESS_SINGLETON
	get();
#endif
}

Comm::~Comm()
{
//	fclose(fout);
	
	close();

#if INTERPROCESS_SINGLETON
	release();
#endif
}

bool Comm::open(const std::string& port, int baud_rate, bool flow_control)
{
	close();

	boost::system::error_code ec;

	m_out_buf.reset();

	m_port.open(port, ec);

	if (ec) {
		log() << "Port opening error : " << ec.message();
		return false;
	}

	m_port.set_option(asio::serial_port::baud_rate(baud_rate)); // ? 
//	m_port.set_option(asio::serial_port::baud_rate(2400)); // ? 
	m_port.set_option(asio::serial_port::character_size(8));
//	m_port.set_option(asio::serial_port::flow_control(asio::serial_port::flow_control::software)); // ?
	m_port.set_option(asio::serial_port::flow_control(flow_control ? asio::serial_port::flow_control::hardware : asio::serial_port::flow_control::none)); // ?
//	m_port.set_option(asio::serial_port::flow_control(asio::serial_port::flow_control::hardware)); // ?
	m_port.set_option(asio::serial_port::parity(asio::serial_port::parity::none)); 
//	m_port.set_option(asio::serial_port::parity(asio::serial_port::parity::odd));
	m_port.set_option(asio::serial_port::stop_bits(asio::serial_port::stop_bits::one));

	m_sending_in_progress = false;
	m_in_ff_idx = 0;
	m_cur = m_in_buf[m_in_ff_idx].begin();
	m_remains = 0;

	m_port.async_read_some(asio::buffer(m_in_buf[m_in_ff_idx]), 
		bind(&Comm::recv_chunk, this, m_in_buf[m_in_ff_idx].begin(), asio::placeholders::error, asio::placeholders::bytes_transferred));

	m_thread = thread(bind(&asio::io_service::run, ref(m_io_service)));

	const int hnd = m_port.native_handle();

	struct serial_struct serial_str;
	ioctl(hnd, TIOCGSERIAL, &serial_str);
		
	log() << "Xmit buf size: " << serial_str.xmit_fifo_size;

	return true;
}

void Comm::close()
{
	boost::system::error_code ec;

	if (m_port.is_open()) {
//            m_io_service.post(bind(&asio::serial_port::close, &m_port, ec));
//            m_io_service.post([&](){m_port.close(ec);});
		m_port.close(ec);
	}

	this_thread::sleep(posix_time::milliseconds(125));

	if (ec)
		log() << "Port closing error : " << ec.message();

	m_io_service.stop();

	m_thread.join();

	m_io_service.reset();

	return;
}

void Comm::transmit_and_close()
{
    while (m_sending_in_progress)
        this_thread::yield();

    close();
}

#if INTERPROCESS_SINGLETON
void Comm::get() {
	m_owners++;
}

void Comm::release() {
	m_owners--;

	if (!m_owners) {
		interprocess::managed_shared_memory seg(interprocess::open_only, SHARED_MEM_NAME);
		seg.destroy<Comm>(interprocess::unique_instance);
		interprocess::shared_memory_object::remove(SHARED_MEM_NAME);
	}
}
#endif

//! Pointer p is available for use immediately after returning from transmit() because it will be copied to Pkt inside of transmit_pkt
size_t Comm::transmit(uint8_t cam, uint8_t port, size_t size, const uint8_t* p)
{
	const uint8_t* pend = p+size;

	m_buffered_size += size;

	while (p != pend) {

		if (m_out_buf.full())
			log() << "m_out_buf.full()";

		while (m_out_buf.full()) {
			return size-(pend-p); // according to mail from 05 Aug 2016
//			thread::yield(); 
		}

		{
		lock_guard<mutex> _(m_transmit_lock);

		while (p != pend && !m_out_buf.full()) {
			const int to_send = min(pend-p, static_cast<ptrdiff_t>(mss));

			const int id = (cam&3) << 6 | (port&3) << 4 | m_out_count_lsb[cam&3]++ & 0xF;

			m_out_count_lsb[cam&3] %= LSB_MAX_VAL_PLUS_1;

			transmit_pkt(id, to_send, p);

			p += to_send;
		}
		}
	}
	
	return size;
}

size_t Comm::transmit(uint8_t port, size_t size, const uint8_t* p) {
	return transmit(m_camera_id, port, size, p);
}

void Comm::setCameraID(int id) {
	m_camera_id = id;
}

int Comm::cameraID() const {
	return m_camera_id;
}

void Comm::transmit_pkt(uint8_t id, size_t size, const uint8_t* p)
{
//	log() << "out buf size : " << m_out_buf.size();
	
//	while (m_out_buf.full()) {
//		thread::yield();
//	}
	
//	{
//		lock_guard<mutex> _(m_transmit_lock);

		m_out_buf.push_back(Pkt(id, size, p));

		if (m_no_actual_transmitting)
			return;

		if (!m_sending_in_progress) {
			struct timespec clock_cur;
			clock_gettime(CLOCK_MONOTONIC, &clock_cur);
			m_send_start_us = (clock_cur.tv_nsec/1000 + clock_cur.tv_sec*1000000);

//			log() << "sending start us " << m_send_start_us;
//			log() << "Start sec: " << clock_cur.tv_sec << " nsec: " << clock_cur.tv_nsec;

			m_sending_in_progress = true;
			asio::async_write(m_port, m_out_buf.get_chunk(), bind(&Comm::transmitted, this, asio::placeholders::error, asio::placeholders::bytes_transferred));

//			const int hnd = m_port.native_handle();
//			tcflush(hnd, TCOFLUSH);
		}
//	}
}

void Comm::transmitted(const system::error_code& e, size_t size)
{
	if (e)
		log() << Log::Error << "Transmission error occurred: " << e;

	m_send_size += size;

	struct timespec clock_cur;
	clock_gettime(CLOCK_MONOTONIC, &clock_cur);
	m_sending_time_us += (clock_cur.tv_nsec/1000 + clock_cur.tv_sec*1000000) - m_send_start_us;
	
//	log() << "Transmission finished, finish time: " << (clock_cur.tv_nsec/1000 + clock_cur.tv_sec*1000000) << " size: " << size;
//	log() << "End sec: " << clock_cur.tv_sec << " nsec: " << clock_cur.tv_nsec << " size: " << size;
//	log() << "rate: " << size*8e6 / ((clock_cur.tv_nsec/1000 + clock_cur.tv_sec*1000000) - m_send_start_us);

	{
		lock_guard<mutex> _(m_transmit_lock);

		m_out_buf.taken(size);

		if (!m_out_buf.empty() && !m_no_actual_transmitting) {
			struct timespec clock_cur;
			clock_gettime(CLOCK_MONOTONIC, &clock_cur); // TODO: boost::chrono. Now it can't be built without posix.
			m_send_start_us = (clock_cur.tv_nsec/1000 + clock_cur.tv_sec*1000000);
			
//			log() << "sending start us " << m_send_start_us;
//			log() << "Start cont. sec: " << clock_cur.tv_sec << " nsec: " << clock_cur.tv_nsec;

			asio::async_write(m_port, m_out_buf.get_chunk(), bind(&Comm::transmitted, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
		} else {
			m_send_start_us = 0;
			
			m_sending_in_progress = false;
		}
	}
}

void Comm::drop_unsent() 
{
	lock_guard<mutex> _(m_transmit_lock);
	m_out_buf.drop_unsent();
}

void Comm::allowTransmission(bool val)
{
	m_no_actual_transmitting = !val;
}

bool Comm::isTransmissionAllowed() const
{
	return !m_no_actual_transmitting;
}

int Comm::getBufferedDataSize() const
{
	return m_buffered_size;
}

int Comm::getTransmissionRate() const
{
	return m_send_size*8e6 / m_sending_time_us;
}

int Comm::getTransmissionTime() const
{
	return m_sending_time_us/1e3;
}

int Comm::getBufferSize() const
{
	return m_out_buf.size()*100/out_buf_size;
}

void Comm::resetTransmissionRate()
{
	m_send_size = 0;
	m_sending_time_us = 0;
	m_buffered_size = 0;
	
	return;
}

/*
void Comm::transmitted(shared_ptr<Pkt> sp, const system::error_code& e, size_t)
{
	if (!e)
		log() << *sp;
	else
		log() << Log::Error << "Transmission error occurred: " << e;
}
*/

void Comm::recv_pkt(const Pkt* pkt)
{
	int comment = Invalid;

	if (!pkt->crc_valid()) {
		comment |= CrcError;
		log() << "CrcError";
	}

	if (pkt->count_lsb() != m_in_count_lsb[pkt->camera()])
		comment |= PacketLost;

	if (comment == Invalid)
		comment = Normal;

	m_in_count_lsb[pkt->camera()] = (pkt->count_lsb()+1) % LSB_MAX_VAL_PLUS_1;

	m_recv_amount[pkt->port()] += mss;

	if (m_callback[pkt->port()])
		m_callback[pkt->port()](pkt->camera(), pkt->payload, comment);

//	log() << *pkt;
}

void Comm::recv_chunk(uint8_t* p, const system::error_code& e, std::size_t bytes_transferred)
{
//	log() << "rcv : " << bytes_transferred;

//	fwrite(p, 1, bytes_transferred, fout);
	
//	if (!e) {
		if (e) {
			if (e != asio::error::operation_aborted)
				log() << Log::Error << "Error of data receiving : " << e.message();
			else
				log() << "Error of data receiving : operation aborted";

			if (!bytes_transferred)
				return;
		}

		uint8_t* pend = p + bytes_transferred;

		while (m_remains + pend - m_cur >= sizeof(Pkt)) {
			if (m_remains) {
				assert(m_cur == m_in_buf[m_in_ff_idx].begin());

				uint8_t* buf_end = m_in_buf[m_in_ff_idx^1].end();

				uint8_t* pkt_start = std::find(buf_end - m_remains, buf_end, PREAMBLE);

				if (buf_end - pkt_start + pend - m_cur < sizeof(Pkt)) {
					m_remains = buf_end - pkt_start;
					break;
				}

				if (pkt_start != m_in_buf[m_in_ff_idx^1].end()) {
					Pkt pkt;
					uint8_t* ppkt = pkt.buf();
					const ptrdiff_t len = buf_end - pkt_start;

					memcpy(ppkt, pkt_start, len);
					memcpy(ppkt + len, m_cur, sizeof(Pkt) - len);

					m_cur += sizeof(Pkt) - len;

					recv_pkt(&pkt);
				}

				m_remains = 0;
			}

			uint8_t* pkt_start = std::find(m_cur, pend, PREAMBLE);

			if (pend - pkt_start >= sizeof(Pkt)) {
				recv_pkt(reinterpret_cast<const Pkt*>(pkt_start));
				m_cur = pkt_start + sizeof(Pkt);
			} else {
				m_cur = pkt_start;
				break;
			}
		}

		if (pend < m_in_buf[m_in_ff_idx].end()) {
			m_port.async_read_some(asio::buffer(pend, m_in_buf[m_in_ff_idx].end() - pend), 
				bind(&Comm::recv_chunk, this, pend, asio::placeholders::error, asio::placeholders::bytes_transferred));
		} else {
			m_remains = m_in_buf[m_in_ff_idx].end() - m_cur;
			m_in_ff_idx = m_in_ff_idx^1;
			m_cur = m_in_buf[m_in_ff_idx].begin();
			m_port.async_read_some(asio::buffer(m_in_buf[m_in_ff_idx]),
				bind(&Comm::recv_chunk, this, m_cur, asio::placeholders::error, asio::placeholders::bytes_transferred));
		}
/*	} else if (e != asio::error::operation_aborted) {
		log() << Log::Error << "Error of data receiving : " << e.message();
	} else {
		log() << "Error of data receiving : operation aborted";
	}*/
}

/**
 * returns Amount of bytes received per second.
 * */
std::vector<std::pair<std::string, uint32_t> > Comm::getStat()
{
	std::vector<std::pair<std::string, uint32_t> > times;

	boost::chrono::duration<double> dur = boost::chrono::steady_clock::now() - m_start;
	double count = dur.count();

	times.push_back(std::pair<std::string, uint32_t>("Command bitrate: ", 		(m_recv_amount[0]+0.5)/count));
	times.push_back(std::pair<std::string, uint32_t>("Video bitrate: ", 		(m_recv_amount[1]+0.5)/count));
	times.push_back(std::pair<std::string, uint32_t>("Detector bitrate: ", 	(m_recv_amount[2]+0.5)/count));
	times.push_back(std::pair<std::string, uint32_t>("Auxiliary bitrate: ", 	(m_recv_amount[3]+0.5)/count));

	memset(m_recv_amount, 0, sizeof(m_recv_amount));

	m_start = boost::chrono::steady_clock::now();

	return times;
}
