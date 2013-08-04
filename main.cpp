#include <xdc/std.h>
#include <ti/xdais/ires.h>
#include <ti/sdo/fc/rman/rman.h>

#include <sys/mman.h>

#include "ext_headers.h"
#include "log_to_file.h"
#include "exception.h"
#include "comm.h"
#include "cap.h"
#include "enc.h"
#include "proto.h"
/*
extern "C" {
#include "tables.h"
#include "bs.h"
#include "cabac.h"
}*/

extern "C" uint8_t* encode_frame(const uint8_t* in, uint8_t* out, unsigned long w, unsigned long h);

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
	std::ifstream cfg(std::string("md.cfg"));

	int val1 = -1, val2 = -1, val3 = -1, val4 = -1, val5 = -1;

	cfg >> val1;
	cfg >> val2;
	cfg >> val3;
	cfg >> val4;
	cfg >> val5;

	log() << "MD config : " << val1 << ", " << val2 << ", " << val3 << ", " << val4 << ", " << val5; 
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

const char STARTCODE[12] = "FRAME START";

void readStartcode(const volatile uint16_t* reg, int thres)
{
	bool startcode_at_begin = true;
	
	int startcode_off = 0;
	int was_read = 0;

	while (startcode_off != sizeof(STARTCODE)/(sizeof(STARTCODE[0])) && was_read++ < thres) {
		const uint16_t val = *reg;

		if (val == *(uint16_t*)&STARTCODE[startcode_off]) {
			startcode_off += 2;
		} else {
			if (val == *(uint16_t*)&STARTCODE[0])
				startcode_off = 2;
			else
				startcode_off = 0;
			startcode_at_begin = false;
			continue;
		}
	}

	if (!startcode_at_begin)
		log() << "No startcode was found at the place where it should be. Was thrown " << was_read*2 << " bytes.";
	
	if (startcode_off == 12)
		log() << "Startcode was found";
	else	
		log() << "Startcode wasn't found";
}

void fillInfo(std::vector<uint8_t>& info, const std::vector<uint8_t>& info_mask, int w, int s, int h)
{
	std::vector<uint8_t>::iterator it_info = info.begin();
	std::vector<uint8_t>::const_iterator it_info_mask = info_mask.begin();

    int fd;

    if ((fd = open("/dev/mem", O_RDONLY | O_SYNC)) == -1) {
		log() << "cannot open /dev/mem. su?";
		return;
    }

    void * volatile map_base = mmap(0, 1024, PROT_READ, MAP_SHARED, fd, 0x04000000);

	const ptrdiff_t offset = 0x0030;

	readStartcode((uint16_t*)((uint8_t*)map_base + offset), w*h*2/8+12);

	for (int y=0; y<h*2; y+=2) {
		for(int i=0; i<w/8; i+=2) {
			const uint16_t val = *(volatile uint16_t*)((uint8_t*)map_base + offset);

			if (val == 0xFAAC || val == 0xACFA)
				log() << "FAAC!!!";

			*(uint16_t*)&*it_info = val; // бгыыы
			it_info += 2;

//			*it_info++ = val & 0x0ff;
//			*it_info++ = val >> 8;
		}

		for(int i=0; i<w/8; i+=2) {
			const uint16_t val = *(volatile uint16_t*)((uint8_t*)map_base + offset);

			if (val == 0xFAAC || val == 0xACFA)
				log() << "FAAC!!!";
		}
	}

    munmap((void*)map_base, 1024);
    close(fd);

//	while (it_info != info.end())
//		*it_info++ = *it_info_mask++;
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

	std::vector<uint8_t> info_out;
	info_out.resize(w*h/8);

//	bs_t info_bs;

	initMD();

	v4l2_buffer buf = cap.getFrame(); // skip first frame as it contains garbage
	cap.putFrame(buf);

//	FILE* f_dump = fopen("dump.h264", "wb");
//FILE* f_dump_info = fopen("dump.anal", "wb");

	while(!g_stop) {
		v4l2_buffer buf = cap.getFrame();

		size_t coded_size=0;

		XDAS_Int8* bs = enc.encFrame((XDAS_Int8*)buf.m.userptr, w, h, w, &coded_size);

		cap.putFrame(buf);

		fillInfo(info, info_mask, w, w, h);

//fwrite((uint8_t*)&info[0], 1, w*h/8, f_dump_info);

		const uint8_t* cur = encode_frame(&info[0], &info_out[0], w, h);

		const ptrdiff_t info_size = cur - &info_out[0];

		Auxiliary::SendTimestamp(buf.timestamp.tv_sec, buf.timestamp.tv_usec);

		if (coded_size) {
			Comm::instance().transmit(0, 1, coded_size, (uint8_t*)bs);

//			fwrite((uint8_t*)bs, 1, coded_size, f_dump);
		}

		if (info_size)
			Comm::instance().transmit(0, 2, info_size, &info_out[0]);

		for (int i=0; i<to_skip; i++) {
			v4l2_buffer buf = cap.getFrame();
			cap.putFrame(buf);
		}
	}
	
//	fclose(f_dump);
//fclose(f_dump_info);
}

int main(int argc, char *argv[])
{
	try {
		if (argc != 3) {
			std::cout << "a.out <baud_rate> <flow_control>" << std::endl;
			return 0;
		}

		if (!Comm::instance().open("/dev/ttyS1", atoi(argv[1]), atoi(argv[2])))
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
