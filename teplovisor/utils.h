#pragma once

#include "ext_headers.h"
#include "log_to_file.h"

namespace utils {
void LED_RXD(int on);
void LED_ERR(int on);
void set_gpio(int name, int val);
int get_gpio(int name);
std::string get_version_info();
void set_streaming_mode(uint8_t mode);
uint8_t get_streaming_mode();
} // namespace utils {
