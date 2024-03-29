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

const int AVI_FIRST_SEGMENT_DURATION = 10;
const int AVI_DURATION_MAX_SEC = 30;

const int CONTROL_PORT_BAUDRATE = 4800;

const unsigned char CRC_OFFSET = 0x89;

const char USB_DRIVE_DEV[] = "/dev/sda1";
const char AVI_PATH[] = "/mnt/usb-storage";
const char AVI_FALLBACK_PATH[] = "/tmp";

const char PROC_MOUNTS_PATH[] = "/proc/mounts";

const char eeprom_filename[] = "/sys/devices/platform/spi_davinci.4/spi4.0/eeprom";
const char version_info_filename[] = "/etc/versioninfo";

enum {
	cam_id_config_offset = 0,
	md_config_offset = 1,
	encoder_config_offset = 1024,
	roi_config_offset = 50*1024,
	md_config_max_size = encoder_config_offset - md_config_offset,
	encoder_config_max_size = roi_config_offset - encoder_config_offset,
	roi_config_max_size = 50*1024,
	streaming_mode_offset = roi_config_offset + roi_config_max_size,
	streaming_mode_size = 1,
	hw_status_offset = streaming_mode_offset + streaming_mode_size,
	hw_status_size = 2
};
