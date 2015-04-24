#pragma once

class VSensor
{
public:
	VSensor();
	
	void set(const Auxiliary::VideoSensorSettingsData&);
    void increment_integration_time(uint16_t);
    void decrement_integration_time(uint16_t);
    void set_integration_time(uint16_t);
    void increment_analog_gain(void);
    void decrement_analog_gain(void);
    void aec_init(int binning);
    void aec_tune(int h, int w, uint8_t *pYplane);
    double aec_time(int i);
    double aec_gain(int i);
    double aec_exposure(int i);
    
    void aec_agc_algorithm_A(uint32_t *H64, int Mediana, int w, int h);
    void aec_agc_algorithm_B(uint32_t *cdf, int b);
    double get_framerate(void);
};
