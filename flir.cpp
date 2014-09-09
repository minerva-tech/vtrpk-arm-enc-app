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

	send(0x0, NULL, 0);
	const uint8_t XP_mode[] = {0x03, 0x03};
	send(0x12, XP_mode, 2);
	const uint8_t LVDS_mode[] = {0x05, 0x00};
	send(0x12, LVDS_mode, 2);
	const uint8_t CMOS_mode[] = {0x06, 0x01};
	send(0x12, CMOS_mode, 2);

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
/*
void Flir::send(uint8_t data[8])
{
	if (data[0] == 0x6e && data[4] == 0x79) {
		log() << "Command SHUTTER_POSITION was received, and it could be an error, and this command is dangerous, so it was rejected."; 
		return;
	}

	asio::write(m_port, asio::buffer(data, 8));

	return;
}*/

void Flir::send(const uint8_t* data, size_t size)
{
	if (data[0] == 0x6e && data[4] == 0x79) {
		log() << "Command SHUTTER_POSITION was received, and it could be an error, and this command is dangerous, so it was rejected."; 
		return;
	}

	asio::write(m_port, asio::buffer(data, size));

	return;
}

void Flir::send(uint8_t cmd, const uint8_t* args, size_t arg_size)
{
    std::vector<uint8_t> p;
    p.reserve(10+arg_size);

    p.push_back(0x6e); // Process code
    p.push_back(0x00); // Status byte
    p.push_back(0x00); // Reserved
    p.push_back(cmd); // Function

    p.push_back(arg_size>>8); // byte count msb
    p.push_back(arg_size&0xff); // byte count lsb

    //boost::crc_ccitt_type crc;
    boost::crc_optimal<16, 0x1021, 0, 0, false, false>  crc;
    crc.process_bytes(&p[0], p.size());
    const uint16_t crc1 = crc.checksum();

    p.push_back(crc1>>8);
    p.push_back(crc1&0xff);

    for(int i=0; i<arg_size; i++)
        p.push_back(args[i]);

    crc.reset();
    crc.process_bytes(&p[0], p.size()); // yesyes, it's not necessary, i can process_bytes(&p[6], size-6);

    const uint16_t crc2 = crc.checksum();

    p.push_back(crc2>>8);
    p.push_back(crc2&0xff);
	
	send(&p[0], p.size());
}

void Flir::recv_cb(uint8_t* p, const system::error_code& err, std::size_t size)
{
	Auxiliary::SendCameraRegisterVal(p, size);

	m_port.async_read_some(asio::buffer(m_buf), 
		bind(&Flir::recv_cb, this, m_buf.begin(), asio::placeholders::error, asio::placeholders::bytes_transferred));
}
