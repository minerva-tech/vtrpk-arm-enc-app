#ifndef SERVERCMDS_H
#define SERVERCMDS_H

extern volatile bool g_stop;

class ServerCmds : public IServerCmds
{
public:
	ServerCmds(Flir* flir = NULL) : m_flir(flir) {}

	virtual bool Hello(int id) {Comm::instance().drop_unsent();return true;}
	virtual void Start() {log()<<"start";g_stop = false;}
	virtual void Stop() {g_stop = true;}

	virtual std::string GetEncCfg();
	virtual std::string GetMDCfg();
	virtual std::vector<uint8_t> GetROI();
	virtual std::string GetVersionInfo();
	virtual uint8_t GetCameraID();
	virtual uint16_t GetRegister(uint8_t addr);
	virtual void SetEncCfg(const std::string& str);
	virtual void SetMDCfg(const std::string& str);
	virtual void SetROI(const std::vector<uint8_t>& str);
	virtual void SetVersionInfo(const std::string& str);
	virtual void SetCameraID(uint8_t id);
	
private:
	Flir* m_flir;
};

#endif
