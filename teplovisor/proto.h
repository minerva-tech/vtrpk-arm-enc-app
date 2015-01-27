#ifndef PROTO_H_
#define PROTO_H_

#include "iobserver.h"
#include "auxiliary.h"

class IServerCmds
{
public:
	virtual bool Hello(int id) = 0;
	virtual void Start() = 0;
	virtual void Stop() = 0;

	virtual std::string GetEncCfg() = 0;
	virtual std::string GetMDCfg() = 0;
	virtual std::vector<uint8_t> GetROI() = 0;
	virtual std::string GetVersionInfo() = 0;
	virtual uint16_t GetRegister(uint8_t addr) = 0;
	virtual Auxiliary::VideoSensorParameters GetVideoSensorParameters() = 0;
	virtual uint8_t GetCameraID() = 0;

	virtual void SetEncCfg(const std::string&) = 0;
	virtual void SetMDCfg(const std::string&) = 0;
	virtual void SetROI(const std::vector<uint8_t>&) = 0;
	virtual void SetVersionInfo(const std::string&) = 0;
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
		RequestVersionInfo,
		RequestRegister,
		RequestVideoSensorParameters,
		SetID
	};

	Server(IServerCmds* callbacks);
	~Server();

	static void SendCommand(Commands cmd, uint8_t arg = 0);

	static void SendEncCfg(const std::string&);
	static void SendMDCfg(const std::string&);
	static void SendROI(const std::vector<uint8_t>&);
	static void SendVersionInfo(const std::string&);
	
	static void SendID(int id);
	
private:
	struct Message;

	enum MessageTypes {
		EncConfig = 0,
		ROIConfig,
		MDConfig,
		VersionInfo,
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
	std::string m_version_info;

//	static const int msg_type_to_config_idx[NumberOfMessageTypes];
	bool m_first_packet_was_received[NumberOfMessageTypes];
};

struct Server::Message
{
	static const size_t mts;// = Comm::mss-1;

	Message(uint8_t type, bool first_pkt, uint8_t size) : header((type & 7) << 5 | (size & 0xf) << 1 | first_pkt==1) { }
	int type() const {return header >> 5;}
	int size() const {return header >> 1 & 0xf;}
	bool first_pkt() const {return header & 1;}
	bool last_pkt() const {return size() != sizeof(payload);}

	uint8_t header;
	uint8_t payload[Comm::mss-1];
};

class Client {

	class Cmds : public IServerCmds {
	public:
		Cmds() : m_hello_received(false), m_enc_cfg_received(false), m_md_cfg_received(false), m_roi_received(false) {}

		virtual bool Hello(int id) {m_hello_received = true; m_camera_id = id; return false;}
		virtual void Start() {assert(0);}
		virtual void Stop() {assert(0);}

		virtual std::string GetEncCfg() {assert(0); return std::string();}
		virtual std::string GetMDCfg() {assert(0); return std::string();}
		virtual std::vector<uint8_t> GetROI() {assert(0); return std::vector<uint8_t>();}
		virtual std::string GetVersionInfo() {assert(0); return std::string();}
		virtual uint16_t GetRegister(uint8_t addr) { assert(0); return 0; }
		virtual Auxiliary::VideoSensorParameters GetVideoSensorParameters() { assert(0); return Auxiliary::VideoSensorParameters(); }
		virtual uint8_t GetCameraID() { assert(0); return 0; }

		virtual void SetEncCfg(const std::string& cfg) {m_enc_cfg = cfg; m_enc_cfg_received = true;}
		virtual void SetMDCfg(const std::string& cfg) {m_md_cfg = cfg; m_md_cfg_received = true;}
		virtual void SetROI(const std::vector<uint8_t>& roi) {m_roi = roi; m_roi_received = true;}
		virtual void SetVersionInfo(const std::string& ver_info) {m_version_info = ver_info; m_version_info_received = true;}
		virtual void SetCameraID(uint8_t id) {}

		bool m_hello_received;
		bool m_enc_cfg_received;
		bool m_md_cfg_received;
		bool m_roi_received;
		bool m_version_info_received;
		int  m_camera_id;
		std::string m_enc_cfg;
		std::string m_md_cfg;
		std::vector<uint8_t> m_roi;
		std::string m_version_info;
	};

public:
	static const boost::chrono::seconds timeout;
	static const boost::chrono::seconds get_enc_cfg_timeout;

	static int Handshake(IObserver* observer = NULL);
	static std::string GetEncCfg(IObserver* observer = NULL);
	static std::string GetMDCfg(IObserver* observer = NULL);
	static std::vector<uint8_t> GetROI(IObserver* observer = NULL);
	static std::string GetVersionInfo(IObserver* observer = NULL);
};

#endif
