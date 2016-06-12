#pragma once

class SerialParser {
public:
	virtual void rate() = 0;
	virtual void parse(uint8_t* p, size_t sz) = 0;
};

class SerialComm { // TODO : use this in Flir
public:
	SerialComm(const std::string& port, SerialParser* parser);

	void send(const uint8_t* data, size_t sz);
	void set_rate(uint32_t rate);

private:
	boost::thread m_thread;
	boost::asio::io_service m_io_service;
	boost::asio::serial_port m_port;

	boost::array<uint8_t, 128> m_buf;

	SerialParser* m_parser;

	void recv_cb(uint8_t* p, const boost::system::error_code& e, std::size_t bytes_transferred);
};

class Control : public SerialParser { // TODO : it's duplicate of Flir and Comm, all serial communication code should be generalized
public:
	Control(const std::string& port);

	void send(const uint8_t* data, size_t sz); // TODO 

	virtual void rate() override;
	virtual void parse(uint8_t* p, size_t sz) override;

private:
	SerialComm m_serial; // TODO : SerialComm<Control> just because whynot
	
	std::deque<uint8_t> m_buf;
};
