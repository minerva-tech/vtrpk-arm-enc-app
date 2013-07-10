#include <xdc/std.h>
#include <ti/xdais/ires.h>
#include <ti/sdo/fc/rman/rman.h>

#include "ext_headers.h"
#include "log_to_file.h"
#include "exception.h"
#include "comm.h"
#include "cap.h"
#include "enc.h"
#include "proto.h"

extern "C" {
#include "tables.h"
#include "bs.h"
#include "cabac.h"
}

volatile bool g_stop = false;

class ServerCmds : public IServerCmds
{
public:
	virtual bool Hello() {return true;}
	virtual void Start() {g_stop = false;}
	virtual void Stop() {g_stop = true;}

	virtual std::string GetEncCfg() {
		std::ifstream cfg(std::string("encoder_console.cfg"));
		if (!cfg)
			return std::string();
		std::string str(std::istreambuf_iterator<char>(cfg), (std::istreambuf_iterator<char>()));
		return str;
	}
	virtual std::string GetMDCfg() {
		std::ifstream cfg(std::string("md.cfg"));
		if (!cfg)
			return std::string();
		std::string str(std::istreambuf_iterator<char>(cfg), (std::istreambuf_iterator<char>()));
		return str;		
	}
	virtual std::vector<uint8_t> GetROI() {
		std::ifstream cfg(std::string("md_roi.dat"), std::ios_base::binary | std::ios_base::ate);
		if (!cfg)
			return std::vector<uint8_t>();
		std::vector<uint8_t> str;
		str.resize(cfg.tellg());
		cfg.seekg(std::ios_base::beg);
		cfg.read((char*)&str[0], str.size()*sizeof(str[0]));
		return str;
	}

	virtual void SetEncCfg(const std::string& str) {
		std::ofstream cfg(std::string("encoder_console.cfg"));
		cfg << str;
	}
	virtual void SetMDCfg(const std::string& str) {
		std::ofstream cfg(std::string("md.cfg"));
		cfg << str;
	}
	virtual void SetROI(const std::vector<uint8_t>& str) {
		std::ofstream cfg(std::string("md_roi.dat"));
		cfg.write((char*)&str[0], str.size()*sizeof(str[0]));
	}
};

void initMD()
{
	
}

void loadMask(std::vector<uint8_t>& info_mask, int s, int h)
{
	std::ifstream cfg(std::string("md_roi.dat"), std::ios_base::binary | std::ios_base::ate);
	const std::streampos size = cfg.tellg();
	
	if (size <= 0) {
		info_mask.resize(s*h/8);
		return;
	}

	assert(size == s*h/8);

	info_mask.resize(cfg.tellg());
	cfg.seekg(std::ios_base::beg);
	cfg.read((char*)&info_mask[0], info_mask.size()*sizeof(info_mask[0]));
}

void fillInfo(std::vector<uint8_t>& info, const std::vector<uint8_t>& info_mask, int w, int s, int h)
{
	std::vector<uint8_t>::iterator it_info = info.begin();
	std::vector<uint8_t>::const_iterator it_info_mask = info_mask.begin();

	while (it_info != info.end())
		*it_info++ = *it_info_mask++;
//		*it_info++ &= *it_info_mask++;
}

void run()
{
	const int w = 384;
	const int h = 144;

	const int to_skip = 10; // how much frames should be skipped after captured one to reduce framerate.

	while(g_stop)
		boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

	Enc enc;

	Cap cap(384, 288);

	cap.start_streaming();

	std::vector<uint8_t> info;
	info.resize(w*h/8);

	std::vector<uint8_t> info_mask;
	loadMask(info_mask, w, h);

//	bs_t info_bs;

	initMD();

	v4l2_buffer buf = cap.getFrame(); // skip first frame as it contains garbage
	cap.putFrame(buf);

//	FILE* f_dump = fopen("dump.h264", "wb");

	while(!g_stop) {
		v4l2_buffer buf = cap.getFrame();

		size_t coded_size=0;

		XDAS_Int8* bs = enc.encFrame((XDAS_Int8*)buf.m.userptr, w, h, w, &coded_size);

		cap.putFrame(buf);

		fillInfo(info, info_mask, w, w, h);

/*		bs_create(&info_bs);
		bs_resize(&info_bs, w*h/8);

		const int info_size = encode_frame(&info[0], &info_bs, w*h/8);
*/
		Auxiliary::SendTimestamp(buf.timestamp.tv_sec, buf.timestamp.tv_usec);

		if (coded_size) {
			
			Comm::instance().transmit(0, 1, coded_size, (uint8_t*)bs);
			
//			fwrite((uint8_t*)bs, 1, coded_size, f_dump);
		}
/*
		if (info_size)
			Comm::instance().transmit(0, 2, info_size, (uint8_t*)info_bs.stream);

		bs_delete(&info_bs);
*/
		for (int i=0; i<to_skip; i++) {
			v4l2_buffer buf = cap.getFrame();
			cap.putFrame(buf);
		}
	}
	
//	fclose(f_dump);
}

int main(int argc, char *argv[])
{
	try {
		if (!Comm::instance().open("/dev/ttyS1"))
			throw ex("Cannot open serial port");

		ServerCmds cmds;
		Server server(&cmds);

		CMEM_init();
		
		Enc::rman_init();

		while(1) {
			try {
				run();
			}
			catch (ex& e) {
				log() << e.str();
				g_stop = true;
			}
		}
	}
	catch (ex& e) {
		log() << e.str();
	}

	return 0;
}
