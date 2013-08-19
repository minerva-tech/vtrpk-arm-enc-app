#ifndef PROTO_H_
#define PROTO_H_

class IServerCmds
{
public:
	virtual bool Hello() = 0;
	virtual void Start() = 0;
	virtual void Stop() = 0;

	virtual std::string GetEncCfg() = 0;
	virtual std::string GetMDCfg() = 0;
	virtual std::vector<uint8_t> GetROI() = 0;
	virtual uint16_t GetRegister(uint8_t addr) = 0;

	virtual void SetEncCfg(const std::string&) = 0;
	virtual void SetMDCfg(const std::string&) = 0;
	virtual void SetROI(const std::vector<uint8_t>&) = 0;
};

class Server
{
public:
	enum Commands {
		InvalidCmd = 0,
		Hello,
		Start,
		Stop,
		RequestEncConfig,
		RequestMDConfig,
		RequestROI,
		RequestRegister
	};

	Server(IServerCmds* callbacks);
	~Server();

	static void SendCommand(Commands cmd, uint8_t arg = 0);

	static void SendEncCfg(const std::string&);
	static void SendMDCfg(const std::string&);
	static void SendROI(const std::vector<uint8_t>&);

private:
	struct Message;

	enum MessageTypes {
		EncConfig = 0,
		ROIConfig,
		MDConfig,
		Command,
		NumberOfMessageTypes
	};

	static_assert(NumberOfMessageTypes <= 7, "There are only three bits to code message type");

	void Callback(uint8_t camera, const uint8_t* payload, int comment);

	void execute(uint8_t command, uint8_t arg = 0);

//	void getEncCfgChunk(const Message* msg);
	template<typename CfgContainer, typename Cb>
	void getEncCfgChunk(const Message* msg, CfgContainer& cfg, const Cb& cb);

	template <typename CfgContainer>
	static void SendCfg(const CfgContainer& cfg, MessageTypes msg_type);

	IServerCmds* m_callbacks;

//	std::string m_config[3];
	std::string m_enc_cfg;
	std::string m_md_cfg;
	std::vector<uint8_t> m_roi;

	static const int msg_type_to_config_idx[NumberOfMessageTypes];
};

struct Server::Message
{
	static const size_t mts = Comm::mss-1;

	Message(uint8_t type, bool last_pkt, uint8_t size) : header((type & 7) << 5 | (size & 0xf) << 1 | last_pkt==1) { }
	int type() const {return header >> 5;}
	int size() const {return header >> 1 & 0xf;}
	bool last_pkt() const {return header & 1;}

	uint8_t header;
	uint8_t payload[mts];
};

class Client {
	static const boost::chrono::seconds timeout;

	class Cmds : public IServerCmds {
	public:
		Cmds() : m_hello_received(false), m_enc_cfg_received(false), m_md_cfg_received(false), m_roi_received(false) {}

		virtual bool Hello() {m_hello_received = true; return false;}
		virtual void Start() {assert(0);}
		virtual void Stop() {assert(0);}

		virtual std::string GetEncCfg() {assert(0); return std::string();}
		virtual std::string GetMDCfg() {assert(0); return std::string();}
		virtual std::vector<uint8_t> GetROI() {assert(0); return std::vector<uint8_t>();}
		virtual uint16_t GetRegister(uint8_t addr) { assert(0); return 0; }

		virtual void SetEncCfg(const std::string& cfg) {m_enc_cfg = cfg; m_enc_cfg_received = true;}
		virtual void SetMDCfg(const std::string& cfg) {m_md_cfg = cfg; m_md_cfg_received = true;}
		virtual void SetROI(const std::vector<uint8_t>& roi) {m_roi = roi; m_roi_received = true;}

		bool m_hello_received;
		bool m_enc_cfg_received;
		bool m_md_cfg_received;
		bool m_roi_received;
		std::string m_enc_cfg;
		std::string m_md_cfg;
		std::vector<uint8_t> m_roi;
	};

public:
	static bool Handshake();
	static std::string GetEncCfg();
	static std::string GetMDCfg();
	static std::vector<uint8_t> GetROI();
};

namespace Auxiliary {
	static const int Port = 3;

	enum AuxiliaryType {
		InvalidType = 0,
		TimestampType,
		RegisterValType
	};

	template <typename T>
	struct Pkt {
		AuxiliaryType 	type;
		T				data;
	};

	struct TimestampData {
		uint32_t sec;   // ok, it's actually crap, but i know that timestamps are long, and long on target platform (ARM v5t) with my compiler (GCC 4.7.2) is 32-bit.
		uint32_t usec;  // so, use it, instead of "long int" which could be 64 bit on other platforms, where this code will be used for data receiving.
	};
	
	struct RegisterValData {
		uint8_t addr;
		uint16_t val;
	};

	static_assert(sizeof(Pkt<TimestampData>) <= Comm::mss, "Size of single auxiliary data packet shouldnt exceed Comm::mss");
	static_assert(sizeof(Pkt<RegisterValData>) <= Comm::mss, "Size of single auxiliary data packet shouldnt exceed Comm::mss");

	inline void SendTimestamp(long sec, long usec) {
		Pkt<TimestampData> pkt;
		pkt.type = TimestampType;
		pkt.data.sec  = sec;
		pkt.data.usec = usec;
		Comm::instance().transmit(0, Port, sizeof(pkt), (uint8_t*)&pkt);
	}

	inline void SendRegisterVal(uint8_t addr, uint16_t val) {
		Pkt<RegisterValData> pkt;
		pkt.type = RegisterValType;
		pkt.data.addr = addr;
		pkt.data.val = val;
		Comm::instance().transmit(0, Port, sizeof(pkt), (uint8_t*)&pkt);
	}

	inline AuxiliaryType Type(const uint8_t* data) {
		return ((Pkt<char>*)data)->type;
	}

	inline TimestampData Timestamp(const uint8_t* data) {
		assert(Type(data) == TimestampType);
		const Pkt<TimestampData>* pkt = (const Pkt<TimestampData>*)data;
		return pkt->data;
	}

	inline RegisterValData RegisterVal(const uint8_t* data) {
		assert(Type(data) == RegisterValType);
		const Pkt<RegisterValData>* pkt = (const Pkt<RegisterValData>*)data;
		return pkt->data;
	}
};

#endif
