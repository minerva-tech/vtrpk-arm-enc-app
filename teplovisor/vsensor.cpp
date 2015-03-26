#include <sys/mount.h>
#include <sys/mman.h>
#include "ext_headers.h"
#include "log_to_file.h"

#include "comm.h"
#include "auxiliary.h"
#include "vsensor.h"

const uint16_t binning_reg_list[][2] = {
	{0x22, 0x0280},
	{0x24, 0x0200},
	{0x26, 0x0b80},
	{0x28, 0x000A},
	{0x34, 0x0e00},
	{0x36, 0x020c},
	{0x3A, 0x0005},
	{0x38, 0x028B},
	{0x3A, 0x611f},
	{0x38, 0x0289},
	{0x3A, 0xdf21},
	{0x38, 0x0288},
	{0x3A, 0x0500},
	{0x38, 0x0295},
	{0x3a, 0x0400},
	{0x38, 0x0293},
	{0x3A, 0x0a01},
	{0x38, 0x0287},
	{0x3A, 0x0201},
	{0x38, 0x028a},
	{0x3A, 0x0006},
	{0x38, 0x028b},
	{0x20, 0x6605}
};

VSensor::VSensor()
{
	int fd;

	if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
		log() << "cannot open /dev/mem. su?";
		return;
	}

	void * volatile map_base = mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0x04000000);

	uint8_t* regs = (uint8_t*)map_base;

	for (int i=0; i<sizeof(binning_reg_list)/sizeof(binning_reg_list[0]); i++) {
		*(uint16_t*)&regs[binning_reg_list[i][0]] = binning_reg_list[i][1];
		boost::this_thread::sleep_for(boost::chrono::milliseconds(100)); // TODO : set correct timeout
	}

	munmap((void*)map_base, 1024);
	close(fd);
}

void VSensor::set(const Auxiliary::VideoSensorSettingsData& settings)
{
	log() << "Video sensor parameters : ";
	log() << " binning : " << settings.binning;
	log() << " fps divider : " << settings.fps_divider;
	log() << " pixel correction : " << settings.pixel_correction;
	log() << " ten bit compression : " << settings.ten_bit_compression;
}
