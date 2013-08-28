#include <sys/mount.h>

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
int g_chroma_value = 0x80;

class ServerCmds : public IServerCmds
{
public:
	virtual bool Hello() {return true;}
	virtual void Start() {g_stop = false;}
	virtual void Stop() {g_stop = true;}

	virtual std::string GetEncCfg() {
		std::ifstream cfg(std::string("/mnt/2/encoder.cfg"));
		if (!cfg)
			return std::string();
		std::string str(std::istreambuf_iterator<char>(cfg), (std::istreambuf_iterator<char>()));
		return str;
	}
	
	virtual std::string GetMDCfg() {
		std::ifstream cfg(std::string("/mnt/2/md.cfg"));
		if (!cfg)
			return std::string();
		std::string str(std::istreambuf_iterator<char>(cfg), (std::istreambuf_iterator<char>()));
		return str;		
	}
	
	virtual std::vector<uint8_t> GetROI() {
		std::ifstream cfg(std::string("/mnt/2/md_roi.dat"), std::ios_base::binary | std::ios_base::ate);
		if (!cfg)
			return std::vector<uint8_t>();
		std::vector<uint8_t> str;
		str.resize(cfg.tellg());
		cfg.seekg(std::ios_base::beg);
		cfg.read((char*)&str[0], str.size()*sizeof(str[0]));
		return str;
	}
	
	virtual uint16_t GetRegister(uint8_t addr) {
		int fd;

		if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
			log() << "cannot open /dev/mem. su?";
			return 0;
		}

		void* map_base = mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0x04000000);

		uint16_t* regs = (uint16_t*)map_base;

		const uint16_t val = regs[addr/2];

		munmap((void*)map_base, 1024);
		close(fd);

		return val;		
	}

	virtual void SetEncCfg(const std::string& str) {
		mount("", "/mnt/2", "", MS_MGC_VAL | MS_REMOUNT, ""); // yes, MS_MGC_VAL is obsolete. But i don't believe linux at all.
		std::ofstream cfg(std::string("/mnt/2/encoder.cfg"));
		cfg << str;
		cfg.close();
		mount("", "/mnt/2", "", MS_MGC_VAL | MS_REMOUNT | MS_RDONLY, "");
	}
	virtual void SetMDCfg(const std::string& str) {
		mount("", "/mnt/2", "", MS_MGC_VAL | MS_REMOUNT, "");
		std::ofstream cfg(std::string("/mnt/2/md.cfg"));
		cfg << str;
		cfg.close();
		mount("", "/mnt/2", "", MS_MGC_VAL | MS_REMOUNT | MS_RDONLY, "");
	}
	virtual void SetROI(const std::vector<uint8_t>& str) {
		if (str.size()<384*144/8) // dirty hack while we have no stable comm channel with error correction.
			return;
		mount("", "/mnt/2", "", MS_MGC_VAL | MS_REMOUNT, "");
		std::ofstream cfg(std::string("/mnt/2/md_roi.dat"));
		cfg.write((char*)&str[0], str.size()*sizeof(str[0]));
		cfg.close();
		mount("", "/mnt/2", "", MS_MGC_VAL | MS_REMOUNT | MS_RDONLY, "");
	}
};

void setReg(uint8_t addr, uint16_t val)
{
	int fd;

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
		log() << "cannot open /dev/mem. su?";
		return;
    }

    void* map_base = mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0x04000000);

	uint16_t* regs = (uint16_t*)map_base;
	
	regs[addr/2] = val;

	munmap((void*)map_base, 1024);
    close(fd);
}

void auxiliaryCb(uint8_t camera, const uint8_t* payload, int comment)
{
	log() << "aux packet received: " << std::hex << (int)payload[-1] << " " << (int)payload[0] << " " << (int)payload[1] << 
	" " << (int)payload[2] << " " << (int)payload[3] << " " << (int)payload[4] << " " << (int)payload[5] <<
	" " << (int)payload[6] << " " << (int)payload[7] << " " << (int)payload[8] << " " << (int)payload[9] <<
	" " << (int)payload[10] << " " << (int)payload[11] << " " << (int)payload[12] << std::dec;

	log() << "addr:" << std::hex << (int)&payload[0] << std::dec;

	if (comment & ~Comm::Normal)
		log() << "Aux packet was received, flags: " << comment;

	log() << std::hex << "Type: " << Auxiliary::Type(payload);
	log() << "Should be: " << Auxiliary::RegisterValType << std::dec;

	if (Auxiliary::Type(payload) == Auxiliary::RegisterValType) {
		Auxiliary::RegisterValData reg = Auxiliary::RegisterVal(payload);
		log() << "Set reg 0x" << std::hex << (int)reg.addr << " to 0x" << reg.val << std::dec;
		setReg(reg.addr, reg.val);
	}
}

void dumpRegs()
{
	int fd;

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
		log() << "cannot open /dev/mem. su?";
		return;
    }

    void * volatile map_base = mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0x04000000);

	uint8_t* regs = (uint8_t*)map_base;
	
	for (int i=0; i<0x30; i+=2) {
		log() << std::hex << "reg 0x" << i << " : 0x" << *(uint16_t*)(regs+i);
	}

	log() << "end dump" << std::dec;

    munmap((void*)map_base, 1024);
    close(fd);
}

void restartFpga()
{
	int fd;
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
		log() << "cannot open /dev/mem. su?";
		return;
    }

/*FPGA RESET:
# cd /sys/class/gpio
# echo 49 > export
# cd /sys/class/gpio/gpio49
# echo "out" > direction
# echo 1 > value
# echo 0 > value*/

	void * volatile map_base = mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0x04000000);

	const int regs_num = 0x30/2;
	short regs[regs_num];

	uint16_t* p = (uint16_t*)map_base;
	for(int i = 0; i<regs_num; i++)
		regs[i] = *p++;

	FILE* fexport = fopen("/sys/class/gpio/export", "wt");
	fprintf(fexport, "92");
	fclose(fexport);
	
	FILE* fdir = fopen("/sys/class/gpio/gpio92/direction", "wt");
	fprintf(fdir, "out");
	fclose(fdir);

	FILE* fval = fopen("/sys/class/gpio/gpio92/value", "wb");
	fprintf(fval, "0\n");
	fflush(fval);
	boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
	fprintf(fval, "1\n");
	fclose(fval);
	boost::this_thread::sleep_for(boost::chrono::milliseconds(750));

	p = (uint16_t*)map_base;
	log() << "Firmware version: " << std::hex << *(p+0x2e/2) << std::dec;
	for(int i = 0; i<regs_num; i++)
		*p++ = regs[i];

    munmap((void*)map_base, 1024);
    close(fd);
}

void initMD()
{
	restartFpga();
	
	std::ifstream cfg(std::string("/mnt/2/md.cfg"));

	int val1 = -1, val2 = -1, val3 = -1, val4 = -1, val5 = -1, val6 = -1, val7 = -1;

	cfg >> val1 >> val2 >> val3 >> val4 >> val5 >> val6 >> val7;

	log() << "MD config : " << val1 << ", " << val2 << ", " << val3 << ", " << val4 << ", " << val5; 

	if (val1 < 0 || val2 < 0 || val3 < 0 || val4 < 0 || val5 < 0) {
		dumpRegs();
		return;
	}

    int fd;

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
		log() << "cannot open /dev/mem. su?";
		return;
    }

    void * volatile map_base = mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0x04000000);

	uint8_t* regs = (uint8_t*)map_base;
	
	*(uint16_t*)(regs + 0x014) = val1 & 0xffff;
	*(uint16_t*)(regs + 0x016) = val1 >> 16;

	const int tmp = *(uint16_t*)(regs + 0x01A);
	*(uint16_t*)(regs + 0x01A) = tmp & ~0x01f0 | (val2 << 4 & 0x01f0);

	*(uint16_t*)(regs + 0x000) = val3 & 0xffff;
	*(uint16_t*)(regs + 0x002) = val3 >> 16;
	
	*(uint16_t*)(regs + 0x004) = val4 & 0xffff;
	*(uint16_t*)(regs + 0x006) = val4 >> 16;
	
	*(uint16_t*)(regs + 0x00c) = val5 & 0xffff;
	*(uint16_t*)(regs + 0x00e) = val5 >> 16;
	
	*(uint16_t*)(regs + 0x008) = val6 & 0xffff;
	*(uint16_t*)(regs + 0x00a) = val6 >> 16;
	
	*(uint16_t*)(regs + 0x010) = val7 & 0xffff;
	*(uint16_t*)(regs + 0x012) = val7 >> 16;

	log() << "Time thres: 0x" << std::hex << val1 <<
			"), spat thres: 0x" << val2 << 
			"), top temp: 0x" << val3 <<
			"), bot temp: 0x" << val4 << 
			"), noise: 0x" << val5 <<
			"), noise2: 0x" << val6 << 
			"), inoise: 0x" << val7 << std::dec;

    munmap((void*)map_base, 1024);
    close(fd);
	
	dumpRegs();
}

void loadMask(std::vector<uint8_t>& info_mask, int s, int h)
{
	std::ifstream cfg(std::string("/mnt/2/md_roi.dat"), std::ios_base::binary | std::ios_base::ate);
	const std::streampos size = cfg.tellg();

//	if (size <= 0) {
	if (size < s*h/8) { // dirty hack while we have no stable comm channel with error correction.		
		info_mask.resize(s*h/8);
		memset(&info_mask[0], 0xff, info_mask.size());
		return;
	}

	assert(size == s*h/8);

	info_mask.resize(cfg.tellg());
	cfg.seekg(std::ios_base::beg);
	cfg.read((char*)&info_mask[0], info_mask.size()*sizeof(info_mask[0]));
	
	for(int i=0; i<info_mask.size(); i++) {
		if (info_mask[i] != 0)
			return;
	}
	
	// as info_mask has nothing but zeroes, fill it with ones
	memset(&info_mask[0], 0xff, info_mask.size());
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

void fillInfoFromFifo(std::vector<uint8_t>& info, const std::vector<uint8_t>& info_mask, int w, int s, int h)
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

void fillInfo(std::vector<uint8_t>& info, const std::vector<uint8_t>& info_mask, uint8_t* data, int w, int s, int h, int chroma_val)
{
	std::vector<uint8_t>::iterator it_info = info.begin();
	std::vector<uint8_t>::const_iterator it_info_mask = info_mask.begin();

// move detector data two pixels up-left

	for (int y=2; y<h; y++) {
		uint8_t *p = data + y*s + 2;
		for (int x=2; x<w; x+=16, p+=16) {
			uint8_t b = 0;
			const int pels_left = w-x > 16 ? 16 : w-x;
			for (int i=0; i<pels_left; i++) {
				if (i == 8)
					it_info_mask++;
				if (p[i] > 0x80 && *it_info_mask & 1<<7-i%8)
					b |= 1 << 7-i/2;
				p[i] = chroma_val;
			}

			it_info_mask++;
			*it_info++ = b;
		}
		it_info_mask += s/8;
	}
	
	memset(&info[(h-2)*s/2/8], 0, w/2*2/8); // set last two lines to zeroes

//	while (it_info != info.end())
//		*it_info++ = *it_info_mask++;
//		*it_info++ &= *it_info_mask++;
}

void run()
{
	const int w = 384;
	const int h = 144;

	const int to_skip = 5; // how much frames should be skipped after captured one to reduce framerate.

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

		fillInfo(info, info_mask, (uint8_t*)(buf.m.userptr + w*h), w, w, h/2, g_chroma_value); // data is in chroma planes.

		XDAS_Int8* bs = enc.encFrame((XDAS_Int8*)buf.m.userptr, w, h, w, &coded_size);

		cap.putFrame(buf);

//fwrite((uint8_t*)&info[0], 1, w/2*h/2/8, f_dump_info);

		const uint8_t* cur = encode_frame(&info[0], &info_out[0], w/2, h/2);

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
		if (argc < 3) {
			std::cout << "a.out <baud_rate> <flow_control>" << std::endl;
			return 0;
		}

		if (!Comm::instance().open("/dev/ttyS1", atoi(argv[1]), atoi(argv[2])))
			throw ex("Cannot open serial port");

		ServerCmds cmds;
		Server server(&cmds);

		Comm::instance().setCallback(auxiliaryCb, 3);

		CMEM_init();

		Enc::rman_init();

		if (argc == 4)
			g_chroma_value = atoi(argv[3]);

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
	
	::system("sync");

	return 0;
}
