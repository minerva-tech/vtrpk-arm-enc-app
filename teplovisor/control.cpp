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
	m_serial.send(reinterpret_cast<const uint8_t*>(&pkt), pkt.len ? pkt.len+5 : 4);
}

void Control::rate()
{
	m_serial.set_rate(CONTROL_PORT_BAUDRATE);
}

template <typename T>
static T crc(T* b, T* e, T crc)
{
	for (T* i=b; i!=e; ++i)
		crc += *i;

	return crc;
}

void Control::parse(uint8_t* p, size_t sz)
{
	m_buf.insert(m_buf.end(), p, p+sz);

	while (m_buf.size() > 2) {
		const uint32_t length = m_buf[2];
		
		const uint32_t full_len = length ? length + 5 : 4;

		log() << "Control data length : " <<  length << ", so full message length : " << full_len;

		if (m_buf.size() >= full_len) {
			log() << "Packet was received, size : " << full_len << " dst/src : " << m_buf[0] << " cmd/ack/req : " << m_buf[1];

			pkt_t *pkt = reinterpret_cast<pkt_t*>(&m_buf[0]);
			pkt->crcf = m_buf[full_len-1];

			const uint8_t crch = crc(&m_buf[0], &m_buf[3], CRC_OFFSET);

			const uint8_t crcf = crc(&m_buf[4], &m_buf[full_len-1], crch);

			if (pkt->dst == 0x03) { // according to mail from 16.08.2016
				const pkt_t ans = dispatch(pkt);

				send(ans);
			}

			m_buf.erase(m_buf.begin(), m_buf.begin() + full_len);
		} else
			break;
	}
}

Control::pkt_t Control::dispatch(const pkt_t* pkt)
{
	pkt_t ans = {0};
	ans.src = 3;
	ans.dst = pkt->src;

	switch (pkt->cmd) {
		case VISYS_VC_GET_TIME : {
			auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			std::tm* hms = std::localtime(&t);

			ans.data[0] = hms->tm_hour;
			ans.data[1] = hms->tm_min;
			ans.data[2] = hms->tm_sec;
			ans.crch = crc((uint8_t*)&ans, ((uint8_t*)&ans) + 3, CRC_OFFSET); // TODO : set_crc(pkt_t&); in send(pkt_t)
			ans.data[3] = crc(&ans.data[0], &ans.data[3], ans.crch);

			ans.cmd = VISYS_VC_TIME_NOTIFY;
			ans.len = 3;
		} break;

		case VISYS_VC_SET_TIME : {
			// TODO : set time to RT clock.
			char time[9];
			sprintf(time, "%.2i:%.2i:%.2i", pkt->data[0], pkt->data[1], pkt->data[2]);
			::system((std::string("date --set=") + time).c_str());
			::system("hwclock --systohc");
			
			ans.cmd = pkt->cmd;
			ans.req = 1;
		} break;

		case VISYS_VC_GET_DATE : {
			auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			std::tm* hms = std::localtime(&t);
			
			ans.data[0] = hms->tm_year;
			ans.data[1] = hms->tm_mon;
			ans.data[2] = hms->tm_mday;
			ans.data[3] = crc(&ans.data[0], &ans.data[3], ans.crch);
			
			ans.cmd = VISYS_VC_DATE_NOTIFY;
			ans.len = 3;
		} break;

		case VISYS_VC_SET_DATE : {
			// TODO : set date to RT clock.
			char date[9];
			sprintf(date, "%.2i%.2i%.4i", pkt->data[1], pkt->data[2], pkt->data[0]);
			::system((std::string("date ") + date).c_str());
			::system("hwclock --systohc");

			ans.cmd = pkt->cmd;
			ans.req = 1;
		} break;

		case VISIS_VC_GET_STATUS : { // TODO : error state
			ans.len = 2;
			ans.cmd = VISYS_VC_STATUS_NOTIFY;
		} break;

		case VISYS_VC_GET_SW_VER : { // TODO : firmware version
			auto ver = utils::get_version_info();
			memcpy(&ans.data[0], ver.c_str(), std::min(size_t(26), ver.size()));
			ans.len = 26;
			ans.cmd = VISYS_VC_SW_VER_NOTIFY;
		} break;

		case VISYS_VC_GET_MODE : { // TODO : firmware version
			ans.data[0] = utils::get_streaming_mode();
			ans.len = 1;
			ans.cmd = VISYS_VC_MODE_NOTIFY;
		} break;

		case VISYS_VC_SET_MODE : {
			// TODO : set mode (and really obey it)
			utils::set_streaming_mode(ans.data[0]);
			ans.cmd = pkt->cmd;
			ans.req = 1;
		} break;

		default :
			log() << "Error : unknown command code";
			ans.req = 1;
			ans.ack = 1;
			break;
	}

	ans.crch = crc((uint8_t*)&ans, ((uint8_t*)&ans) + 3, CRC_OFFSET); // TODO : set_crc(pkt_t&); in send(pkt_t)
	if (ans.len > 0)
		ans.crcf = crc((uint8_t*)&ans.data[0], (uint8_t*)&ans.data[ans.len], ans.crch);

	return ans;
}
