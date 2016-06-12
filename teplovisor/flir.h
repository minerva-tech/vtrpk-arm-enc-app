#pragma once

class Flir {
public:
	Flir(const std::string& port);
	~Flir();
//	void send(uint8_t data[8]);
	void send(const uint8_t* data, size_t size);
	void send(uint8_t cmd, const uint8_t* args, size_t arg_size);
	void recv_cb(uint8_t* p, const boost::system::error_code& e, std::size_t bytes_transferred);

	void get_serials(uint32_t data[4]);

private:
	uint32_t detect_baudrate(bool boot=false);
	void wait_for_answer(uint32_t timeout=100) const;

	boost::thread m_thread;
	boost::asio::io_service m_io_service;
	boost::asio::serial_port m_port;

	boost::array<uint8_t, 128> m_buf;

	uint32_t m_serials[2];
	uint32_t m_versions[2];

	bool m_answered;
}; 
