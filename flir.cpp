#include "ext_headers.h"

#include "pchbarrier.h"

#include "flir.h"
#include "log_to_file.h"

#include "comm.h"
#include "proto.h"

using namespace boost;

Flir::Flir(const std::string& port) :
	m_port(m_io_service)
{
	boost::system::error_code ec;

	m_port.open(port, ec);

	if (ec) {
		log() << "FLIR port opening error : " << ec.message();
		return;
	}

	m_port.set_option(asio::serial_port::baud_rate(57600)); 
	m_port.set_option(asio::serial_port::character_size(8));
	m_port.set_option(asio::serial_port::flow_control(asio::serial_port::flow_control::none));
	m_port.set_option(asio::serial_port::parity(asio::serial_port::parity::none)); 
	m_port.set_option(asio::serial_port::stop_bits(asio::serial_port::stop_bits::one));

	m_port.async_read_some(asio::buffer(m_buf), 
		bind(&Flir::recv_cb, this, m_buf.begin(), asio::placeholders::error, asio::placeholders::bytes_transferred));

	m_thread = thread(bind(&asio::io_service::run, ref(m_io_service)));
}

Flir::~Flir()
{
	boost::system::error_code ec;

	if (!m_port.is_open())
		return;

	m_port.close(ec);

	if (ec)
		log() << "FLIR port closing error : " << ec.message();

	m_io_service.stop();

	m_thread.join();

	m_io_service.reset();	
}

void Flir::send(uint8_t data[8])
{
	if (data[0] == 0x6e && data[4] == 0x79) {
		log() << "Command SHUTTER_POSITION was received, and it could be an error, and this command is dangerous, so it was rejected."; 
		return;
	}

	asio::write(m_port, asio::buffer(data, 8));

	return;
}

void Flir::recv_cb(uint8_t* p, const system::error_code& err, std::size_t size)
{
	Auxiliary::SendCameraRegisterVal(p, size);

	m_port.async_read_some(asio::buffer(m_buf), 
		bind(&Flir::recv_cb, this, m_buf.begin(), asio::placeholders::error, asio::placeholders::bytes_transferred));
}
