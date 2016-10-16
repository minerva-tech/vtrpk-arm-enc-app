#pragma once

#include "ext_headers.h"
#include "log_to_file.h"

namespace utils {
	union hw_state_t {
		struct {
			uint8_t flash_avail : 1 ;
			uint8_t flir_avail : 1;
		};
		uint8_t data;
	};
	struct hw_status_t {
		hw_state_t last_run = {0};
		hw_state_t this_run = {0};
	};
	
	void LED_RXD(int on);
	void LED_ERR(int on);
	
	void set_gpio(int name, int val);
	int get_gpio(int name);
	
	std::string get_version_info();
	
	void set_streaming_mode(uint8_t mode);
	uint8_t get_streaming_mode();

	void set_hw_status(const hw_status_t&);
	hw_status_t get_hw_status();
} // namespace utils {
