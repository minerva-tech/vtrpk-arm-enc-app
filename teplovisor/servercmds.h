#ifndef SERVERCMDS_H
#define SERVERCMDS_H

extern volatile bool g_stop;
extern volatile int  g_bitrate;

class ServerCmds : public IServerCmds
{
public:
#if VIDEO_SENSOR
	ServerCmds(VSensor* vsensor = NULL) : m_vsensor(vsensor) {}
#else
	ServerCmds(Flir* flir = NULL) : m_flir(flir) {}
#endif

	virtual bool Hello(int id) {Comm::instance().drop_unsent();return true;}
	virtual void Start() {log()<<"start";g_stop = false;}
	virtual void Stop() {g_stop = true;}

	virtual std::string GetEncCfg();
	virtual std::string GetMDCfg();
	virtual std::vector<uint8_t> GetROI();
	virtual std::string GetVersionInfo();
    virtual int GetStreamsEnableFlag();
	virtual uint8_t GetCameraID();
	virtual Auxiliary::VideoSensorParameters GetVideoSensorParameters();
	virtual uint16_t GetRegister(uint8_t addr);

	virtual void SetEncCfg(const std::string& str);
	virtual void SetMDCfg(const std::string& str);
	virtual void SetROI(const std::vector<uint8_t>& str);
	virtual void SetVersionInfo(const std::string& str);
    virtual void SetStreamsEnableFlag(int streams_enable);
	virtual void SetCameraID(uint8_t id);

    virtual void BufferClear();
    virtual void SetBitrate(int bitrate);

private:

#if VIDEO_SENSOR
	VSensor* m_vsensor;
#else
	Flir* m_flir;
#endif
};

#endif
