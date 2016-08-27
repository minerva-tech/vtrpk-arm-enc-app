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
	static const int MAX_DATA_SIZE = 26;

	struct pkt_t {
		uint8_t dst : 4;
		uint8_t src : 4;

		uint8_t cmd : 5;
		uint8_t ack : 2;
		uint8_t req : 1;

		uint8_t len;

		uint8_t crch;

		uint8_t data[MAX_DATA_SIZE];

		uint8_t crcf;
	};

	enum {
		VISYS_VC_GET_TIME = 0x09,
		VISYS_VC_TIME_NOTIFY = 0x10,
		VISYS_VC_SET_TIME = 0x11,
		VISYS_VC_GET_DATE = 0x12,
		VISYS_VC_DATE_NOTIFY = 0x13,
		VISYS_VC_SET_DATE = 0x14,
		VISIS_VC_GET_STATUS = 0x15,
		VISYS_VC_STATUS_NOTIFY = 0x16,
		VISYS_VC_GET_SW_VER = 0x17,
		VISYS_VC_SW_VER_NOTIFY = 0x18,
		VISYS_VC_GET_MODE = 0x19,
		VISYS_VC_MODE_NOTIFY = 0x1a,
		VISYS_VC_SET_MODE = 0x1b
	};

public:
	Control(const std::string& port);

	void send(const pkt_t&); // TODO 

	virtual void rate() override;
	virtual void parse(uint8_t* p, size_t sz) override;
	
private:
	SerialComm m_serial; // TODO : SerialComm<Control> just because whynot
	
	std::deque<uint8_t> m_buf;

	pkt_t dispatch(const pkt_t* pkt);
};
