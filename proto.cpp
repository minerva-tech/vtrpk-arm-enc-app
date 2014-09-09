#include "ext_headers.h"

#include "pchbarrier.h"

#include "log_to_file.h"
#include "comm.h"
#include "proto.h"

const chrono::seconds Client::timeout(5);

const int Server::msg_type_to_config_idx[Server::NumberOfMessageTypes] = {0, 1, 2, -1};
const size_t Server::Message::mts;

Server::Server(IServerCmds* callbacks) : 
	m_callbacks(callbacks) 
{
	Comm::instance().setCallback(boost::bind(&Server::Callback, this, _1, _2, _3), 0);
}

Server::~Server()
{
	Comm::instance().setCallback(NULL, 0);
}

void Server::Callback(uint8_t camera, const uint8_t* payload, int comment)
{
	const Message* msg = reinterpret_cast<const Message*>(payload);

	log() << "camera in packet: " << (int)camera << " my camera id: " << Comm::instance().cameraID();

	if (camera!=Comm::instance().cameraID() && (msg->type()!=Command || msg->payload[0]!=Hello)) { // TODO: All connected cameras will answer to Hello command. May be it's wrong behavior. 
		log() << "Command wasn't accepted (camera in packet: " << (int)camera << " my camera id: " << Comm::instance().cameraID();
		return;
	}

	switch (msg->type()) {
	case Command:
		execute(msg->payload[0], msg->payload[1]);
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
	}

	return;
}

void Server::execute(uint8_t command, uint8_t arg)
{
	log() << "command received";
	
	switch (command) {
	case Hello:
		if (m_callbacks->Hello(arg))
			boost::thread(boost::bind(&Server::SendCommand, Hello, Comm::instance().cameraID()));
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
	case RequestRegister:
		boost::thread(boost::bind(&Auxiliary::SendRegisterVal, arg, m_callbacks->GetRegister(arg)));
		break;
	case SetID:
		m_callbacks->SetCameraID(arg);
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
	std::copy(&msg->payload[0], &msg->payload[msg->size()], std::inserter(cfg, cfg.end()));

	if (msg->last_pkt()) {
//		m_callbacks->SetEncCfg(str);
		cb(cfg);
//		str.clear();
		cfg.clear();
	}

	return;
}

template <typename CfgContainer>
void Server::SendCfg(const CfgContainer& cfg, MessageTypes msg_type) 
{
	static_assert(sizeof(cfg[0]) == 1, "CfgContainer should contain chars");
	
	std::vector<Server::Message> msgs;

	typename CfgContainer::const_iterator cfg_it = cfg.begin();

	while(cfg_it != cfg.end()) {
		const size_t data_left = cfg.end() - cfg_it;
		const bool last_pkt = data_left <= Server::Message::mts;
		const size_t to_send = std::min(data_left, Server::Message::mts);
		msgs.push_back(Server::Message(msg_type, last_pkt, to_send));

		std::copy(cfg_it, cfg_it+to_send, &msgs.back().payload[0]);

		cfg_it += to_send;
	}

	if (msgs.size())
		Comm::instance().transmit(0, msgs.size()*sizeof(msgs[0]), reinterpret_cast<const uint8_t*>(&msgs[0]));

	return;
}

void Server::SendEncCfg(const std::string& cfg) 
{
	SendCfg(cfg, EncConfig);
	
/*	std::vector<Server::Message> msgs;

	std::string::const_iterator cfg_it = cfg.begin();

	while(cfg_it != cfg.end()) {
		const size_t data_left = cfg.end() - cfg_it;
		const bool last_pkt = data_left <= Server::Message::mts;
		const size_t to_send = std::min(data_left, Server::Message::mts);
		msgs.push_back(Server::Message(EncConfig, last_pkt, to_send));

		std::copy(cfg_it, cfg_it+to_send, &msgs.back().payload[0]);

		cfg_it += to_send;
	}

	if (msgs.size())
		Comm::instance().transmit(0, msgs.size()*sizeof(msgs[0]), reinterpret_cast<const uint8_t*>(&msgs[0]));

	return;*/
}

void Server::SendMDCfg(const std::string& str)
{
	SendCfg(str, MDConfig);
}

void Server::SendROI(const std::vector<uint8_t>& roi) 
{
	SendCfg(roi, ROIConfig);
}

void Server::SendCommand(Commands cmd, uint8_t arg)
{
	Server::Message msg(Command, true, 1);
	msg.payload[0] = cmd;
	msg.payload[1] = arg;
	Comm::instance().transmit(0, sizeof(msg), reinterpret_cast<const uint8_t*>(&msg));
}

bool Client::Handshake()
{
	Cmds cmds;
	Server server(&cmds);

	server.SendCommand(Server::Hello);

	chrono::steady_clock::time_point start = chrono::steady_clock::now();
	while(!cmds.m_hello_received) {
		if (chrono::steady_clock::now() - start > timeout)
			return false;
		boost::this_thread::yield();
	}

	return true;
}

std::string Client::GetEncCfg()
{
	Cmds cmds;
	Server server(&cmds);

	server.SendCommand(Server::RequestEncConfig);

	chrono::steady_clock::time_point start = chrono::steady_clock::now();
	while(!cmds.m_enc_cfg_received && chrono::steady_clock::now() - start < timeout)
		boost::this_thread::yield();

	return cmds.m_enc_cfg;
}

std::string Client::GetMDCfg()
{
	Cmds cmds;
	Server server(&cmds);

	server.SendCommand(Server::RequestMDConfig);

	chrono::steady_clock::time_point start = chrono::steady_clock::now();
	while(!cmds.m_enc_cfg_received && chrono::steady_clock::now() - start < timeout)
		boost::this_thread::yield();

	return cmds.m_md_cfg;
}

std::vector<uint8_t> Client::GetROI()
{
	Cmds cmds;
	Server server(&cmds);

	server.SendCommand(Server::RequestROI);

	chrono::steady_clock::time_point start = chrono::steady_clock::now();
	while(!cmds.m_roi_received && chrono::steady_clock::now() - start < timeout)
		boost::this_thread::yield();

	return cmds.m_roi;
}
