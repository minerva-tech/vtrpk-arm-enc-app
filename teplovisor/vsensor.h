#pragma once

class set_gain;
class VSensor
{
public:
	VSensor();
	
	void set(const Auxiliary::VideoSensorSettingsData&);
    /* aec() - auto exposure control: tune integration time  and gain of the sensor */
    void aec(int h, int w, uint8_t *pYplane);
    double get_framerate(void);
    
    void set_state(char type, int value);
    
private:
    void increment_integration_time(uint16_t);
    void decrement_integration_time(uint16_t);
    void set_integration_time(uint16_t);
    void increment_analog_gain(void);
    void decrement_analog_gain(void);
    
    void aec_init(int binning);
    
    double aec_time(int i);
    double aec_gain(int i);
    
    void set_gain(int analog, int digital);
    void set_time(int lines, int fraction);
    
    void aec_agc_algorithm_A(uint32_t *H64, int Mediana, int w, int h);// 1st algo
    bool aec_agc_algorithm_AA(uint32_t *H64, uint32_t *CDF, int Mediana, int w, int h);
    bool aec_agc_algorithm_AAA(uint32_t *H64, uint32_t *CDF, int Mediana, int w, int h);
    bool aec_agc_algorithm_B(uint32_t *cdf, int Ymean, int b, float smoothness);// Francecam (FCam) algo
    bool aec_agc_algorithm_C(uint32_t *cdf, int Mediana, int b, float smoothness);
    void aec_agc_set(float Gain, float Exposure, int command);// writes to sensor registers appropriate values
    
    void exposure_set(float exposure);
};
