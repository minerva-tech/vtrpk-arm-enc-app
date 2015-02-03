#include <sys/mount.h>
#include <sys/mman.h>
#include "ext_headers.h"
#include "log_to_file.h"

#include "comm.h"
#include "auxiliary.h"
#include "vsensor.h"

struct reg_val_t {
    uint16_t reg;
    uint16_t val;
};

const reg_val_t idle_state_reg_list[] = {
    {0x3A, 0x0005},//Idle state
	{0x38, 0x028B}
};

const reg_val_t init_reg_list[] = {
	/*Sensor init settings*/
	{0x3A, 0x611f},//PLL -96 MHz
	{0x38, 0x0289},
	{0x3A, 0xdf21},//All clocks from PLL
	{0x38, 0x0288},
	{0x3A, 0x0500},//widht of 1st SIMD vertical band 500 default (1280)
	{0x38, 0x0295},
	{0x3a, 0x0400},//height of 1st SIMD horizontal band 400 default (1024)
	{0x38, 0x0293},
	{0x3A, 0x0a01},//Pattern->video (default)
	{0x38, 0x0287},
    {0x20, 0x6605}//bounching ball removing
    /*End of sensor init settings*/
};

const reg_val_t resolution_half_reg_list[] = {
	/* FPGA reg settings for binning*/
    {0x22, 0x0280},//Picture width in hex (640)
	{0x24, 0x0200},//Picture height in hex (512)
	{0x26, 0x0b80},//Pictire start Hor (3584-640)
	{0x28, 0x000A},//Picture start Vert (522-512)
	{0x34, 0x0e00},//Frame width (3584)
	{0x36, 0x020c}//Frame height (524)
};

const reg_val_t resolution_full_reg_list[] = {
    /*FPGA settings for full resolution*/
    {0x22, 0x0500},//Picture width in hex (1280)
	{0x24, 0x0400},//Picture height in hex (1024)
	{0x26, 0x0500},//Pictire start Hor (1792-1280)
	{0x28, 0x0200},//Picture start Vert (1536-1024)
	{0x34, 0x0700},//Frame width (1792)
	{0x36, 0x0600}//Frame height (1536)
};

reg_val_t binning_reg_list[] = {
    /*Binning enable in sensor*/
	{0x3A, 0x0000},//001- binning enable. 200-div factor =4, 100 =2, 000 =1
	{0x38, 0x028a}
};

reg_val_t compression_correction_reg_list[] = {
	/*10 bit compression // установить 4й бит(enable) убрать/disable (0x0010)
    Pixel correction // установить 5й бит(enable) убрать/disable (0x0020) */
    {0x3A, 0x0000},
    {0x38, 0x028a}
};

const reg_val_t start_sensor_reg_list[] = {
    {0x3A, 0x0006},//Start sensor
	{0x38, 0x028b}
};

static void set_registers(const reg_val_t* reg_list, size_t len)
{
	int fd;

	if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
		log() << "cannot open /dev/mem. su?";
		return;
	}

	void * volatile map_base = mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0x04000000);

	uint8_t* regs = (uint8_t*)map_base;

	for (int i=0; i<len; i++) {
		*(uint16_t*)&regs[reg_list[i].reg] = reg_list[i].val;
//		boost::this_thread::sleep_for(boost::chrono::milliseconds(100)); // TODO : set correct timeout
	}

	munmap((void*)map_base, 1024);
	close(fd);
}

#define set_regs(x) set_registers(x, sizeof(x)/sizeof(x[0]))

VSensor::VSensor()
{
    set_regs(idle_state_reg_list);
    set_regs(binning_reg_list);
}

void VSensor::set(const Auxiliary::VideoSensorSettingsData& settings)
{
    set_regs(idle_state_reg_list);

    if (settings.binning)
        binning_reg_list[0].val = 1 | (settings.binning-1)<<8;
    else
        binning_reg_list[0].val = 0;

    set_regs(binning_reg_list);

    compression_correction_reg_list[0].val = ((settings.ten_bit_compression?1:0) << 4) | ((settings.pixel_correction?1:0) << 5);

    set_regs(compression_correction_reg_list);

    set_regs(start_sensor_reg_list);
}
