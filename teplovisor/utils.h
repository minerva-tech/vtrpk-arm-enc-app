#pragma once

#include "ext_headers.h"
#include "log_to_file.h"

namespace utils {
	union hw_state_t {
		struct {
			uint8_t flash_avail : 1;
			uint8_t flir_avail : 1;
			uint8_t recovery : 1;
		};
		uint8_t data;
	};

	struct hw_status_t {
		hw_state_t last_run = {0};
		hw_state_t this_run = {0};
	};

	enum class streaming_mode_t {
		no_latency = 0,
		transmit_all = 1
	};


	void LED_RXD(int on);
	void LED_ERR(int on);
	
	void set_gpio(int name, int val);
	int get_gpio(int name);
	
	std::string get_version_info();
	bool is_recovery();
	
	void set_streaming_mode(uint8_t mode);
	utils::streaming_mode_t get_streaming_mode();

	void set_hw_status(const hw_status_t&);
	hw_status_t get_hw_status();
} // namespace utils {
