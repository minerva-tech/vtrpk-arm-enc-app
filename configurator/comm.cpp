#include "ext_headers.h"

#include "pchbarrier.h"

#include "logview.h" // todo: as comm.cpp will be used everywhere, qt dependencies should be removed from log.
//#include "log_to_file.h"
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

Comm::Comm() :
	m_port(m_io_service),
    m_camera_id(0),
//	m_work(m_io_service),
#if INTERPROCESS_SINGLETON
	m_owners(0),
#endif
	m_out_buf(out_buf_size),
    m_sending_in_progress(false)
{
//	asio::io_service::work work(m_io_service);

//	fout = fopen("dump.dat", "wb");

	memset(m_out_count_lsb, 0, sizeof(m_out_count_lsb));
//	memset(m_in_count_lsb, 0, sizeof(m_in_count_lsb));
	std::fill(&m_in_count_lsb[0], &m_in_count_lsb[sizeof(m_in_count_lsb)/sizeof(m_in_count_lsb[0])], -1);

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

bool Comm::open(const std::string& addr, unsigned short port)
{
	close();

	boost::system::error_code ec;

    // TODO : Proper error handling
    m_port.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string(addr), port), ec);

	m_out_buf.reset();

	if (ec) {
		log() << "Port opening error : " << ec.message();
		return false;
	}

	m_sending_in_progress = false;
    m_in_buf.reset();

	std::fill(&m_in_count_lsb[0], &m_in_count_lsb[sizeof(m_in_count_lsb)/sizeof(m_in_count_lsb[0])], -1);

    m_port.async_read_some(asio::buffer(m_in_buf.buf),
        bind(&Comm::recv_ethernet_chunk, this, &m_in_buf.buf[0], asio::placeholders::error, asio::placeholders::bytes_transferred));

	m_thread = thread(bind(&asio::io_service::run, ref(m_io_service)));

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
    while (m_sending_in_progress) // TODO: timeout!
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

void Comm::transmit(uint8_t port, size_t size, const uint8_t* p)
{
    transmit(m_camera_id, port, size, p);
}

//! Pointer p is available for use immediately after returning from transmit() because it will be copied to Pkt inside of transmit_pkt
void Comm::transmit(uint8_t cam, uint8_t port, size_t size, const uint8_t* p)
{
	const uint8_t* pend = p+size;

	while (p != pend) {

		if (m_out_buf.full())
			log() << "m_out_buf.full()";

		while (m_out_buf.full())
			thread::yield();

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

		if (!m_sending_in_progress) {
			m_sending_in_progress = true;
			asio::async_write(m_port, m_out_buf.get_chunk(), bind(&Comm::transmitted, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
		}
//	}
}

void Comm::transmitted(const system::error_code& e, size_t size)
{
	if (e)
		log() << Log::Error << "Transmission error occurred: " << e;

	{
		lock_guard<mutex> _(m_transmit_lock);

        m_out_buf.taken(size);

		if (!m_out_buf.empty())
			asio::async_write(m_port, m_out_buf.get_chunk(), bind(&Comm::transmitted, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
		else
			m_sending_in_progress = false;
	}
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

void Comm::recv_pkt(const Pkt* pkt, int cam_id_override)
{
	int comment = Invalid;

    if (!pkt->crc_valid()) {
        log() << "CrcError";
		comment |= CrcError;
    }

//	if (pkt->count_lsb() != m_in_count_lsb[pkt->camera()] && m_in_count_lsb[pkt->camera()] >= 0)
//		comment |= PacketLost;

	if (comment == Invalid) 
		comment = Normal;

    m_in_count_lsb[pkt->camera()] = (pkt->count_lsb()+1) % LSB_MAX_VAL_PLUS_1; // TODO calculated wrong

    m_recv_amount[pkt->port()] += mss; // TODO calculated sum of all cams

    if (m_callback[pkt->port()] && comment == Normal) {
        if (cam_id_override >= 0)
            m_callback[pkt->port()](cam_id_override, pkt->payload, comment);
        else
            m_callback[pkt->port()](pkt->camera(), pkt->payload, comment);
    }

//	log() << *pkt;
}

// TODO : recv_*_chunk MUST be rewritten
void Comm::recv_ethernet_chunk(uint8_t* p, const system::error_code& e, std::size_t bytes_transferred)
{
    if (e) {
        if (e != asio::error::operation_aborted)
            log() << Log::Error << "Error of data receiving : " << e.message();
        else
            log() << "Error of data receiving : operation aborted";

        if (!bytes_transferred)
            return;
    }

    uint8_t* pend = p + bytes_transferred;

    m_in_buf.writing_pt += bytes_transferred;

    while (m_in_buf.remains + m_in_buf.writing_pt - m_in_buf.cur >= sizeof(EthernetPkt)) {
        if (m_in_buf.remains) {
            assert(m_in_buf.cur == 0);

            uint8_t* buf_end = m_in_buf.buf.end();

            uint8_t* pkt_start = buf_end - m_in_buf.remains; // No data loss or corruptions, right?

            EthernetPkt pkt;
            uint8_t* ppkt = reinterpret_cast<uint8_t*>(&pkt);

            memcpy(ppkt, pkt_start, m_in_buf.remains);
            memcpy(ppkt + m_in_buf.remains, m_in_buf.buf.begin(), sizeof(EthernetPkt) - m_in_buf.remains);

            m_in_buf.cur += sizeof(EthernetPkt) - m_in_buf.remains;

            recv_chunk(pkt.payload.data, e, pkt.payload.len, pkt.payload.header.uni_net_addr.device_number);

            m_in_buf.remains = 0;
        }

        uint8_t* pkt_start = m_in_buf.buf.begin() + m_in_buf.cur;

        if (m_in_buf.buf.begin() + m_in_buf.writing_pt - pkt_start >= sizeof(EthernetPkt)) {
            EthernetPkt* pkt = reinterpret_cast<EthernetPkt*>(pkt_start);
            recv_chunk(pkt->payload.data, e, pkt->payload.len, pkt->payload.header.uni_net_addr.device_number);
            m_in_buf.cur += sizeof(EthernetPkt);
        }
    }

    if (pend < m_in_buf.buf.end()) {
        assert(pend == m_in_buf.buf.begin()+m_in_buf.writing_pt);
        m_port.async_read_some(asio::buffer(pend, m_in_buf.buf.end() - pend),
            bind(&Comm::recv_ethernet_chunk, this, pend, asio::placeholders::error, asio::placeholders::bytes_transferred));
    } else {
        m_in_buf.remains = m_in_buf.buf.size() - m_in_buf.cur;
        m_in_buf.cur = 0;
        m_in_buf.writing_pt = 0;
        m_port.async_read_some(asio::buffer(m_in_buf.buf),
            bind(&Comm::recv_ethernet_chunk, this, m_in_buf.buf.begin(), asio::placeholders::error, asio::placeholders::bytes_transferred));
    }
}

void Comm::recv_chunk(uint8_t* p, const system::error_code& e, std::size_t bytes_transferred, uint8_t cam_id)
{
    Buf& in_buf = m_in_buf_cam[cam_id];

    assert(in_buf.remains < sizeof(Pkt));

    if (in_buf.remains) {
        if (bytes_transferred+in_buf.remains >= sizeof(Pkt)) {
            uint8_t* pkt_start = std::find(in_buf.buf.begin(), in_buf.buf.begin() + in_buf.remains, PREAMBLE);

            if (pkt_start == in_buf.buf.end()) {
                Pkt pkt;
                uint8_t* ppkt = pkt.buf();

                const size_t first_part_len = in_buf.remains - (pkt_start - in_buf.buf.begin());

                memcpy(ppkt, pkt_start, first_part_len);

                const size_t second_part_len = sizeof(Pkt)-first_part_len;

                memcpy(ppkt+first_part_len, p, second_part_len);

                p += second_part_len;
                bytes_transferred -= second_part_len;

                recv_pkt(&pkt, cam_id);
            }

            in_buf.remains = 0;
            in_buf.writing_pt = 0;
        } else {
            memcpy(in_buf.buf.begin()+in_buf.writing_pt, p, bytes_transferred);
            in_buf.writing_pt += bytes_transferred;
            in_buf.remains += bytes_transferred;
            p += bytes_transferred;
            bytes_transferred = 0;
        }
    }

    while (bytes_transferred > sizeof(Pkt)) {
        const uint8_t* pend = p + bytes_transferred;

        uint8_t* pkt_start = std::find(p, p + bytes_transferred, PREAMBLE);

        if (pend - pkt_start >= sizeof(Pkt)) {
            recv_pkt((Pkt*)pkt_start, cam_id);
            p += sizeof(Pkt);
            bytes_transferred -= sizeof(Pkt);
        } else {
            bytes_transferred = pend-pkt_start;
            p = pkt_start;
        }
    }

    memcpy(in_buf.buf.begin() + in_buf.writing_pt, p, bytes_transferred);
    in_buf.writing_pt += bytes_transferred;
    in_buf.remains += bytes_transferred; // TODO : remains == wrinting_pt huh?
}

/**
 * returns Amount of bytes received per second.
 * */
void Comm::getStat(int bitrates[4])
{
//	std::vector<std::pair<std::string, uint32_t> > times;

	boost::chrono::duration<double> dur = boost::chrono::steady_clock::now() - m_start;
	double count = dur.count();

/*	times.push_back(std::pair<std::string, uint32_t>("Command bitrate: ", 		(m_recv_amount[0]+0.5)/count));
	times.push_back(std::pair<std::string, uint32_t>("Video bitrate: ", 		(m_recv_amount[1]+0.5)/count));
	times.push_back(std::pair<std::string, uint32_t>("Detector bitrate: ", 		(m_recv_amount[2]+0.5)/count));
	times.push_back(std::pair<std::string, uint32_t>("Auxiliary bitrate: ", 	(m_recv_amount[3]+0.5)/count));
*/
	for (int i=0; i<4; i++)
		bitrates[i] = (m_recv_amount[i]+0.5)/count;

	memset(m_recv_amount, 0, sizeof(m_recv_amount));

	m_start = boost::chrono::steady_clock::now();

//	return times;
}
