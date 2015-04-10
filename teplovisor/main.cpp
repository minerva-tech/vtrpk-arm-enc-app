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
#include "vsensor.h"

#include "defines.h"
#include "default_enc_cfg.h"

#include "servercmds.h"

/*
extern "C" {
#include "tables.h"
#include "bs.h"
#include "cabac.h"
}*/

extern "C" uint8_t* encode_frame(const uint8_t* in, uint8_t* out, unsigned long w, unsigned long h);

volatile bool g_stop = false;
volatile int  g_bitrate = -1;
volatile bool g_change_bitrate = false; // TODO argh, it's soooo ugly. It must be changed as soon as i'll have hardware for checking that console still works after all changes.
int g_chroma_value = 0x80;
bool g_dump_yuv = false;

int g_tx_buffer_size = 1000;

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

#if not VIDEO_SENSOR
void auxiliaryCb(uint8_t camera, const uint8_t* payload, int comment, Flir& flir)
#else
void auxiliaryCb(uint8_t camera, const uint8_t* payload, int comment, VSensor& vsensor)
#endif
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

	switch(Auxiliary::Type(payload)) {
		case Auxiliary::RegisterValType : {
			Auxiliary::RegisterValData reg = Auxiliary::RegisterVal(payload);
			log() << "Set reg 0x" << std::hex << (int)reg.addr << " to 0x" << reg.val << std::dec;
			setReg(reg.addr, reg.val);
			break;
		}
		case Auxiliary::CameraRegisterValType : {
			Auxiliary::CameraRegisterValData data = Auxiliary::CameraRegisterVal(payload);
			log() << "sending data to camera, size : " << Auxiliary::Size(payload);
#if not VIDEO_SENSOR			
			flir.send(data.val, Auxiliary::Size(payload));
#endif
			break; 
		}
		case Auxiliary::VideoSensorResolutionType: {
			Auxiliary::VideoSensorResolutionData res = Auxiliary::VideoSensorResolution(payload);
			log() << "new video sensor resolutions: " << res.src_w << "x" << res.src_h << ", dst res: " << res.dst_w << "x" << res.dst_h;
			
			std::ofstream eeprom(eeprom_filename);
			if (!eeprom)
				break;

			eeprom.seekp(vsensor_config_offset, std::ios_base::beg);
			eeprom.write((char*)&res, sizeof(res));
			break;
		}
		case Auxiliary::VideoSensorSettingsType: {
			Auxiliary::VideoSensorSettingsData set = Auxiliary::VideoSensorSettings(payload);

			std::ofstream eeprom(eeprom_filename);
			if (!eeprom)
				break;

            log() << "received fps divider: " << set.fps_divider;

			eeprom.seekp(vsensor_config_offset+sizeof(Auxiliary::VideoSensorResolutionData), std::ios_base::beg);
			eeprom.write((char*)&set, sizeof(set));
//			vsensor.set(set);
			break;
		}
		default:
			log() << "Something goes wrong, auxiliary packet of unknoun type was received.";
			break;
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

struct adapt_bitrate_desc_t {
	int to_skip;
	int switch_up_from_here;
	int switch_down_from_here;
};


static const double analog_gain_table[8] = { 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 8.0 };
static const double digital_gain_step = (15.875/255.0);

struct aec_parameters_t{
    uint16_t ROI_T_INT_II;   //
    uint16_t ROI_T_INT_CLK;  //
    uint16_t ROI_ANA_GAIN;   //
    uint16_t ROI_DIG_GAIN;   //
    uint64_t FRAME;          // raw frame counter
};

static aec_parameters_t aec[8];


adapt_bitrate_desc_t g_adapt_bitrate_desc[] = {{0, 50, -1}, {1, 60, 40}, {3, 70, 50}, {7, 80, 60}, {-1, 1000, 70}};

#if VIDEO_SENSOR
void run(VSensor& vsensor)
#else
void run()
#endif
{
	Comm::instance().allowTransmission(true);

	while(g_stop) {
		boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
	}

    int fps_divider = 1;

	Auxiliary::VideoSensorResolutionData res = {-1, -1, -1, -1};

#if VIDEO_SENSOR
	std::ifstream eeprom(eeprom_filename);
	if (eeprom) {
		Auxiliary::VideoSensorSettingsData vs_set;

		eeprom.seekg(vsensor_config_offset, std::ios_base::beg);
		eeprom.read((char*)&res, sizeof(res));
		eeprom.read((char*)&vs_set, sizeof(vs_set));

		vsensor.set(vs_set);

		fps_divider = vs_set.fps_divider;
	}
#endif

	if (res.src_w <= 0 || res.src_h <= 0 || res.dst_w <= 0 || res.dst_h <= 0) {
		res.src_w = SRC_WIDTH;
		res.src_h = SRC_HEIGHT;
		res.dst_w = TARGET_WIDTH;
		res.dst_h = TARGET_HEIGHT;
	}
  res.dst_h = 1024;// TODO: ЭТО ЧИСТО ДЕБАЖНОЕ!!!! ЗАМЕНИТЬ!!!!!
	const int w = res.dst_w;
	const int h = res.dst_h;

	int adapt_bitrate_pos = 0;
	int to_skip = g_adapt_bitrate_desc[adapt_bitrate_pos].to_skip * fps_divider; // how much video frames should be skipped according to current channel state
	int to_skip_motion = 0;

	log() << "wxh: " << res.src_w << "x" << res.src_h << " -> " << res.dst_w << "x" << res.dst_h;
	log() << "Fps divider : " << (int)fps_divider;
	log() << "Bitrate : " << g_bitrate;

	ServerCmds tmp_cmds;
	std::string enc_cfg = tmp_cmds.GetEncCfg();

	Enc enc;
	enc.init(enc_cfg, res.dst_w, res.dst_h, g_bitrate);

	Cap cap(res.src_w, res.src_h, res.dst_w, res.dst_h);

	cap.start_streaming();

	std::vector<uint8_t> info;
	info.resize(w*h/8);

	std::vector<uint8_t> info_mask;
	loadMask(info_mask, w, h);

	std::vector<uint8_t> info_out;
	info_out.resize(w*h/8);

	initMD();

    bool motion_enable = true;//tmp_cmds.GetStreamsEnableFlag() & MOTION_ENABLE_BIT;

    log() << "motion_enable = " << (int)motion_enable;

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

		if (g_change_bitrate) {
			enc.changeBitrate(g_bitrate);
			g_change_bitrate = false;
		}

		v4l2_buffer buf = cap.getFrame();

		if (g_dump_yuv) {
			fwrite((uint8_t*)buf.m.userptr, 1, w*h*3/2, f_dump_yuv);
		}


  // Вот ТУТ, из (uint8_t*)buf.m.userptr надо доставать 1ую и 2 последние строки.
  // Подробное, с излишним количеством локальных переменных, описание
  // сделано исключительно для тупящих.
      {
          /*
           * Draw histogram and some numbers
           */
            uint8_t* ptl_luma_plane = (uint8_t*)buf.m.userptr;
        #ifdef FIRST_LINE_OK
            uint8_t* ptr_header_line;
        #endif
            uint8_t* ptr_footer_line;
            uint8_t* ptr_histogram_line;
            uint16_t* ptr_histogram;
            uint32_t  histo_summ = 0;
            uint16_t Blacks,Whites;
            double average_luma = 0;
            int i,j;
            unsigned short tmp;
            int histogram[4] = {0,0,0,0};

        #ifdef FIRST_LINE_OK
            ptr_header_line = ptl_luma_plane;
            ptr_footer_line = ptl_luma_plane + (sizeof(uint8_t) * w * (h-1));
            ptr_histogram_line = ptl_luma_plane + (sizeof(uint8_t) * w * (h-2));
            ptr_histogram = (uint16_t*)ptr_histogram_line;

            printf(" -- Header -- w=%d h=%d\n", w, h);
            for(i=0; i<25; i++) printf( " %02X",ptr_header_line[i]);
            printf("\n\n");

            printf("ROI analog gain: %d\n", ptr_header_line[14]);
            printf("ROI global digital gain: %d\n", ptr_header_line[15]);
        #else
            ptr_footer_line = ptl_luma_plane + (sizeof(uint8_t) * w * (h-2));
            ptr_histogram_line = ptl_luma_plane + (sizeof(uint8_t) * w * (h-3));
            ptr_histogram = (uint16_t*)ptr_histogram_line;
        #endif

            printf(" -- Footer -- w=%d h=%d\n", w, h);
            for(i=0; i<5; i++) printf(" %02X",ptr_footer_line[0]);
            printf("\n");

            printf("Error flags: 0x%02X\n",ptr_footer_line[0]);
            tmp = ((unsigned short)ptr_footer_line[1]<<8 | (unsigned short)ptr_footer_line[2]);
            printf("0x00 pixels: %d\n",tmp);
            Blacks = tmp;
            tmp = ((unsigned short)ptr_footer_line[3]<<8 | (unsigned short)ptr_footer_line[4]);
            printf("0xFF pixels: %d\n",tmp);
            Whites = tmp;

            printf(" -- Histogramma --\n");
            for(i=0; i<(64*2); i+=2){
                int stars;
                int bin_index=i/2;
                tmp = ((unsigned short)ptr_histogram_line[i]<<8 | (unsigned short)ptr_histogram_line[i+1]);
                stars = (int)(((float)tmp)/(1280.) + 0.5);
                printf( " %6d ",tmp);
                for(j=0;j<stars;j++) printf("*");
                printf("\n");

                histo_summ += (uint32_t)tmp;

                /* 0 1 ** 2...32 ** 33...61 ** 62 63 */
                if( bin_index<2 ){
                    histogram[0] += (int)tmp;
                }else if( (bin_index>1)&&(bin_index<33) ){
                    histogram[1] += (int)tmp;
                }else if( (bin_index>32)&&(bin_index<62) ) {
                    histogram[2] += (int)tmp;
                }else{
                    histogram[3] += (int)tmp;
                }

            }

            /*
             * AUTO EXPOSURE CONTROL
             *
             *
             * default integration_time = 512 * 1792 / 48000000 = 19.11466 mS
             * integration time step is 1 line delta = 1792 / 48000000 = 37.3 uS
             */
             uint32_t frame_counter;
             uint16_t integration_time = aec[0].ROI_T_INT_II;
             char exposure_command = ' ';

#define AEC_TOO_LIGHT_THRESH ( (int)((1280*1024 / 4)*0.05) )/* 5% pixels */
#define AEC_TOO_DARK_THRESH ( (int)((1280*1024 / 4)*0.05) ) /* 5% pixels */
#define AEC_GOOD_THRESH ( (int)((1280*1024 / 4)*0.20) )     /* 20% pixels */


             if( (Whites==0xFFFF) && (Blacks<0xFFFF) ){// too light
                exposure_command = '-';
             }else if( (Whites<0xFFFF) && (Blacks==0xFFFF) ){ // too dark
                exposure_command = '+';
             }else if( (Whites==0xFFFF) && (Blacks==0xFFFF) ){// let it bee too light
                exposure_command = '-';// ????
             }else{
                /*
                if( (histogram[0] > AEC_TOO_LIGHT_THRESH) || (Whites > AEC_TOO_LIGHT_THRESH) ){
                    exposure_command = '-';
                }else if( (histogram[3] > AEC_TOO_DARK_THRESH)||(Blacks>AEC_TOO_DARK_THRESH) ){
                    exposure_command = '+';
                }else if( ((histogram[1] < histogram[2])) || ((histogram[1] - histogram[2]) < AEC_GOOD_THRESH) ){
                    exposure_command = '+';
                }else{
                    exposure_command = ' ';
                }
                */
                if( (histogram[0] > AEC_TOO_LIGHT_THRESH) && (histogram[3] < AEC_TOO_DARK_THRESH) ){
                    exposure_command = '-';
                }else if( (histogram[3] > AEC_TOO_DARK_THRESH) && (histogram[0] < AEC_TOO_LIGHT_THRESH) ){
                    exposure_command = '+';
                }else if( (histogram[0] > AEC_TOO_LIGHT_THRESH) && (histogram[3] > AEC_TOO_DARK_THRESH)  ){
                    exposure_command = ' ';
                    // TODO:
                    // надо проверить среднюю яркость
                }else if( ((histogram[1] < histogram[2])) || ((histogram[1] - histogram[2]) < AEC_GOOD_THRESH) ){
                    exposure_command = '+';
                }else{
                    exposure_command = ' ';
                }

             }

            aec[0].FRAME++;

            printf(" --TotalHisto=%6d(%6d)--Total=%6d(%6d)  EXPOSURE(%c) %5d \n",
                histo_summ,
                (int)histo_summ - (int)(1280*1024)/4,
                histo_summ+Blacks+Whites,
                (int)histo_summ + (int)Blacks + (int)Whites - (int)(1280*1024)/4,
                exposure_command,
                integration_time
                  );

             if(  exposure_command != ' ' )
                 if( exposure_command == '+' ){
                     #define EXPOSURE_DELTA_INCR ((uint16_t)24)
                     integration_time += 4;
                     if(aec[0].ROI_T_INT_II<0xFFFF-EXPOSURE_DELTA_INCR){
                        aec[0].ROI_T_INT_II += EXPOSURE_DELTA_INCR;
                        vsensor.increment_integration_time(EXPOSURE_DELTA_INCR);
                     }else{
                         // NEED Gain Correction
                     }
                 }else{
                     #define EXPOSURE_DELTA_DECR ((uint16_t)16)
                     integration_time -= EXPOSURE_DELTA_DECR;
                     if(aec[0].ROI_T_INT_II > EXPOSURE_DELTA_DECR ){
                        aec[0].ROI_T_INT_II -= EXPOSURE_DELTA_DECR;
                        vsensor.decrement_integration_time(EXPOSURE_DELTA_DECR);
                     }else{
                         // NEED Gain Correction
                     }
                 }



            //for(i=7;i>1;i--) memcpy( (void*)&aec[i], (void*)&aec[i-1], sizeof(aec));

            /*
             * Fill nonvideo lines with video data.
             * Copy second line into first and h-2 line into two lowest
             */
        #ifdef FIRST_LINE_OK
             memcpy(ptl_luma_plane, ptl_luma_plane + w, w);
             memcpy(ptl_luma_plane + (w*(h-2)), ptl_luma_plane + (w*(h-3)), w);
             memcpy(ptl_luma_plane + (w*(h-1)), ptl_luma_plane + (w*(h-3)), w);
        #endif

        }
        /////////////////////////////////////////////////////////////////////////////////////////////////

		fillInfo(info, info_mask, (uint8_t*)(buf.m.userptr + w*h), w, w, h/2, g_chroma_value); // data is in chroma planes.

		size_t coded_size=0;

		XDAS_Int8* bs = NULL;

		if (!to_skip) {
			bs = enc.encFrame((XDAS_Int8*)buf.m.userptr, w, h, w, &coded_size);
			to_skip = g_adapt_bitrate_desc[adapt_bitrate_pos].to_skip;
			to_skip = to_skip * fps_divider + fps_divider-1;
//			log() << "Frame encoded " << coded_size;
		} else {
			if (to_skip>0) // to_skip < 0 means no video is sent at all
				to_skip--; 

//			log() << "VRTPK App: Frame skipped";
		}

		cap.putFrame(buf);

        ptrdiff_t info_size = 0;

        if (!to_skip_motion) {
            const uint8_t* cur = encode_frame(&info[0], &info_out[0], w/2, h/2);
            info_size = cur - &info_out[0];
            to_skip_motion = fps_divider-1;
        } else {
            if (to_skip_motion > 0)
                to_skip_motion--;
        }

		Auxiliary::SendTimestamp(buf.timestamp.tv_sec, buf.timestamp.tv_usec);

        // Startup time measure point #2
        LED_ERR(0);

		if (coded_size) {
			Comm::instance().transmit(1, coded_size, (uint8_t*)bs);

//			fwrite((uint8_t*)bs, 1, coded_size, f_dump);
		}

		if (motion_enable && info_size)
			Comm::instance().transmit(2, info_size, &info_out[0]);

		const int buf_size = Comm::instance().getBufferSize();
//		log() << "buffer size " << buf_size;
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
    int i;
	struct timespec clock_cur;
	clock_gettime(CLOCK_MONOTONIC, &clock_cur);
	printf("Teplovisor app starting time: %lu ms\n", clock_cur.tv_nsec/1000000+clock_cur.tv_sec*1000);

    /* initialize e2v aec */
    for(i=0;i<8;i++){
        aec[i].ROI_T_INT_II = 0x0200;
        aec[i].ROI_T_INT_CLK = 0;
        aec[i].ROI_ANA_GAIN = 0;
        aec[i].ROI_DIG_GAIN = 0;
        aec[i].FRAME = 0;
    }

	try {
        LED_ERR(1);

		if (argc < 3) {
			std::cout << "a.out <baud_rate> <flow_control> <output buffer size (PACKETS, NOT BYTES)>\n" << std::endl;
			return 0;
		}

		if (!Comm::instance().open("/dev/ttyS1", atoi(argv[1]), atoi(argv[2])))
			throw ex("Cannot open serial port");

        {
            std::ifstream adapt_bitrate_fs("levels.txt");

            if (adapt_bitrate_fs) {
                for (int i=0; i<sizeof(g_adapt_bitrate_desc)/sizeof(g_adapt_bitrate_desc[0]); i++) {
                    adapt_bitrate_fs >> g_adapt_bitrate_desc[i].switch_down_from_here;
                    adapt_bitrate_fs >> g_adapt_bitrate_desc[i].switch_up_from_here;
                }

                log() << "Adaptive bitrate thresholds : ";
                for (int i=0; i<sizeof(g_adapt_bitrate_desc)/sizeof(g_adapt_bitrate_desc[0]); i++) {
                    log() << "level " << i << " up   : " << g_adapt_bitrate_desc[i].switch_up_from_here;
                    log() << "level " << i << " down : " << g_adapt_bitrate_desc[i].switch_down_from_here;
                }
            }
        }

#if not VIDEO_SENSOR
		Flir flir("/dev/ttyS0");

		ServerCmds cmds(&flir); // flir instance is needed to ask it for versions/serials when host asks it.
#else
		log() << "Init vsensor";

		VSensor vsensor;

		Auxiliary::VideoSensorSettingsData vs_set;

		std::ifstream eeprom(eeprom_filename); // TODO : remove it from here, hide it in ServerCmds ctor/VSensor ctor ?
		if (eeprom) {
			eeprom.seekg(vsensor_config_offset+sizeof(Auxiliary::VideoSensorResolutionData), std::ios_base::beg);
			eeprom.read((char*)&vs_set, sizeof(vs_set));
			
			vsensor.set(vs_set);
		}

		log() << "Vsensor was initialized";

		ServerCmds cmds(&vsensor);
#endif

		Server server(&cmds);

		Comm::instance().setCameraID(cmds.GetCameraID());

#if not VIDEO_SENSOR
		Comm::instance().setCallback(boost::bind(auxiliaryCb, _1, _2, _3, boost::ref(flir)), 3);
#else
		Comm::instance().setCallback(boost::bind(auxiliaryCb, _1, _2, _3, boost::ref(vsensor)), 3);
#endif
		CMEM_init();

		Enc::rman_init();

		if (argc==4)
			g_tx_buffer_size = atoi(argv[3]);

		Comm::instance().setTxBufferSize(g_tx_buffer_size);

		while(1) {
			try {
#if VIDEO_SENSOR
				run(vsensor);
#else
				run();
#endif
			}
			catch (ex& e) {
				log() << e.str();
				g_stop = true;

				if (e.flag() == Ex_TryAnotherConfig) {
					ServerCmds tmp_cmds;
					log() << "Writing default config to eeprom";
					tmp_cmds.SetEncCfg(DefaultEncCfg);
				}
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
