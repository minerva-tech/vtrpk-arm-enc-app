#include "utils.h"

#include "defines.h"

namespace utils {

void LED_RXD(int on)
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

void LED_ERR(int on)
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

void set_gpio(int num, int val)
{
	FILE* fexport = fopen("/sys/class/gpio/export", "wt");
	fprintf(fexport, "%i\n", num);
	fclose(fexport);

	char name[] = "/sys/class/gpio/gpioXXXX/direction";
	sprintf(name, "/sys/class/gpio/gpio%i/direction", num);
	FILE* fdir = fopen(name, "wt");
	fprintf(fdir, "out\n");
	fclose(fdir);

	sprintf(name, "/sys/class/gpio/gpio%i/value", num);
	FILE* fval = fopen(name, "wb");
	fprintf(fval, "%i\n", val);
	fclose(fval);
}

int get_gpio(int num)
{
	FILE* fexport = fopen("/sys/class/gpio/export", "wt");
	fprintf(fexport, "%i\n", num);
	fclose(fexport);

	char name[] = "/sys/class/gpio/gpioXXXX/direction";
	sprintf(name, "/sys/class/gpio/gpio%i/direction", num);
	FILE* fdir = fopen(name, "wt");
	fprintf(fdir, "in\n");
	fclose(fdir);

	sprintf(name, "/sys/class/gpio/gpio%i/value", num);
	FILE* fval = fopen(name, "rb");
	int val = 0;
	fscanf(fval, "%i", &val);
	fclose(fval);
	
	return val;
}

std::string get_version_info() 
{
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

	return ver;
}

bool is_recovery() 
{
	std::string ver = get_version_info();
	return ver[0] == 'R';
}

void set_streaming_mode(uint8_t mode)
{
		std::ofstream eeprom(eeprom_filename, std::ios_base::out | std::ios_base::in | std::ios_base::binary);
		if (!eeprom)
			return;

		eeprom.seekp(streaming_mode_offset, std::ios_base::beg);

		log() << "Writing mode " << (int)mode;

		eeprom.write((char*)&mode, sizeof(mode));
}

utils::streaming_mode_t get_streaming_mode()
{
		std::ifstream eeprom(eeprom_filename, std::ios_base::in | std::ios_base::binary);
		if (!eeprom)
			return utils::streaming_mode_t::no_latency;

		eeprom.seekg(streaming_mode_offset, std::ios_base::beg);

		uint8_t mode = 0xBA;

		eeprom.read((char*)&mode, sizeof(mode));

		log() << "Read mode " << (int)mode;

		return static_cast<utils::streaming_mode_t>(mode);
}

void set_hw_status(const hw_status_t& v)
{
		std::ofstream eeprom(eeprom_filename, std::ios_base::out | std::ios_base::in | std::ios_base::binary);
		if (!eeprom)
			return;

		eeprom.seekp(hw_status_offset, std::ios_base::beg);

		eeprom.write((char*)&v, sizeof(v));
}

hw_status_t get_hw_status()
{
		std::ifstream eeprom(eeprom_filename, std::ios_base::in | std::ios_base::binary);
		if (!eeprom)
			return hw_status_t();

		eeprom.seekg(hw_status_offset, std::ios_base::beg);

		hw_status_t v;

		eeprom.read((char*)&v, sizeof(v));

		return v;
}

} // namespace utils
