#pragma once

class Flir {
public:
	Flir(const std::string& port);
	~Flir();
	void send(uint8_t data[8]);
	void recv_cb(uint8_t* p, const boost::system::error_code& e, std::size_t bytes_transferred);
	
private:
	boost::thread m_thread;
	boost::asio::io_service m_io_service;
	boost::asio::serial_port m_port;
	
	boost::array<uint8_t, 128> m_buf;
}; 
