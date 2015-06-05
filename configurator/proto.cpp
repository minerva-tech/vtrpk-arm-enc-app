#include "ext_headers.h"

#include "pchbarrier.h"

//#include "log_to_file.h"
#include "comm.h"
#include "proto.h"

const chrono::seconds Client::timeout(5);
const chrono::seconds Client::get_enc_cfg_timeout(30);

//const int Server::msg_type_to_config_idx[Server::NumberOfMessageTypes] = {0, 1, 2, -1};
const size_t Server::Message::mts = Comm::mss-1;

Server::Server(IServerCmds* callbacks) : 
	m_callbacks(callbacks) 
{
    memset(m_first_packet_was_received, 0, sizeof(m_first_packet_was_received));
	Comm::instance().setCallback(boost::bind(&Server::Callback, this, _1, _2, _3), 0);
}

Server::~Server()
{
	Comm::instance().setCallback(NULL, 0);
}

void Server::Callback(uint8_t camera, const uint8_t* payload, int comment)
{
	const Message* msg = reinterpret_cast<const Message*>(payload);

//	log() << "camera in packet: " << (int)camera << " my camera id: " << Comm::instance().cameraID();

	if (camera!=Comm::instance().cameraID() && (msg->type()!=Command || msg->payload[0]!=Hello)) { // TODO: All connected cameras will answer to Hello command. May be it's wrong behavior. 
		log() << "Command wasn't accepted (camera in packet: " << (int)camera << " my camera id: " << (int)Comm::instance().cameraID();
		return;
	}

	switch (msg->type()) {
	case Command:
        execute(msg->payload[0], msg->payload[1], msg->payload[2]);
		break;
	case EncConfig:
		//getEncCfgChunk(msg);
		getEncCfgChunk(msg, m_enc_cfg, boost::bind(&IServerCmds::SetEncCfg, m_callbacks, _1));
		break;
	case ROIConfig:
		getEncCfgChunk(msg, m_roi, boost::bind(&IServerCmds::SetROI, m_callbacks, _1));
		break;
	case MDConfig:
		getEncCfgChunk(msg, m_md_cfg, boost::bind(&IServerCmds::SetMDCfg, m_callbacks, _1));
		break;
	case VersionInfo:
		getEncCfgChunk(msg, m_version_info, boost::bind(&IServerCmds::SetVersionInfo, m_callbacks, _1));
		break;
    }

	return;
}

void Server::execute(uint8_t command, uint8_t arg0, uint8_t arg1)
{
	log() << "command was received : " << (int)command;
	
	switch (command) {
	case Hello:
        log() << "Hello command received : " << (int)arg0;
        if (m_callbacks->Hello(arg0))
            boost::thread(boost::bind(&Server::SendCommand, Hello, Comm::instance().cameraID(), 0));
		break;
	case Start:
		m_callbacks->Start();
		break;
	case Stop:
		m_callbacks->Stop();
		break;
	case RequestEncConfig:
		boost::thread(boost::bind(&Server::SendEncCfg, m_callbacks->GetEncCfg())); // possible deadlock, SendEncCfg should start separate thread for sending.
		break;
	case RequestMDConfig:
		boost::thread(boost::bind(&Server::SendMDCfg, m_callbacks->GetMDCfg()));
		break;
	case RequestROI:
		boost::thread(boost::bind(&Server::SendROI, m_callbacks->GetROI()));
		break;
	case RequestVersionInfo:
		boost::thread(boost::bind(&Server::SendVersionInfo, m_callbacks->GetVersionInfo()));
		break;
	case RequestRegister:
        boost::thread(boost::bind(&Auxiliary::SendRegisterVal, arg0, m_callbacks->GetRegister(arg0)));
		break;
	case SetID:
        m_callbacks->SetCameraID(arg0);
		break;
    case ToggleStreams:
        log() << "Set streams enable flag command : " << (int)arg0;
        m_callbacks->SetStreamsEnableFlag(arg0);
        break;
    case RequestVSensorConfig:
        boost::thread(boost::bind(&Server::SendVSensorConfig, m_callbacks->GetVSensorConfig()));
        break;
    case BufferClear:
        m_callbacks->BufferClear();
        break;
    case SetBitrate:
        m_callbacks->SetBitrate(arg0 | (arg1 << 8));
        break;
	default:
		log() << "Invalid command";
	};
}

template<typename CfgContainer, typename Cb>
void Server::getEncCfgChunk(const Server::Message* msg, CfgContainer& cfg, const Cb& cb)
{
//	const int cfg_idx = msg_type_to_config_idx[msg->type()];
	
//	assert(cfg_idx >= 0);
	
//	std::string& str = m_config[cfg_idx];

//	std::copy(&msg->payload[0], &msg->payload[msg->size()], std::inserter(str, str.end()));
	if (msg->first_pkt()) {
		cfg.clear();
		m_first_packet_was_received[msg->type()] = true;
	}

	std::copy(&msg->payload[0], &msg->payload[msg->size()], std::inserter(cfg, cfg.end()));

	if (msg->last_pkt() && m_first_packet_was_received[msg->type()]) {
//		m_callbacks->SetEncCfg(str);
		cb(cfg);
//		str.clear();
		cfg.clear();
		m_first_packet_was_received[msg->type()] = false;
	}

	return;
}

template <typename CfgContainer>
void Server::SendCfg(const CfgContainer& cfg, MessageTypes msg_type) 
{
	static_assert(sizeof(cfg[0]) == 1, "CfgContainer should contain chars");
	
	std::vector<Server::Message> msgs;

	typename CfgContainer::const_iterator cfg_it = cfg.begin();

	bool first_pkt = true;

	bool empty_last_pkt = false;

	while(cfg_it != cfg.end()) {
		const size_t data_left = cfg.end() - cfg_it;
//		const bool last_pkt = data_left <= Server::Message::mts;

		empty_last_pkt = data_left == Server::Message::mts; // there will be one more packet with zero size to mark end of cfg string.

		const size_t to_send = std::min(data_left, Server::Message::mts);
		msgs.push_back(Server::Message(msg_type, first_pkt, to_send));

		first_pkt = false;

		std::copy(cfg_it, cfg_it+to_send, &msgs.back().payload[0]);

		cfg_it += to_send;
	}
	
	if (empty_last_pkt)
		msgs.push_back(Server::Message(msg_type, false, 0));

	if (msgs.size())
		Comm::instance().transmit(0, msgs.size()*sizeof(msgs[0]), reinterpret_cast<const uint8_t*>(&msgs[0]));

	return;
}

void Server::SendEncCfg(const std::string& cfg) 
{
	SendCfg(cfg, EncConfig);
}

void Server::SendMDCfg(const std::string& str)
{
	SendCfg(str, MDConfig);
}

void Server::SendROI(const std::vector<uint8_t>& roi) 
{
	SendCfg(roi, ROIConfig);
}

void Server::SendVersionInfo(const std::string& version_info)
{
    SendCfg(version_info, VersionInfo);
}

void Server::SendVSensorConfig(const std::string& vsensset)
{
    SendCfg(vsensset, VSensSettings);
}


void Server::SendID(int id)
{
    SendCommand(SetID, id);
}

void Server::SendCommand(Commands cmd, uint8_t arg0, uint8_t arg1)
{
	Server::Message msg(Command, true, 1);
	msg.payload[0] = cmd;
    msg.payload[1] = arg0;
    msg.payload[2] = arg1;
	Comm::instance().transmit(0, sizeof(msg), reinterpret_cast<const uint8_t*>(&msg));
}

int Client::Handshake(IObserver* observer, bool* motion_enable)
{
    Cmds cmds;
    Server server(&cmds);

    server.SendCommand(Server::Hello);

    chrono::steady_clock::time_point start = chrono::steady_clock::now();
    while(!cmds.m_hello_received) {
//    while(cmds.m_streams_enable == -1) { // CAUTION! when we will enable "motion detector" checkbox, this part of a code should be changed. Problem is after hello is received, camera number still isn't set. So, togglestreams command is rejected because it contains wrong camera number, and we ignore cam id in hello packet only
        if (chrono::steady_clock::now() - start > timeout)
            return -1;
        if (observer)
            observer->progress();
        boost::this_thread::sleep(boost::posix_time::milliseconds(10));
    }

    if (motion_enable)
        *motion_enable = cmds.m_streams_enable & MOTION_ENABLE_BIT;

    return cmds.m_camera_id;
}

std::string Client::GetEncCfg(IObserver* observer)
{
    Cmds cmds;
    Server server(&cmds);

    server.SendCommand(Server::RequestEncConfig);

    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    while(!cmds.m_enc_cfg_received && chrono::steady_clock::now() - start < get_enc_cfg_timeout) {
        if (observer)
            observer->progress();
        boost::this_thread::sleep(boost::posix_time::milliseconds(10));
    }

    cmds.m_enc_cfg_received = false;

    return cmds.m_enc_cfg;
}

std::string Client::GetMDCfg(IObserver* observer)
{
    Cmds cmds;
    Server server(&cmds);

    server.SendCommand(Server::RequestMDConfig);

    chrono::steady_clock::time_point start = chrono::steady_clock::now();
    while(!cmds.m_md_cfg_received && chrono::steady_clock::now() - start < timeout) {
        if (observer)
            observer->progress();
        boost::this_thread::sleep(boost::posix_time::milliseconds(10));
    }

    cmds.m_md_cfg_received = false;

    return cmds.m_md_cfg;
}

std::vector<uint8_t> Client::GetROI(IObserver* observer)
{
    Cmds cmds;
    Server server(&cmds);

    server.SendCommand(Server::RequestROI);

    chrono::steady_clock::time_point start = chrono::steady_clock::now();
    while(!cmds.m_roi_received && chrono::steady_clock::now() - start < timeout) {
        if (observer)
            observer->progress();
        boost::this_thread::sleep(boost::posix_time::milliseconds(10));
    }

    cmds.m_roi_received = false;

    return cmds.m_roi;
}

std::string Client::GetVersionInfo(IObserver* observer)
{
    Cmds cmds;
    Server server(&cmds);

    server.SendCommand(Server::RequestVersionInfo);

    chrono::steady_clock::time_point start = chrono::steady_clock::now();
    while(!cmds.m_version_info_received && chrono::steady_clock::now() - start < timeout) {
        if (observer)
            observer->progress();
        boost::this_thread::sleep(boost::posix_time::milliseconds(10));
    }

    cmds.m_version_info_received = false;

    return cmds.m_version_info;
}

std::string Client::GetVSensorConfig(IObserver* observer)
{
    Cmds cmds;
    Server server(&cmds);

    server.SendCommand(Server::RequestVSensorConfig);

    chrono::steady_clock::time_point start = chrono::steady_clock::now();
    while(!cmds.m_vsensor_settings_received && chrono::steady_clock::now() - start < timeout) {
        if (observer)
            observer->progress();
        boost::this_thread::sleep(boost::posix_time::milliseconds(10));
    }

    cmds.m_vsensor_settings_received = false;

    return cmds.m_vsensor_settings;
}
