#ifndef PROTO_H_
#define PROTO_H_

class IServerCmds
{
public:
	virtual bool Hello(int id) = 0;
	virtual void Start() = 0;
	virtual void Stop() = 0;

	virtual std::string GetEncCfg() = 0;
	virtual std::string GetMDCfg() = 0;
	virtual std::vector<uint8_t> GetROI() = 0;
	virtual uint16_t GetRegister(uint8_t addr) = 0;
	virtual uint8_t GetCameraID() = 0;

	virtual void SetEncCfg(const std::string&) = 0;
	virtual void SetMDCfg(const std::string&) = 0;
	virtual void SetROI(const std::vector<uint8_t>&) = 0;
	virtual void SetCameraID(uint8_t id) = 0;
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
		RequestRegister,
		SetID
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

		virtual bool Hello(int id) {m_hello_received = true; return false;}
		virtual void Start() {assert(0);}
		virtual void Stop() {assert(0);}

		virtual std::string GetEncCfg() {assert(0); return std::string();}
		virtual std::string GetMDCfg() {assert(0); return std::string();}
		virtual std::vector<uint8_t> GetROI() {assert(0); return std::vector<uint8_t>();}
		virtual uint16_t GetRegister(uint8_t addr) { assert(0); return 0; }
		virtual uint8_t GetCameraID() { assert(0); return 0; }

		virtual void SetEncCfg(const std::string& cfg) {m_enc_cfg = cfg; m_enc_cfg_received = true;}
		virtual void SetMDCfg(const std::string& cfg) {m_md_cfg = cfg; m_md_cfg_received = true;}
		virtual void SetROI(const std::vector<uint8_t>& roi) {m_roi = roi; m_roi_received = true;}
		virtual void SetCameraID(uint8_t id) {}

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
		RegisterValType,
		CameraRegisterValType
	};

	template <typename T>
	struct Pkt {
		uint32_t	type;
		T			data;
	};

	struct TimestampData {
		uint32_t sec;   // ok, it's actually crap, but i know that timestamps are long, and long on target platform (ARM v5t) with my compiler (GCC 4.7.2) is 32-bit.
		uint32_t usec;  // so, use it, instead of "long int" which could be 64 bit on other platforms, where this code will be used for data receiving.
	};
	
	struct RegisterValData {
		uint32_t addr;
		uint32_t val;
	};

	struct CameraRegisterValData {
		uint8_t val[8];
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

	inline void SendCameraRegisterVal(uint8_t* p, size_t size) {
		while (size>0) {
			Pkt<CameraRegisterValData> pkt;
			pkt.type = CameraRegisterValType;

			const size_t to_send = std::min(size, sizeof(pkt.data.val));
			memcpy(pkt.data.val, p, to_send);

			Comm::instance().transmit(0, Port, sizeof(pkt), (uint8_t*)&pkt);

			size -= to_send;
		}
	}

	inline AuxiliaryType Type(const uint8_t* data) {
		int type;
		const uint8_t* p = (uint8_t*)&((Pkt<uint32_t>*)data)->type;
		memcpy(&type, p, sizeof(type)); // unaligned
//		memcpy(&type, (uint8_t*)&((Pkt<uint32_t>*)data)->type, sizeof(type)); // doesn't work with gcc 4.7.2
		return (AuxiliaryType)type;
	}

	inline TimestampData Timestamp(const uint8_t* buf) {
		assert(Type(buf) == TimestampType);
		TimestampData data;
		const uint8_t* p = (uint8_t*)&((Pkt<TimestampData>*)buf)->data;
		memcpy(&data, p, sizeof(data)); // unaligned
		return data;
	}

	inline RegisterValData RegisterVal(const uint8_t* buf) {
		assert(Type(buf) == RegisterValType);
		RegisterValData data;
		const uint8_t* p = (uint8_t*)&((Pkt<RegisterValData>*)buf)->data;
		memcpy(&data, p, sizeof(data)); // unaligned
		return data;
	}
	
	inline CameraRegisterValData CameraRegisterVal(const uint8_t* buf) {
		assert(Type(buf) == CameraRegisterValType);
		CameraRegisterValData data;
		const uint8_t* p = (uint8_t*)&((Pkt<CameraRegisterValData>*)buf)->data;
		memcpy(&data, p, sizeof(data)); // unaligned
		return data;
	}
};

#endif
