#include "ext_headers.h"

#include "pchbarrier.h"

#include <chrono>

#include "log_to_file.h"

#include "control.h"

#include "defines.h"

#include "utils.h"

using namespace boost;

SerialComm::SerialComm(const std::string& port, SerialParser* parser) :
	m_port(m_io_service),
	m_parser(parser)
{
	boost::system::error_code ec;

	m_port.open(port, ec);

	if (ec) {
		log() << "Control port opening error : " << ec.message();
		return;
	}

	m_port.set_option(asio::serial_port::character_size(8));
	m_port.set_option(asio::serial_port::flow_control(asio::serial_port::flow_control::none));
	m_port.set_option(asio::serial_port::parity(asio::serial_port::parity::none)); 
	m_port.set_option(asio::serial_port::stop_bits(asio::serial_port::stop_bits::one));
	m_parser->rate();
//	m_port.set_option(asio::serial_port::baud_rate(CONTROL_PORT_BAUDRATE));

	m_port.async_read_some(asio::buffer(m_buf), 
		bind(&SerialComm::recv_cb, this, m_buf.begin(), asio::placeholders::error, asio::placeholders::bytes_transferred));

	m_thread = thread(bind(&asio::io_service::run, ref(m_io_service)));		
}

void SerialComm::send(const uint8_t* data, size_t sz)
{
	asio::write(m_port, asio::buffer(data, sz));	
}

void SerialComm::recv_cb(uint8_t* p, const boost::system::error_code& e, std::size_t bytes_transferred)
{
	if (e) {
		log() << "Control port error " << e.message();
		return;
	}

	log() << "Control port received " << bytes_transferred << " bytes";
	log() << "[" << (int)p[0] << ", " << (bytes_transferred>1 ? (int)p[1] : 0) << ", " << 
		(bytes_transferred>2 ? (int)p[2] : 0) << "]";

	m_parser->parse(p, bytes_transferred);

	m_port.async_read_some(asio::buffer(m_buf), 
		bind(&SerialComm::recv_cb, this, m_buf.begin(), asio::placeholders::error, asio::placeholders::bytes_transferred));	
}

void SerialComm::set_rate(uint32_t rate)
{
	m_port.set_option(asio::serial_port::baud_rate(rate));
}

Control::Control(const std::string& port) :
	m_serial(port, this) // TODO : put buffer to the m_serial, to avoid copying
{
}

void Control::send(const pkt_t& pkt) // TODO : parameters like command, dst, blablabla, and fill raw data buffer in this method
{
	const size_t len = pkt.len ? pkt.len+5 : 4;
	log() << "Sending answer, len : " << len;
	m_serial.send(reinterpret_cast<const uint8_t*>(&pkt), len);
}

void Control::rate()
{
	m_serial.set_rate(CONTROL_PORT_BAUDRATE);
}

template <typename I, typename T>
static T crc(const I& b, const I& e, T crc)
{
//	log() << "CRC start " << (int)crc;
	for (I i=b; i!=e; ++i) {
//		log () << "CRC byte " << (int)*i;
		crc += *i;
	}

//	log() << "CRC value " << (int)crc;

	return crc;
}

void Control::parse(uint8_t* p, size_t sz)
{
	m_buf.insert(m_buf.end(), p, p+sz);

	while (m_buf.size() > 2) {
		const uint32_t length = m_buf[2];

		const uint32_t full_len = length ? length + 5 : 4;

		log() << "Control data length : " << length << ", so full message length : " << full_len;

		if (m_buf.size() >= full_len) {
			log() << "Packet was received, size : " << full_len << " dst/src : " << (int)m_buf[0] 
				<< " cmd/ack/req : " << (int)m_buf[1];

//			pkt_t *pkt = reinterpret_cast<pkt_t*>(&m_buf[0]);
			pkt_t pkt;
			std::copy(m_buf.begin(), m_buf.begin()+full_len, (uint8_t*)&pkt);
			pkt.crcf = m_buf[full_len-1];

			const uint8_t crch = crc(m_buf.begin(), m_buf.begin()+3, CRC_OFFSET);

			uint8_t crcf = length ? crc(m_buf.begin()+4, m_buf.begin()+full_len-1, crch) : 0;

			if (pkt.dst == 0x03) { // according to a mail from 16.08.2016
				if (crch == pkt.crch && (pkt.len == 0 || crcf == pkt.crcf)) {
					const pkt_t ans = dispatch(pkt);

					send(ans);
				} else {
					// doing nothing accorind to a mail from 13 0ct 2016
				}
			}

			m_buf.erase(m_buf.begin(), m_buf.begin() + full_len);
		} else
			break;
	}
}

Control::pkt_t Control::dispatch(const pkt_t& pkt)
{
	pkt_t ans = {0};
	ans.src = 3;
	ans.dst = pkt.src;

	switch (pkt.cmd) {
		case VISYS_VC_PING:
			ans.cmd = pkt.cmd;
			ans.req = 1;			
			break;

		case VISYS_VC_GET_TIME : {
			auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			std::tm* hms = std::localtime(&t);

			log() << "Current time: " << hms->tm_hour << ":" << hms->tm_min << ":" << hms->tm_sec;

			ans.data[0] = hms->tm_hour;
			ans.data[1] = hms->tm_min;
			ans.data[2] = hms->tm_sec;
//			ans.crch = crc((uint8_t*)&ans, ((uint8_t*)&ans) + 3, CRC_OFFSET); // TODO : set_crc(pkt_t&); in send(pkt_t)
//			ans.data[3] = crc(&ans.data[0], &ans.data[3], ans.crch);

			ans.cmd = VISYS_VC_TIME_NOTIFY;
			ans.len = 3;
		} break;

		case VISYS_VC_SET_TIME : {
			ans.cmd = pkt.cmd;
			ans.req = 1;

			if (pkt.data[0] > 23 || pkt.data[1] > 59 || pkt.data[2] > 59) {
				ans.ack = 1;
			} else {
				char time[32];
				sprintf(time, "%.2i:%.2i:%.2i", pkt.data[0], pkt.data[1], pkt.data[2]);
				::system((std::string("date --set=") + time).c_str());
				::system("hwclock --systohc");
			}
		} break;

		case VISYS_VC_GET_DATE : {
			auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			std::tm* hms = std::localtime(&t);
			
			ans.data[0] = hms->tm_year;
			ans.data[1] = hms->tm_mon;
			ans.data[2] = hms->tm_mday;
//			ans.crch = crc((uint8_t*)&ans, ((uint8_t*)&ans) + 3, CRC_OFFSET); // TODO : set_crc(pkt_t&); in send(pkt_t)
//			ans.data[3] = crc(&ans.data[0], &ans.data[3], ans.crch);
			
			ans.cmd = VISYS_VC_DATE_NOTIFY;
			ans.len = 3;
		} break;

		case VISYS_VC_SET_DATE : {
			auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			std::tm* hms = std::localtime(&t);

			ans.cmd = pkt.cmd;
			ans.req = 1;

			if (pkt.data[0] <= 70 || pkt.data[0] >= 138 || pkt.data[1] > 11 || pkt.data[2] > 31) {
				ans.ack = 1;
			} else {
				char date[32];

				sprintf(date, "\"%.4i-%.2i-%.2i %.2i:%.2i:%.2i\"", pkt.data[0]+1900, pkt.data[1]+1, pkt.data[2],
					hms->tm_hour, hms->tm_min, hms->tm_sec);
				log() << "Setting date " << date;
				::system((std::string("date --set=") + date).c_str());
				::system("hwclock --systohc");
			}
		} break;

		case VISIS_VC_GET_STATUS : { // TODO : error state
			ans.len = 2;
			ans.cmd = VISYS_VC_STATUS_NOTIFY;
			auto hw_status = utils::get_hw_status();
			ans.data[0] = hw_status.last_run.data;
			ans.data[1] = hw_status.this_run.data;
		} break;

		case VISYS_VC_GET_SW_VER : { // TODO : firmware version
			auto ver = utils::get_version_info();
			memcpy(&ans.data[0], ver.c_str(), std::min(size_t(20), ver.size()));
			ans.len = std::min(size_t(20), ver.size());
			ans.cmd = VISYS_VC_SW_VER_NOTIFY;
		} break;

		case VISYS_VC_GET_MODE : { // TODO : firmware version
			ans.data[0] = static_cast<uint8_t>(utils::get_streaming_mode());
			ans.len = 1;
			ans.cmd = VISYS_VC_MODE_NOTIFY;
		} break;

		case VISYS_VC_SET_MODE : {
			// TODO : set mode (and really obey it)
			utils::set_streaming_mode(pkt.data[0]);
			ans.cmd = pkt.cmd;
			ans.req = 1;
		} break;

		default :
			log() << "Error : unknown command code";
			ans.req = 1;
			ans.ack = 1;
			ans.cmd = pkt.cmd;
			break;
	}

	ans.crch = crc((uint8_t*)&ans, ((uint8_t*)&ans) + 3, CRC_OFFSET); // TODO : set_crc(pkt_t&); in send(pkt_t)
	if (ans.len > 0)
		ans.data[ans.len] = crc((uint8_t*)&ans.data[0], (uint8_t*)&ans.data[ans.len], ans.crch);

	return ans;
}

void Control::send_status()
{
	pkt_t ans = {0};
	ans.src = 3;
	ans.dst = 1;

	ans.len = 2;
	ans.cmd = VISYS_VC_STATUS_NOTIFY;
	auto hw_status = utils::get_hw_status();
	ans.data[0] = hw_status.last_run.data;
	ans.data[1] = hw_status.this_run.data;
	
	ans.crch = crc((uint8_t*)&ans, ((uint8_t*)&ans) + 3, CRC_OFFSET); // TODO : set_crc(pkt_t&); in send(pkt_t)
	if (ans.len > 0)
		ans.data[ans.len] = crc((uint8_t*)&ans.data[0], (uint8_t*)&ans.data[ans.len], ans.crch);	
		
	send(ans);
}
