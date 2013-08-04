#ifndef COMM_H
#define COMM_H

#include "boost/circular_buffer.hpp"

const int CAMERAS_N = 4;

const int LSB_MAX_VAL_PLUS_1 = 16;

const int CHUNK_SIZE = 128;

const int PREAMBLE = 0x02;

using namespace boost;

#define INTERPROCESS_SINGLETON 0

template<typename T>
struct CircBuf 
{ // Why not boost::circular_buffer? i don't know.
	CircBuf(size_t size);

	void reset();

	void push_back(T&& pkt);

	bool full() const;
	bool empty() const;
	size_t size() const;

	T* get_chunk(size_t* size) const;
	asio::mutable_buffers_1 get_chunk() const;
	T* get_free_chunk(size_t* size) const;

	void taken(size_t size);
	void filled(size_t size);

	boost::scoped_array<T> m_buf; // im not sure if unique_ptr<Pkt[]> will work fine with gcc 4.4.2
	T* m_buf_end;

	T* m_w;
	T* m_r;

	bool m_empty;
};

/*! Low layer communication class. Stuff like "send these 12 bytes over COM1" should be processed here. Let it be just
	wrapper over boost::asio.
*/
class Comm
{
struct Pkt;

friend Log::PrintLine& operator << (Log& log, const Pkt& pkt);

/*template <class T>
friend struct boost::interprocess::ipcdetail::placement_destroy; // with even minor sign of problems, it should be removed and Comm ctor/dtor be moved to public.
*/

public:
	enum Comment { //!< bit flags
		Invalid		= 0,
		Normal		= 1,
		CrcError	= 2,
		PacketLost	= 4
	};

#if INTERPROCESS_SINGLETON
	static Comm& instance();
#else
	static Comm& instance() { static Comm c; return c; }
#endif

	/*! Callback receives incoming packet. Notice that pointer to the payload will not be preserved after returning back from callback. 
		So, it must copy it somewhere. It won't be excessive copying as it points at payload in packet struct. I.e. copying inside of 
		callback is the single copying which cannot be avoided.

		todo: avoid boost::function due to copy ctors problem
	*/
	typedef boost::function<void (uint8_t camera, const uint8_t* payload, int comment)> Callback;

	static const int mss = 12; //< Maximum payload size for single packet

	void transmit(uint8_t cam, uint8_t port, size_t size, const uint8_t* p);

	void setCallback(const Callback& c, int port) { m_callback[port] = c; }

	Comm();
	~Comm();
	
//	FILE* fout;

	bool open(const std::string& port, int baud_rate, bool flow_control);
	void close();
	void transmit_and_close();

private:
//	static Comm* m_this;

#if INTERPROCESS_SINGLETON
	void get();
	void release();
	int m_owners;
#endif

	typedef array<uint8_t, CHUNK_SIZE> Buf;

	static const int MAX_QUEUE_LEN = 1;

	void transmit_pkt(uint8_t id, size_t size, const uint8_t* p);
	void transmitted(const system::error_code&, size_t);
//	void transmitted(shared_ptr<Pkt> sp, const system::error_code&, size_t);

	void recv_pkt(const Pkt* pkt);
	void recv_chunk(uint8_t* p, const boost::system::error_code& e, std::size_t bytes_transferred);

	Callback m_callback[4];

	thread m_thread;

	mutex m_transmit_lock;

	asio::io_service m_io_service;

//	asio::io_service::work m_work;

	asio::serial_port m_port;
	
	int m_in_count_lsb[CAMERAS_N];
	int m_out_count_lsb[CAMERAS_N];

	static const int out_buf_size = 1000; //!< Number of packets, not bytes
	CircBuf<Pkt> m_out_buf; // i'm not sure that it's really necessary. Simple and error-free way is to use deque<Pkt>, and call async_write for single element in a time. As we have bitrate 115200 top, async write will be called 960 times a second tops. But such simple way is not enough fun.
//	std::vector<Pkt> m_out_buf[2];
//	std::vector<Pkt>::iterator m_out_cur;
//	int m_out_ff_idx;
	bool m_sending_in_progress;

	Buf m_in_buf[2]; // it's not necessary indeed. single buffer is enough. and async_read_some() goes till current reading pointer
	int m_in_ff_idx;

	uint8_t* m_cur;
	ptrdiff_t m_remains;
};

struct Comm::Pkt {
	Pkt();
	Pkt(uint8_t addr, int size, const uint8_t* payload);
	uint8_t* buf();
	bool crc_valid() const;
	int count_lsb() const;
	int port() const;
	int camera() const;
	static const uint8_t crc_polynom = 0x97;

	uint8_t preamble;
	uint8_t payload[mss];
	uint8_t address;
	uint8_t crc;
};

template <typename T>
inline CircBuf<T>::CircBuf(size_t size) :
	m_buf(new T[size]),
	m_buf_end(&m_buf[0] + size),
	m_w(&m_buf[0]),
	m_r(&m_buf[0]),
	m_empty(true)
{ }

template <typename T>
inline void CircBuf<T>::reset()
{
	m_w = m_r = &m_buf[0];
}

template <typename T>
inline void CircBuf<T>::push_back(T&& val) 
{
	std::swap(*m_w, val); // as push_back()'s argument is declared as Pkt&, if i really passes rvalue to the method, will swap for rvalue be called? And answer is NO

	m_w++;

	m_empty = false;

	if (m_w == m_buf_end)
		m_w = &m_buf[0];
}

template <typename T>
inline T* CircBuf<T>::get_chunk(size_t* size) const
{
	if (m_r < m_w)
		*size = m_w - m_r;
	else
		*size = m_buf_end - m_r;

	*size *= sizeof(T);

	return m_r;
}

template <typename T>
inline asio::mutable_buffers_1 CircBuf<T>::get_chunk() const
{
	size_t size;

	if (m_r < m_w)
		size = m_w - m_r;
	else
		size = m_buf_end - m_r;

	return asio::buffer(m_r, size * sizeof(T));
}

template <typename T>
inline T* CircBuf<T>::get_free_chunk(size_t* size) const
{
	if (m_w < m_r)
		*size = m_r - m_w;
	else
		*size = m_buf_end - m_w;

	*size *= sizeof(T);

	return m_w;
}

template <typename T>
inline void CircBuf<T>::taken(size_t size)
{
	assert(size % sizeof(T) == 0);

	m_r += size / sizeof(T);

	assert(m_r <= m_buf_end);

	if (m_r == m_buf_end)
		m_r = &m_buf[0];

	m_empty = m_r == m_w;
}

template <typename T>
inline void CircBuf<T>::filled(size_t size)
{
	assert(size % sizeof(T) == 0);

	m_w += size / sizeof(T);

	assert(m_w <= m_buf_end);

	if (m_w == m_buf_end)
		m_w = &m_buf[0];
}

template <typename T>
inline bool CircBuf<T>::full() const
{
	return !m_empty && m_w == m_r;
}

template <typename T>
inline bool CircBuf<T>::empty() const
{
	return m_empty;
}

template <typename T>
inline size_t CircBuf<T>::size() const
{
	return m_empty ? 0 : m_r < m_w ? m_w-m_r : m_buf_end-m_r+m_w-&m_buf[0];
}

#endif // COMM_H


/*
UART (COM Port) Ã¯Ã°Ã®Ã²Ã®ÃªÃ®Ã«:
1 Ã±Ã²Ã Ã°Ã² Ã¡Ã¨Ã²
8 Ã¡Ã¨Ã² Ã¤Ã Ã­Ã­Ã»Ãµ
1 Ã±Ã²Ã®Ã¯ Ã¡Ã¨Ã².

ÃÃÃÃÃÃÃÃ Ã¤Ã«Ã¿ DEMO version 1.0

1. ÃÃ²Ã°Ã³ÃªÃ²Ã³Ã°Ã  Ã¯Ã ÃªÃ¥Ã²Ã .
    ÃÃ Ã§Ã¬Ã¥Ã° Ã¯Ã ÃªÃ¥Ã²Ã : 15 Ã¡Ã Ã©Ã²
     1 Ã¡Ã Ã©Ã² - ÃÃÃÃÃÃÃÃÃ 0x02 (stx)
    12 Ã¡Ã Ã©Ã² - ÃÃÃÃÃÃ. ÃÃ±Ã¥Ã£Ã¤Ã  12 Ã¡Ã Ã©Ã².
     1 Ã¡Ã Ã©Ã² - ÃÃÃÃÃ: Camera ID 2[7:6] Ã¡Ã¨Ã²Ã , Port ID 2[5:4] Ã¡Ã¨Ã²Ã , Packet ID 4[3:0] Ã¡Ã¨Ã²Ã 
     1 Ã¡Ã Ã©Ã² - CRC Ã± Ã¯Ã®Ã«Ã¨Ã­Ã®Ã¬Ã®Ã¬ 0x97. CRC Ã±Ã·Ã¨Ã²Ã Ã¥Ã²Ã±Ã¿ Ã¤Ã«Ã¿ Ã¢Ã±Ã¥Ãµ Ã²Ã°Ã¥Ãµ Ã¯Ã®Ã«Ã¥Ã©:
              ÃÃ°Ã¥Ã Ã¬Ã¡Ã³Ã«Ã», ÃÃ Ã­Ã­Ã»Ãµ Ã¨ ÃÃ¤Ã°Ã¥Ã±Ã .

     ÃÃ¥Ã°Ã¢Ã»Ã¬ Ã¯Ã¥Ã°Ã¥Ã¤Ã Ã¥Ã²Ã±Ã¿ Ã¯Ã°Ã¥Ã Ã¬Ã¡Ã³Ã«Ã , Ã§Ã Ã²Ã¥Ã¬ Ã¤Ã Ã­Ã­Ã»Ã¥ Ã¬Ã«Ã Ã¤Ã¸Ã¨Ã¬ Ã¡Ã Ã©Ã²Ã®Ã¬ Ã¢Ã¯Ã¥Ã°Â¸Ã¤, Ã§Ã Ã²Ã¥Ã¬ Ã Ã¤Ã°Ã¥Ã± Ã¨ Ã¯Ã®Ã±Ã«Ã¥Ã¤Ã­Ã¥Ã© - crc.
     ÃÃ®Ã«Ã¨Ã­Ã®Ã¬ CRC: 0x97 = x^8 + x^5 + x^3 + x^2 + x + 1.

     Camera ID - Ã­Ã®Ã¬Ã¥Ã° ÃªÃ Ã¬Ã¥Ã°Ã». ÃÃ«Ã¿ ÃÃÃÃ - ÃªÃ®Ã­Ã±Ã²Ã Ã­Ã²Ã .
     Port ID - Ã®Ã¯Ã°Ã¥Ã¤Ã¥Ã«Ã¿Ã¥Ã² Ã²Ã¨Ã¯ Ã¯Ã¥Ã°Ã¥Ã¤Ã Ã¢Ã Ã¥Ã¬Ã»Ãµ Ã¤Ã Ã­Ã­Ã»Ãµ:
               00 - Ã¤Ã Ã­Ã­Ã»Ã¥ ÃªÃ®Ã­Ã´Ã¨Ã£Ã³Ã°Ã Ã¶Ã¨Ã¨/Ã­Ã Ã±Ã²Ã°Ã®Ã©ÃªÃ¨
               11 - Ã±Ã«Ã³Ã¦Ã¥Ã¡Ã­Ã»Ã¥ Ã±Ã®Ã®Ã¡Ã¹Ã¥Ã­Ã¨Ã¿
               01 - Ã¢Ã¨Ã¤Ã¥Ã®
               10 - Ã Ã­Ã Ã«Ã¨Ã²Ã¨ÃªÃ 
     Packet ID - Ã¯Ã®Ã°Ã¿Ã¤ÃªÃ®Ã¢Ã»Ã© Ã­Ã®Ã¬Ã¥Ã° Ã¯Ã ÃªÃ¥Ã²Ã . ÃÃªÃ¢Ã®Ã§Ã­Ã Ã¿ Ã­Ã³Ã¬Ã¥Ã°Ã Ã¶Ã¨Ã¿ Ã¯Ã ÃªÃ¥Ã²Ã®Ã¢
                 Ã®Ã²/Ã¢ Ã®Ã¤Ã­Ã®Ã© Ã¨ Ã²Ã®Ã© Ã¦Ã¥ ÃªÃ Ã¬Ã¥Ã°Ã» Ã¤Ã«Ã¿ Ã¢Ã±Ã¥Ãµ Ã²Ã¨Ã¯Ã®Ã¢ Ã¤Ã Ã­Ã­Ã»Ãµ.
                 ÃÃ¥Ã®Ã¡ÃµÃ®Ã¤Ã¨Ã¬ Ã¤Ã«Ã¿ Ã®Ã¡Ã­Ã Ã°Ã³Ã¦Ã¥Ã­Ã¨Ã¿ Ã¯Ã®Ã²Ã¥Ã°Ã¨ Ã¯Ã ÃªÃ¥Ã²Ã .
*/
