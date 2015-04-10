#pragma once

class VSensor
{
public:
	VSensor();
	
	void set(const Auxiliary::VideoSensorSettingsData&);
    void increment_integration_time(uint16_t);
    void decrement_integration_time(uint16_t);
};
