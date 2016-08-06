#pragma once

const int SRC_WIDTH  = 320;
const int SRC_HEIGHT = 256;

const int TARGET_WIDTH  = 320;
//const int TARGET_HEIGHT = 128;
const int TARGET_HEIGHT = 256;

const int BITRATE_BIAS = 0;
const int BITRATE_STEP = 2000;
const int BITRATE_MIN  = 1000;
const int BITRATE_MAX  =64000;

const int FPS_MAX = 8;
const int AVI_LENGTH_MAX = 200;

const int AVI_DURATION_MAX_SEC = 30;

const int CONTROL_PORT_BAUDRATE = 9600;

const char AVI_PATH[] = "/mnt/ntfs";

const char eeprom_filename[] = "/sys/devices/platform/spi_davinci.4/spi4.0/eeprom";
const char version_info_filename[] = "/etc/versioninfo";

enum {
	cam_id_config_offset = 0,
	md_config_offset = 1,
	encoder_config_offset = 1024,
	roi_config_offset = 50*1024,
	md_config_max_size = encoder_config_offset - md_config_offset,
	encoder_config_max_size = roi_config_offset - encoder_config_offset,
	roi_config_max_size = 50*1024
};
