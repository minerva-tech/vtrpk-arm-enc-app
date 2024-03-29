#include <sys/mman.h>

#include "ext_headers.h"
#include "log_to_file.h"
#include "exception.h"
#include "comm.h"
#include "defines.h"
#include "proto.h"
#include "flir.h"
#if VIDEO_SENSOR
#include "vsensor.h"
#endif

#include "servercmds.h"

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

void ServerCmds::Stop()
{
	g_stop = true;
	BufferClear();
}

std::string ServerCmds::GetEncCfg() 
{
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

std::string ServerCmds::GetMDCfg() 
{
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
    log() << "MD config size: " << size;
	return str;
}

std::vector<uint8_t> ServerCmds::GetROI() 
{
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

std::string ServerCmds::GetVSensorConfig() 
{
	log() << "Read eeprom for vsensor settings"; 
#if VIDEO_SENSOR    
    std::ifstream eeprom(eeprom_filename);
	if (!eeprom)
		return std::string();
    
    /* Durty hack. STEP 1.
	eeprom.seekg(vsensor_config_offset, std::ios_base::beg);

	uint32_t size = 0;
	eeprom.read((char*)&size, sizeof(size));
    

    printf(" !!! Vsensor settings size: %08x\n",size);
		
    if (size > vsensor_settings_max_size){
        printf(" ERROR: GetVSensorConfig() size too big\n");
		return std::string();
    }
    */
    
    /* DURTY HACK. STEP 2 */
    #if 1
    
    uint32_t size = 20;
    eeprom.seekg(vsensor_config_offset, std::ios_base::beg);
	std::string str;
    str.resize(size);
    //str[0] = 'a';
    eeprom.read(&str[0], str.size()*sizeof(str[0]));
    printf(" %8X %8X %8X %8X %8X\n", str[0], str[1], str[2], str[3], str[4]);
    printf(" %8X %8X %8X %8X %8X\n", str[5], str[6], str[7], str[8], str[9]);
    printf(" %8X %8X %8X %8X %8X\n", str[10], str[11], str[12], str[13], str[14]);
    
    return str;
    
    #else
    
    /* Best Test Code */
    char teststr[] = "Jopa Hui Pizda 0123456789";
    log() << "    ___ Vsensor settings " << teststr; 
    return teststr;
    
    #endif
        
#endif // #if VIDEO_SENSOR
}

std::string ServerCmds::GetVersionInfo() 
{
	std::string ver;

    log() << "get version info called";

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

		ver += "\n";
	}


#if not VIDEO_SENSOR
	if (m_flir) {
		char t[128];
		sprintf(t, "\"Teplovisor\" build\t\t%u (%u)\n", (unsigned long)&__BUILD_NUMBER, (unsigned long)&__BUILD_DATE); ver += t;

		sprintf(t, "FPGA revision\t\t%x\n", GetRegister(0x2e)); ver += t;

		uint32_t flir_data[4];
		m_flir->get_serials(flir_data);
		sprintf(t, "Camera serial number\t%u\n", flir_data[0]); ver += t;
		sprintf(t, "Sensor serial number\t%u\n", flir_data[1]); ver += t;
		sprintf(t, "Camera SW version\t\t%u\n", flir_data[2]); ver += t;
		sprintf(t, "Camera HW version\t\t%u\n", flir_data[3]); ver += t;
	}
#else
    char t[128];
	sprintf(t, "\"Videosensor\" build\t\t%u (%u)\n", (unsigned long)&__BUILD_NUMBER, (unsigned long)&__BUILD_DATE); ver += t;
	sprintf(t, "FPGA revision\t\t%x\n", GetRegister(0x2e)); ver += t;
#endif

    log() << ver;

	return ver;
}

int ServerCmds::GetStreamsEnableFlag()
{
	std::ifstream eeprom(eeprom_filename);
	if (!eeprom)
		return 0;

#if VIDEO_SENSOR

	eeprom.seekg(streams_enable_offset, std::ios_base::beg);

	uint8_t flags;
	eeprom >> flags;

    log() << "Streams enable flags : " << (int)flags;

    if (flags == -1) // uninitalized eeprom probably. And moreover, host code will think that we didn't connect with device.
        flags = 0;

	return flags;
	
#else
	return 0;
#endif
}

uint8_t ServerCmds::GetCameraID() 
{
	std::ifstream eeprom(eeprom_filename, std::ios_base::in | std::ios_base::binary);
	if (!eeprom)
		return 0;

	eeprom.seekg(cam_id_config_offset, std::ios_base::beg);

	uint8_t id;
	eeprom.read((char*)&id, sizeof(id));

    log() << "My camera ID taken from eeprom: " << (int)id;

	id = std::min((uint8_t)3, id);

	return id;
}

uint16_t ServerCmds::GetRegister(uint8_t addr) 
{
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

Auxiliary::VideoSensorParameters ServerCmds::GetVideoSensorParameters()
{
#if VIDEO_SENSOR
	Auxiliary::VideoSensorParameters params;
	   log() << "Vsensor settings parameters called";
           
	std::ifstream eeprom(eeprom_filename);
	if (eeprom) {
		eeprom.seekg(vsensor_config_offset, std::ios_base::beg);
		eeprom.read((char*)&params.res, sizeof(params.res));
		eeprom.read((char*)&params.set, sizeof(params.set));
	}
	
	return params;
#else
	return Auxiliary::VideoSensorParameters();
#endif
}

void ServerCmds::SetEncCfg(const std::string& str) 
{
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

void ServerCmds::SetMDCfg(const std::string& str) 
{
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

void ServerCmds::SetROI(const std::vector<uint8_t>& str) 
{
	if (str.size() > roi_config_max_size)
		return;

	std::ofstream eeprom(eeprom_filename);
	if (!eeprom)
		return;

	eeprom.seekp(roi_config_offset, std::ios_base::beg);

	const int size = str.size();
	eeprom.write((char*)&size, sizeof(size));

	log() << "roi size + " << sizeof(size);

	eeprom.write((char*)&str[0], str.size()*sizeof(str[0]));

	log() << "roi size + " << str.size()*sizeof(str[0]);
}

void ServerCmds::SetVersionInfo(const std::string& str)
{
	// nothing to do.
}

void ServerCmds::SetStreamsEnableFlag(int streams_enable)
{
#if VIDEO_SENSOR
	std::ofstream eeprom(eeprom_filename);
	if (!eeprom)
		return;

	eeprom.seekp(streams_enable_offset, std::ios_base::beg);

	eeprom << (uint8_t)streams_enable;

	log() << "Motion detector enable : " << (int)streams_enable;
#endif
}

void ServerCmds::SetCameraID(uint8_t id) {
	std::ofstream eeprom(eeprom_filename, std::ios_base::out | std::ios_base::in | std::ios_base::binary);
	if (!eeprom)
		return;

	eeprom.seekp(cam_id_config_offset, std::ios_base::beg);

	eeprom.write((char*)&id, sizeof(id));

	Comm::instance().setCameraID(id);

	log() << "New camera ID received : " << (int)id;
}

void ServerCmds::BufferClear()
{
    Comm::instance().drop_unsent();
}

void ServerCmds::SetBitrate(int bitrate)
{
   // g_bitrate = bitrate;
    g_bitrate = bitrate * 100;
	g_change_bitrate = true;
	g_stop = false;

	Comm::instance().allowTransmission(true);
}
