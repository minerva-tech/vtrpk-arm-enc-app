#include "ext_headers.h"

#include "pchbarrier.h"

#include "log_to_file.h"

#include "control.h"

#include "defines.h"

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

void Control::send(const uint8_t* data, size_t sz) // TODO : parameters like command, dst, blablabla, and fill raw data buffer in this method
{
	m_serial.send(data, sz);
}

void Control::rate()
{
	m_serial.set_rate(CONTROL_PORT_BAUDRATE);
}

void Control::parse(uint8_t* p, size_t sz)
{
	std::copy(p, p+sz, m_buf.end());
	if (m_buf.size() > 2) {
		const uint32_t length = m_buf[2];

		if (m_buf.size() > length) {
			// TODO : read packet and what is necessary

			m_buf.erase(m_buf.begin(), m_buf.begin()+length);
		}
	}
}
