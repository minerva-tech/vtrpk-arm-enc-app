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
#include "flir.h"

#include "defines.h"
/*
extern "C" {
#include "tables.h"
#include "bs.h"
#include "cabac.h"
}*/

extern "C" uint8_t* encode_frame(const uint8_t* in, uint8_t* out, unsigned long w, unsigned long h);

volatile bool g_stop = false;
int g_chroma_value = 0x80;
bool g_dump_yuv = false;

int g_tx_buffer_size = 1000;

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

class ServerCmds : public IServerCmds
{
public:
	ServerCmds(Flir* flir = NULL) : m_flir(flir) {}

	virtual bool Hello(int id) {Comm::instance().drop_unsent();return true;}
	virtual void Start() {log()<<"start";g_stop = false;}
	virtual void Stop() {g_stop = true;}

	virtual std::string GetEncCfg() {
		std::ifstream eeprom(eeprom_filename);
		if (!eeprom)
			return std::string();

		eeprom.seekg(encoder_config_offset, std::ios_base::beg);

		uint32_t size = 0;
		eeprom.read((char*)&size, sizeof(size));

		if (size > encoder_config_max_size)
			return std::string();

		std::string str;
		str.resize(size);

		eeprom.read(&str[0], str.size()*sizeof(str[0]));

		return str;
	}

	virtual std::string GetMDCfg() {
		std::ifstream eeprom(eeprom_filename);
		if (!eeprom)
			return std::string();

		eeprom.seekg(md_config_offset, std::ios_base::beg);

		uint32_t size = 0;
		eeprom.read((char*)&size, sizeof(size));

		log() << "MD config size: " << size;

		if (size > md_config_max_size)
			return std::string();

		std::string str;
		str.resize(size);

		eeprom.read(&str[0], str.size()*sizeof(str[0]));

		return str;
	}

	virtual std::vector<uint8_t> GetROI() {
		std::ifstream eeprom(eeprom_filename);
		if (!eeprom)
			return std::vector<uint8_t>();

		eeprom.seekg(roi_config_offset, std::ios_base::beg);

		uint32_t size = 0;
		eeprom.read((char*)&size, sizeof(size));

		if (size > roi_config_max_size)
			return std::vector<uint8_t>();

		std::vector<uint8_t> str;
		str.resize(size);

		eeprom.read((char*)&str[0], str.size()*sizeof(str[0]));

		return str;
	}
	
	virtual std::string GetVersionInfo() {
		std::string ver;

		std::ifstream version_file(version_info_filename);
		if (!version_file) {
			ver = "No system version info.\n";
		} else {
			version_file.seekg(0, std::ios_base::end);
			const uint32_t size = version_file.tellg();
			version_file.seekg(0, std::ios_base::beg);

			log() << "Version Info size: " << size;

			ver.resize(size);

			version_file.read(&ver[0], ver.size()*sizeof(ver[0]));
		}
		
		if (m_flir) {
			char t[128];
			sprintf(t, "\"Teplovisor\" build\t\t%u (%u)\n", (unsigned long)&__BUILD_NUMBER, (unsigned long)&__BUILD_DATE); ver += t;

			sprintf(t, "FPGA revision\t\t\t%x\n", GetRegister(0x2e)); ver += t;

			uint32_t flir_data[4];
			m_flir->get_serials(flir_data);
			sprintf(t, "Camera serial number\t\t%u\n", flir_data[0]); ver += t;
			sprintf(t, "Sensor serial number\t\t%u\n", flir_data[1]); ver += t;
			sprintf(t, "Camera SW version\t\t%u\n", flir_data[2]); ver += t;
			sprintf(t, "Camera HW version\t\t%u\n", flir_data[3]); ver += t;
		}

		return ver;
	}

	virtual uint8_t GetCameraID() {
		std::ifstream eeprom(eeprom_filename);
		if (!eeprom)
			return 0;

		eeprom.seekg(cam_id_config_offset, std::ios_base::beg);

		uint8_t id;
		eeprom >> id;

		id = std::min((uint8_t)3, id);

		return id;
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
		if (str.size() > encoder_config_max_size)
			return;

		std::ofstream eeprom(eeprom_filename);
		if (!eeprom)
			return;

		eeprom.seekp(encoder_config_offset, std::ios_base::beg);

		const int size = str.size();
		eeprom.write((char*)&size, sizeof(size));

		eeprom.write(&str[0], str.size()*sizeof(str[0]));
	}

	virtual void SetMDCfg(const std::string& str) {
		if (str.size() > md_config_max_size)
			return;

		std::ofstream eeprom(eeprom_filename);
		if (!eeprom)
			return;

		eeprom.seekp(md_config_offset, std::ios_base::beg);

		const int size = str.size();
		eeprom.write((char*)&size, sizeof(size));

		eeprom.write(&str[0], str.size()*sizeof(str[0]));
	}

	virtual void SetROI(const std::vector<uint8_t>& str) {
		if (str.size() > roi_config_max_size)
			return;

		std::ofstream eeprom(eeprom_filename);
		if (!eeprom)
			return;

		eeprom.seekp(roi_config_offset, std::ios_base::beg);

		const int size = str.size();
		eeprom.write((char*)&size, sizeof(size));

		eeprom.write((char*)&str[0], str.size()*sizeof(str[0]));
	}
	
	virtual void SetVersionInfo(const std::string& str)
	{
		// nothing to do.
	}

	virtual void SetCameraID(uint8_t id) {
		std::ofstream eeprom(eeprom_filename);
		if (!eeprom)
			return;

		eeprom.seekp(cam_id_config_offset, std::ios_base::beg);

		eeprom << id;

		Comm::instance().setCameraID(id);

		log() << "New camera ID received : " << (int)id;
	}
	
private:
	Flir* m_flir;
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

void auxiliaryCb(uint8_t camera, const uint8_t* payload, int comment, Flir& flir)
{
	log() << "aux packet received: " << std::hex << (int)payload[-1] << " " << (int)payload[0] << " " << (int)payload[1] <<
	" " << (int)payload[2] << " " << (int)payload[3] << " " << (int)payload[4] << " " << (int)payload[5] <<
	" " << (int)payload[6] << " " << (int)payload[7] << " " << (int)payload[8] << " " << (int)payload[9] <<
	" " << (int)payload[10] << " " << (int)payload[11] << " " << (int)payload[12] << std::dec;

	log() << "addr:" << std::hex << (int)&payload[0] << std::dec;

	if (camera != Comm::instance().cameraID()) {
		log() << "Aux packet rejected, wrong camera ID: " << Comm::instance().cameraID() << " (my camera id: " << camera << " )";
		return;
	}

	if (comment & ~Comm::Normal)
		log() << "Aux packet was received, flags: " << comment;

//	log() << std::hex << "Type: " << Auxiliary::Type(payload);
//	log() << "Should be: " << Auxiliary::RegisterValType << std::dec;

	if (Auxiliary::Type(payload) == Auxiliary::RegisterValType) {
		Auxiliary::RegisterValData reg = Auxiliary::RegisterVal(payload);
		log() << "Set reg 0x" << std::hex << (int)reg.addr << " to 0x" << reg.val << std::dec;
		setReg(reg.addr, reg.val);
	}

	if (Auxiliary::Type(payload) == Auxiliary::CameraRegisterValType) {
		Auxiliary::CameraRegisterValData data = Auxiliary::CameraRegisterVal(payload);

        log() << "sending data to camera, size : " << Auxiliary::Size(payload);

		flir.send(data.val, Auxiliary::Size(payload));
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
/*
	FILE* fexport = fopen("/sys/class/gpio/export", "wt");
	fprintf(fexport, "8");
	fclose(fexport);

	FILE* fdir = fopen("/sys/class/gpio/gpio8/direction", "wt");
	fprintf(fdir, "out");
	fclose(fdir);

	FILE* fval = fopen("/sys/class/gpio/gpio8/value", "wb");
	fprintf(fval, "0\n");
	fflush(fval);
	boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
	fprintf(fval, "1\n");
	fclose(fval);
	boost::this_thread::sleep_for(boost::chrono::milliseconds(1750));
*/
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

	int fd;

	if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
		log() << "cannot open /dev/mem. su?";
		return;
	}

	void * volatile map_base = mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0x04000000);

	uint8_t* regs = (uint8_t*)map_base;

	ServerCmds tmp_cmds;
	std::string md_cfg = tmp_cmds.GetMDCfg();
	std::stringstream cfg(md_cfg);

	int val1 = -1, val2 = -1, val3 = -1, val4 = -1, val5 = -1, val6 = -1, val7 = -1;

	cfg >> val1 >> val2 >> val3 >> val4 >> val5 >> val6 >> val7;

	log() << "MD config : " << val1 << ", " << val2 << ", " << val3 << ", " << val4 << ", " << val5;

	if (val1 < 0 || val2 < 0 || val3 < 0 || val4 < 0 || val5 < 0) {
		dumpRegs();
		return;
	}

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
	ServerCmds tmp_cmds;
	std::vector<uint8_t> roi = tmp_cmds.GetROI();

	info_mask.resize(s*h/8);

	if (roi.size() < s*h/8) { // dirty hack while we have no stable comm channel with error correction.
		memset(&info_mask[0], 0xff, info_mask.size());
		return;
	}

	std::copy(roi.begin(), roi.begin()+s*h/8, info_mask.begin());

	for(int i=0; i<info_mask.size(); i++) {
		if (info_mask[i] != 0)
			return;
	}

	// as info_mask has nothing but zeroes, fill it with ones
	memset(&info_mask[0], 0xff, info_mask.size());
}
/*
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
*/
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
				if ( (p[i] < 0xC0) && (p[i] > 0x80) && *it_info_mask & 1<<7-i%8)
					b |= 1 << 7-i/2;
				//p[i] = chroma_val;
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

struct adapt_bitrate_desc_t {
	int to_skip;
	int switch_up_from_here;
	int switch_down_from_here;
};

static void LED_RXD(int on)
{
    FILE* fled = fopen("/sys/class/leds/rxd/brightness", "wt");
    if(NULL==fled) log()<< " - LED RXD open failed";

    if(on)
        fprintf(fled, "1");
    else
        fprintf(fled, "0");

	fflush(fled);
    fclose(fled);
    //boost::this_thread::sleep_for(boost::chrono::milliseconds(1750));
}

static void LED_ERR(int on)
{
    FILE* fled = fopen("/sys/class/leds/error/brightness", "wt");
    if(NULL==fled) log()<< " - LED ERR open failed";

    if(on)
        fprintf(fled, "1");
    else
        fprintf(fled, "0");

	fflush(fled);
    fclose(fled);
    //boost::this_thread::sleep_for(boost::chrono::milliseconds(1750));
}

adapt_bitrate_desc_t g_adapt_bitrate_desc[] = {{0, 50, -1}, {1, 60, 40}, {3, 70, 50}, {7, 80, 60}, {-1, 1000, 70}};

void run()
{
	const int w = TARGET_WIDTH;
	const int h = TARGET_HEIGHT;

	int adapt_bitrate_pos = 0;
	int to_skip = g_adapt_bitrate_desc[adapt_bitrate_pos].to_skip; // how much video frames should be skipped according to current channel state
	
	Comm::instance().allowTransmission(true);

	while(g_stop) {
		boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
	}

	ServerCmds tmp_cmds;
	std::string enc_cfg = tmp_cmds.GetEncCfg();

	Enc enc(enc_cfg);

	Cap cap(SRC_WIDTH, SRC_HEIGHT);

	cap.start_streaming();

	std::vector<uint8_t> info;
	info.resize(w*h/8);

	std::vector<uint8_t> info_mask;
	loadMask(info_mask, w, h);

	std::vector<uint8_t> info_out;
	info_out.resize(w*h/8);

	initMD();

	v4l2_buffer buf = cap.getFrame(); // skip first frame as it contains garbage
	cap.putFrame(buf);
	
	struct timespec clock_cur;
	clock_gettime(CLOCK_MONOTONIC, &clock_cur);
	printf("Teplovisor app first frame captured time: %lu ms\n", clock_cur.tv_nsec/1000000+clock_cur.tv_sec*1000);

//	FILE* f_dump = fopen("dump.h264", "wb");
//FILE* f_dump_info = fopen("dump.anal", "wb");

	FILE* f_dump_yuv = NULL;

	if (g_dump_yuv)
		f_dump_yuv = fopen("dump.yuv", "wb");

	while(!g_stop) {
        // Startup time measure point #1
        LED_RXD(1);

		v4l2_buffer buf = cap.getFrame();

		if (g_dump_yuv) {
			fwrite((uint8_t*)buf.m.userptr, 1, w*h*3/2, f_dump_yuv);
		}

		fillInfo(info, info_mask, (uint8_t*)(buf.m.userptr + w*h), w, w, h/2, g_chroma_value); // data is in chroma planes.

		size_t coded_size=0;

		XDAS_Int8* bs = NULL;

		if (!to_skip) {
			bs = enc.encFrame((XDAS_Int8*)buf.m.userptr, w, h, w, &coded_size);
			to_skip = g_adapt_bitrate_desc[adapt_bitrate_pos].to_skip;
			log() << "Frame encoded";
		} else {
			if (to_skip>0) // to_skip == -1 means no video is sent at all
				to_skip--; 

			log() << "     VRTPK App: Frame skipped";
		}

		cap.putFrame(buf);

		const uint8_t* cur = encode_frame(&info[0], &info_out[0], w/2, h/2);

		const ptrdiff_t info_size = cur - &info_out[0];

		Auxiliary::SendTimestamp(buf.timestamp.tv_sec, buf.timestamp.tv_usec);

        // Startup time measure point #2
        LED_ERR(0);

		if (coded_size) {
			Comm::instance().transmit(1, coded_size, (uint8_t*)bs);

//			fwrite((uint8_t*)bs, 1, coded_size, f_dump);
		}

		if (info_size)
			Comm::instance().transmit(2, info_size, &info_out[0]);

		const int buf_size = Comm::instance().getBufferSize();
		log() << "buffer size " << buf_size;
		if (buf_size > g_adapt_bitrate_desc[adapt_bitrate_pos].switch_up_from_here) {
			adapt_bitrate_pos++;
			log() << "switched to " << adapt_bitrate_pos;
		}
		if (buf_size < g_adapt_bitrate_desc[adapt_bitrate_pos].switch_down_from_here) {
            if (to_skip<0)
                to_skip = 0;
			adapt_bitrate_pos--;
			log() << "switched to " << adapt_bitrate_pos;
		}
	}

	if (g_dump_yuv)
		fclose(f_dump_yuv);

//	fclose(f_dump);
//fclose(f_dump_info);
}

int main(int argc, char *argv[])
{
	struct timespec clock_cur;
	clock_gettime(CLOCK_MONOTONIC, &clock_cur);
	printf("Teplovisor app starting time: %lu ms\n", clock_cur.tv_nsec/1000000+clock_cur.tv_sec*1000);
	
	try {

        LED_ERR(1);

		if (argc < 3) {
			std::cout << "a.out <baud_rate> <flow_control> <output buffer size (PACKETS, NOT BYTES)>\n" << std::endl;
			return 0;
		}

		if (!Comm::instance().open("/dev/ttyS1", atoi(argv[1]), atoi(argv[2])))
			throw ex("Cannot open serial port");

		Flir flir("/dev/ttyS0");

		ServerCmds cmds(&flir); // flir instance is needed to ask it for versions/serials when host asks it.
		Server server(&cmds);

		Comm::instance().setCameraID(cmds.GetCameraID());

		Comm::instance().setCallback(boost::bind(auxiliaryCb, _1, _2, _3, boost::ref(flir)), 3);

		CMEM_init();

		Enc::rman_init();

		if (argc==4)
			g_tx_buffer_size = atoi(argv[3]);

		Comm::instance().setTxBufferSize(g_tx_buffer_size);

		while(1) {
			try {
				run();
			}
			catch (ex& e) {
				log() << e.str();
				g_stop = true;
			}
			catch(std::exception& e) {
				log() << "std::exception: " << e.what();
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