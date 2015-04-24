#include <sys/mount.h>
#include <sys/mman.h>
#include "ext_headers.h"
#include "log_to_file.h"

#include "comm.h"
#include "auxiliary.h"
#include "vsensor.h"

#define DEFAULT_ANALOG_GAIN ((uint16_t)0)

struct reg_val_t {
    uint16_t reg;
    uint16_t val;
};

const reg_val_t idle_state_reg_list[] = {
    {0x3A, 0x0005},//Idle state
	{0x38, 0x028B}
};

struct aec_parameters_t{
    uint16_t ROI_T_INT_II;   //
    uint16_t ROI_T_INT_CLK;  //
    uint16_t ROI_ANA_GAIN;   //
    uint16_t ROI_DIG_GAIN;   //
    uint64_t FRAME;          // raw frame counter
    uint32_t BINNING;
};

static aec_parameters_t aec[8];

const reg_val_t init_reg_list[] = {
	/*Sensor init settings*/
	{0x3A, 0x631f},//PLL -611f 96 MHz, 631f - 48
	{0x38, 0x0289},
	{0x3A, 0xdf21},//All clocks from PLL
	{0x38, 0x0288},
	{0x3A, 0x0500},//widht of 1st SIMD vertical band 500 default (1280)
	{0x38, 0x0295},
	{0x3a, 0x0400-3},//height of 1st SIMD horizontal band 400 default (1024)
	{0x38, 0x0293},
	{0x3A, 0x0a01},//Pattern->video (default)
	{0x38, 0x0287},
    {0x3A, 0x00C0},// Reg.0A: enable Context and Histogramm
    {0x38, 0x028A},
    {0x3A, 0x0200},// Reg.0x0E: roi1_t_init_II.
    {0x38, 0x028e},
    {0x3A, 0x0000|(DEFAULT_ANALOG_GAIN<<8)},// Reg.0x11: roi1_gain.
    {0x38, 0x0280|0x0011}
   // {0x20, 0x6605}//bounching ball removing
    /*End of sensor init settings*/
};

const reg_val_t resolution_half_reg_list[] = {
	/* FPGA reg settings for binning*/
    {0x18, 0x0004},//2^20 / number of pixels
    {0x22, 0x0280},//Picture width in hex (640)
	{0x24, 0x0200},//Picture height in hex (512)
	{0x26, 0x0b80},//Pictire start Hor (3584-640)
	{0x28, 0x000A},//Picture start Vert (522-512)
	{0x34, 0x0e00},//Frame width (3584)
	{0x36, 0x020c}//Frame height (524)
};

const reg_val_t resolution_full_reg_list[] = {
    /*FPGA settings for full resolution*/
    {0x18, 0x0001},//2^20 / number of pixels
    {0x22, 0x0500},//Picture width in hex (1280)
	{0x24, 0x0400},//Picture height in hex (1024)
	{0x26, 0x0200},//Pictire start Hor (1792-1280)
	{0x28, 0x0011},//Picture start Vert (1041-1024) 1536
	{0x34, 0x0700},//Frame width (1792)
	{0x36, 0x0411}//Frame height (1041) 1536
};

reg_val_t bcc_reg_list[] = {
	/*10 bit compression // установить 4й бит(enable) убрать/disable (0x0010)
    Pixel correction // установить 5й бит(enable) убрать/disable (0x0020)
    Binning enable in sensor
	001- binning enable. 200-div factor =4, 100 =2, 000 =1
    */
    {0x3A, 0x00C0},// Reg.0A: enable Context and Histogramm
    {0x38, 0x028a}
};

const reg_val_t start_sensor_reg_list[] = {
    {0x3A, 0x0006},//Start sensor
	{0x38, 0x028b}
};

reg_val_t aec_reg_list[] = {
    {0x3A, 0x0200},// Reg.0x0E: roi1_t_init_II.
    {0x38, 0x028e}
};

reg_val_t agc_reg_list[] = {
    {0x3A, 0x0000|(DEFAULT_ANALOG_GAIN<<8)},// Reg.0x0E: roi1_t_init_II.
    {0x38, 0x0280|0x0011}
};

static const double digital_gain_step = (15.875/256.0);
static const double analog_gain_table[8] = { 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 8.0 };

static void set_registers(const reg_val_t* reg_list, size_t len)
{
	int fd;

	if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
		log() << "cannot open /dev/mem. su?";
		return;
	}

	void * volatile map_base = mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0x04000000);

	uint8_t* regs = (uint8_t*)map_base;

	for (int i=0; i<len; i++) {
		*(uint16_t*)&regs[reg_list[i].reg] = reg_list[i].val;
		boost::this_thread::sleep_for(boost::chrono::milliseconds(100)); // TODO : set correct timeout
	}

	munmap((void*)map_base, 1024);
	close(fd);
}

#define set_regs(x) set_registers(x, sizeof(x)/sizeof(x[0]))
//#define set_regs(x) ;

VSensor::VSensor()
{
    set_regs(idle_state_reg_list);
    set_regs(init_reg_list);
//    set_regs(start_sensor_reg_list);
}

void VSensor::set(const Auxiliary::VideoSensorSettingsData& settings)
{
    set_regs(idle_state_reg_list);

    log() << "Binning: " << (int)settings.binning << " compression: "
            << (int)settings.ten_bit_compression << " correction: " << (int)settings.pixel_correction;

    if (settings.binning) {
        log() << "binning is on";
        set_regs(resolution_half_reg_list);
        bcc_reg_list[0].val |= (0x0001);// reg 0x0A bit 0
    } else {
        log() << "binning is off";
        set_regs(resolution_full_reg_list);
        bcc_reg_list[0].val &= ~(0x0001);// reg 0x0A bit 0
    }

//    set_regs(binning_reg_list);

    bcc_reg_list[0].val |= ((settings.ten_bit_compression?1:0) << 4) | ((settings.pixel_correction?1:0) << 5);

    log() << "bcc reg : " << bcc_reg_list[0].val;

//    set_regs(compression_correction_reg_list);
    set_regs(bcc_reg_list);

    set_regs(start_sensor_reg_list);

    /* initialize e2v aec */
    aec_init((int)settings.binning);
}

void VSensor::increment_integration_time(uint16_t delta)
{
    aec_reg_list[0].val += delta;
    set_regs(aec_reg_list);
}

void VSensor::decrement_integration_time(uint16_t delta)
{
    aec_reg_list[0].val -= delta;
    set_regs(aec_reg_list);
}

void VSensor::set_integration_time(uint16_t time)
{
    aec_reg_list[0].val = time;
    set_regs(aec_reg_list);
}

void VSensor::increment_analog_gain(void)
{
    uint16_t x = (agc_reg_list[0].val & 0xFF00)>>8;

    if(x<5) x++;// to avoid wrong value

    // keep digital gain unchanged
    agc_reg_list[0].val = agc_reg_list[0].val & 0x00FF;
    agc_reg_list[0].val = agc_reg_list[0].val | ((x<<8)&0xFF00);

    set_regs(agc_reg_list);
}

void VSensor::decrement_analog_gain(void)
{
    uint16_t x = (agc_reg_list[0].val & 0xFF00)>>8;

    if(x>0) x--;// to avoid wrong value

    // keep digital gain unchanged
    agc_reg_list[0].val = agc_reg_list[0].val & 0x00FF;
    agc_reg_list[0].val = agc_reg_list[0].val | ((x<<8)&0xFF00);

    set_regs(agc_reg_list);
}
/*
void VSensor::decrement_integration_time(float N_times)
{
    aec_reg_list[0].val = (uint16_t)((float)aec_reg_list[0].val * N_times);
    set_regs(aec_reg_list);
}
*/
void VSensor::aec_init(int binning)
{
    assert( binning==0 || binning==1 || binning==2 || binning==4 );
    
    int i;
    for(i=0; i<8; i++) {
        aec[i].ROI_T_INT_II = 0x0200;
        aec[i].ROI_T_INT_CLK = 0;
        aec[i].ROI_ANA_GAIN = DEFAULT_ANALOG_GAIN;
        aec[i].ROI_DIG_GAIN = 0;
        aec[i].FRAME = 0;
        aec[i].BINNING = (uint32_t)binning;
    }
}

//
// aec_gain(int i)
// i - index of frame: i=0 this frame(0), i=1 previouse frame(-1) etc.
double VSensor::aec_gain(int i)
{
    assert( aec[0].ROI_ANA_GAIN<7 );
    assert( i<8 );
    
    double binning_gain = 1.0;
    if(0 == aec[i].BINNING) binning_gain = 1.0;
    else binning_gain = (double)aec[i].BINNING;
    return analog_gain_table[aec[i].ROI_ANA_GAIN] * 
            (double)(aec[i].ROI_DIG_GAIN+1) * digital_gain_step * binning_gain;
}

#define EV76C661_CLK_CTRL (24000000.0) /* defined by sensor's pll configuration */
#define EV76C661_LINE_LENGTH (112.0 * 8.0) /* register 0x04 content */
#define EV76C661_MULT_FACTOR (8.0) /* t_int_clk_mult_factor, register 0x0A */
#define EV76C661_MIN_TIME (0.0000015) /* 1.5 uS */

//
// aec_gain(int i)
// i - index of frame: i=0 this frame(0), i=1 previouse frame(-1) etc.
double VSensor::aec_time(int i)
{
    
    double time;
    
    assert( i<8 );
    
    time = EV76C661_LINE_LENGTH * aec[i].ROI_T_INT_II;
    time = time + (EV76C661_MULT_FACTOR * aec[i].ROI_T_INT_CLK);
    time = time / EV76C661_CLK_CTRL;
    
    if( time < EV76C661_MIN_TIME ) time = EV76C661_MIN_TIME;
    
    return time;
}

double VSensor::aec_exposure(int i)
{
   return aec_time(i) * aec_gain(i); 
}

// get_histogram(const void *luma_plane, uint32_t *histogram, int w, int h)
// w - image width
// h - image height
static void get_histogram(void *luma_plane, uint32_t *histogram, int w, int h)
{
    uint8_t* ptl_luma_plane = (uint8_t*)luma_plane;
#ifdef FIRST_LINE_OK
    uint8_t* ptr_header_line;
#endif    
    uint8_t* ptr_footer_line;
    uint8_t* ptr_histogram_line;
    uint16_t* ptr_histogram;
    
    uint16_t Blacks,Whites;
    uint16_t tmp;
    uint32_t x,y;
    uint32_t *H64 = histogram;

    int i,j,oe,ue,Mediana;

    assert(luma_plane);
    assert(histogram);
    assert(1280==w);
    assert(1024==h);
    
    // Вот ТУТ, из (uint8_t*)buf.m.userptr надо доставать 1ую и 2 последние строки.
    // Подробное, с излишним количеством локальных переменных, описание
    // сделано исключительно для тупящих.
#ifdef FIRST_LINE_OK
    ptr_header_line = ptl_luma_plane;
    ptr_footer_line = ptl_luma_plane + (sizeof(uint8_t) * w * (h-1));
    ptr_histogram_line = ptl_luma_plane + (sizeof(uint8_t) * w * (h-2));
    ptr_histogram = (uint16_t*)ptr_histogram_line;
    printf(" -- Header -- w=%d h=%d\n", w, h);
    for(i=0; i<25; i++) printf( " %02X",ptr_header_line[i]);
    printf("\n\n");
    printf("ROI analog gain: %d\n", ptr_header_line[14]);
    printf("ROI global digital gain: %d\n", ptr_header_line[15]);
#else
    i = w==1280?2:1;
    ptr_footer_line = ptl_luma_plane + (sizeof(uint8_t) * w * (h-i));
    i = w==1280?3:2;
    ptr_histogram_line = ptl_luma_plane + (sizeof(uint8_t) * w * (h-i));
    ptr_histogram = (uint16_t*)ptr_histogram_line;
#endif

    printf(" -- Footer -- w=%d h=%d\n", w, h);
    for(i=0; i<5; i++) printf(" %02X",ptr_footer_line[0]);
    printf("\n");
    printf("Error flags: 0x%02X\n",ptr_footer_line[0]);
    tmp = ((uint16_t)ptr_footer_line[1]<<8 | (uint16_t)ptr_footer_line[2]);
    printf("0x00 pixels: %d\n",tmp);
    Blacks = tmp;
    tmp = ((uint16_t)ptr_footer_line[3]<<8 | (uint16_t)ptr_footer_line[4]);
    printf("0xFF pixels: %d\n",tmp);
    Whites = tmp;
    oe = 0;
    ue = 0;
    for(i=0; i<(64*2); i+=2) {
        int bin_index=i/2;
        uint32_t bin=0;
        /* use all 4 gistorgamms */
        tmp = ((uint16_t)ptr_histogram_line[i+0+(128*0)]<<8 | (uint16_t)ptr_histogram_line[i+1+(128*0)]);
        if(tmp==0xFFFF && bin_index==0)oe++;
        if(tmp==0xFFFF && bin_index==63)ue++;
        bin += (int)tmp;
        tmp = ((uint16_t)ptr_histogram_line[i+0+(128*1)]<<8 | (uint16_t)ptr_histogram_line[i+1+(128*1)]);
        if(tmp==0xFFFF && bin_index==0)oe++;
        if(tmp==0xFFFF && bin_index==63)ue++;
        bin += (int)tmp;
        tmp = ((uint16_t)ptr_histogram_line[i+0+(128*2)]<<8 | (uint16_t)ptr_histogram_line[i+1+(128*2)]);
        if(tmp==0xFFFF && bin_index==0)oe++;
        if(tmp==0xFFFF && bin_index==63)ue++;
        bin += (int)tmp;
        tmp = ((uint16_t)ptr_histogram_line[i+0+(128*3)]<<8 | (uint16_t)ptr_histogram_line[i+1+(128*3)]);
        if(tmp==0xFFFF && bin_index==0)oe++;
        if(tmp==0xFFFF && bin_index==63)ue++;
        bin += (int)tmp;
        H64[63-bin_index] = bin;
    }
    /* Check e2v histogramm overflow */
    x=0;
    y=0;
    for(i=0; i<64; i++) {
        x += H64[i];
        if(i!=0 && i!=63) y += H64[i];
    }
    if( ((w*h)-x)>((w*h)*0.1) ) { //If Histogramm error bigger than 10%..
        if(oe!=0 && ue!=0) {
            // Histogram has wrong black and white sides
            // Decrese gain
            // Rough histogramm correction
            H64[0]  = (w*h - y)/2;
            H64[63] = (w*h - y)/2;
        } else if(oe!=0 && ue==0) {
            // Histogram has wrong white side
            // Decrease gain or exposure time
            // Correct histogram:
            H64[63] = w*h - y - H64[0];
        } else if(oe==0 && ue!=0) {
            // Histogram has wrong black side
            // Increase gain or exposure time
            // Correct histogram:
            H64[0] = w*h - y - H64[63];
        } else {
            // Histogramm may be ok if other bins has no overflow
            // TODO: correct histogram
        }
    } else {
        // Histogram is ok
        // TODO: correct histogram
    } 
    return;
}

double VSensor::get_framerate(void)
{
    double Treadout, Texposure;
    
    /* page 91 EV76C661 datasheet */
    Treadout = (2 + 3 + 6 + 1024 + 8 + 1) * EV76C661_LINE_LENGTH / EV76C661_CLK_CTRL ; /* 1044 */
    Texposure = aec_time(0);
    
    if( Treadout > Texposure ) return 1.0/Treadout;
    else return 1.0/Texposure;
}

void VSensor::aec_agc_algorithm_A(uint32_t *H64, int Mediana, int w, int h)
{
    char exposure_command = ' ';
    static int need_gain_correction = 0;
    uint32_t h4[4] = {0,0,0,0};
    int i;
    
#define Y_TOP_LIM (155)
#define Y_BOT_LIM (135)
#define EXPOSURE_DELTA_INCR ((uint16_t)(4*6))
#define EXPOSURE_DELTA_DECR ((uint16_t)(4*4))
#define EXPOSURE_TOP_LIMIT (3200)

    for(i=0; i<64; i++) {
        /* pseudohistogram for aec-agc rough correction algorithm */
        if( i<2 ) {
            h4[0] += H64[i];
        } else if( (i>1)&&(i<33) ) {
            h4[1] += H64[i];
        } else if( (i>32)&&(i<62) ) {
            h4[2] += H64[i];
        } else {
            h4[3] += H64[i];
        }
    }
/*
    if( ((h4[1]+h4[2]) > 0.75*(w*h)) && h4[0] < 0.15*(w*h) && h4[3] < 0.15*(w*h) ){
        if(Mediana<Y_BOT_LIM)
            exposure_command = '+';
        else if(Mediana>Y_TOP_LIM)
            exposure_command = '-';
        else
            exposure_command = ' ';
    }else{
        exposure_command = ' ';
    }
*/
        if(Mediana<Y_BOT_LIM)
            exposure_command = '+';
        else if(Mediana>Y_TOP_LIM)
            exposure_command = '-';
        else
            exposure_command = ' ';
    
    /*
     * для разных analog_gain нужны разные дельты времени накопления (шаги изменения экспозиции)
     * для ana_gain=1 24|16,
     */

    if( (exposure_command != ' ') ) {
        if( exposure_command == '+' ) {
            // TODO: do limit expoture(top and bottom)
            // bottom limit is 8 frame per second
            // default integration_time = 512 * 1792 / 48000000 = 19.11466 mS
            // integration time step is 1 line delta = 1792 / 48000000 = 37.3 uS
            // Longest exposure limit is about 3200 (0x0C80)
            if( aec[0].ROI_T_INT_II < EXPOSURE_TOP_LIMIT ) {
                /*
                uint16_t delta = EXPOSURE_TOP_LIMIT - aec[0].ROI_T_INT_II;
                if(delta>1) delta = (uint16_t)((float)delta*0.125);
                */
                uint16_t delta;

                if( (Y_BOT_LIM-Mediana)<((Y_TOP_LIM-Y_BOT_LIM)/2) ) delta = 1;
                else delta = EXPOSURE_DELTA_INCR;

                aec[0].ROI_T_INT_II += delta;
                increment_integration_time(delta);
                need_gain_correction = 0;
            } else {
                // TODO: NEED Gain Correction (increase gain)
                
                if(aec[0].ROI_ANA_GAIN < 6 ){
                    aec[0].ROI_ANA_GAIN++;
                    aec[0].ROI_T_INT_II = 512;
                    increment_analog_gain();
                    set_integration_time(512);
                }
                
                need_gain_correction = 1;
            }
        } else {
            // shortest exposure limited by zero
            if( aec[0].ROI_T_INT_II > 1 ) {
                /*
                uint16_t delta = (uint16_t)((float)aec[0].ROI_T_INT_II*0.125);
                */
                uint16_t delta;
                if( (Mediana-Y_TOP_LIM)<((Y_TOP_LIM-Y_BOT_LIM)/2) ) delta = 1;
                else delta = EXPOSURE_DELTA_INCR;

                if( !(aec[0].ROI_T_INT_II>delta) ) delta=1;

                aec[0].ROI_T_INT_II -= delta;

                decrement_integration_time(delta);
                need_gain_correction = 0;
            } else {
                // TODO: NEED Gain Correction (decrease gain)
                
                if(aec[0].ROI_ANA_GAIN > 0){
                   aec[0].ROI_ANA_GAIN--;
                   aec[0].ROI_T_INT_II = 512;
                   decrement_analog_gain();
                   set_integration_time(512);
                }
            
                need_gain_correction = 1;
            }
        }
    } else {
        need_gain_correction=0;
        /* If histogramm looks like :...: then decrease gain */
        // ((h4[1]+h4[2]) > 0.75*(w*h)) && h4[0] < 0.15*(w*h) && h4[3] < 0.15*(w*h)
        if( (h4[1]+h4[2]) < (0.75*(w*h)) ) {
            if( h4[0] > 0.1*(w*h) && h4[3] > 0.1*(w*h) ){
                if(aec[0].ROI_ANA_GAIN > 0) {
                    aec[0].ROI_ANA_GAIN--;
                    aec[0].ROI_T_INT_II = 512;
                    decrement_analog_gain();
                    set_integration_time(512);
                }
            }else{
                if( h4[0] < 0.03*(w*h) ){
                    if(aec[0].ROI_ANA_GAIN < 6 ){
                        aec[0].ROI_ANA_GAIN++;
                        aec[0].ROI_T_INT_II = 512;
                        increment_analog_gain();
                        set_integration_time(512);
                    }
                }else{
                    if(aec[0].ROI_ANA_GAIN > 0) {
                        aec[0].ROI_ANA_GAIN--;
                        aec[0].ROI_T_INT_II = 512;
                        decrement_analog_gain();
                        set_integration_time(512);
                    }
                }
            }
        }
    }
    
    printf(" -- FPS %4.1f    EXPOSURE(%c) %5d Ga=%d %s\n",
           get_framerate(),
           exposure_command,
           aec[0].ROI_T_INT_II,
           aec[0].ROI_ANA_GAIN,
           need_gain_correction? " GAIN!" : " "
          );    
}

// aec_agc_algorithm_B(uint32_t *cdf, int b)
// Reference is http://fcam.garage.maemo.org/apiDocs.html
// b - histogram buckets number
void VSensor::aec_agc_algorithm_B(uint32_t *cdf, int b)
{
    assert(64==b);
    assert(cdf);
    
    int brightPixels = cdf[b-1] - cdf[b-21]; // top 20 buckets
    int targetBrightPixels = cdf[b-1]/50;
    int maxSaturatedPixels = cdf[b-1]/200;
    int saturatedPixels = cdf[b-1] - cdf[b-6]; // top 5 buckets  

    printf("AutoExposure: totalPixels: %d,"
            "brightPixels: %d, targetBrightPixels: %d,"
            "saturatedPixels: %d, maxSaturatedPixels: %d\n",
            cdf[b-1], brightPixels, targetBrightPixels,
            saturatedPixels, maxSaturatedPixels);
            
     // how much should I change brightness by
     float adjustment = 1.0f;
 
     if (saturatedPixels > maxSaturatedPixels) {
         // first don't let things saturate too much
         adjustment = 1.0f - ((float)(saturatedPixels - maxSaturatedPixels))/cdf[b-1];
     } else if (brightPixels < targetBrightPixels) {
         // increase brightness to try and hit the desired number of well exposed pixels
         int l = b-11;
         while (brightPixels < targetBrightPixels && l > 0) {
             brightPixels += cdf[l];
             brightPixels -= cdf[l-1];
             l--;
         }
 
         // that level is supposed to be at b-11;
         adjustment = float(b-11+1)/(l+1);
     } else {
         // we're not oversaturated, and we have enough bright pixels. Do nothing.
     }
 
     if (adjustment > 4.0) { adjustment = 4.0; }
     if (adjustment < 1/16.0f) { adjustment = 1/16.0f; }
 
     printf("AutoExposure: adjustment: %f\n", adjustment);
     
     float brightness = aec_gain(0) * aec_time(0);
     float desiredBrightness = brightness * adjustment;
     int exposure;
     float gain;
     
                
}

void VSensor::aec_tune(int h, int w, uint8_t *pYplane)
{
    uint8_t* ptl_luma_plane = pYplane;

    uint32_t summ_luma= 0;
    uint32_t summ_histo= 0;
    uint32_t H64[64];// Histogram with 64  backets
    uint32_t CDF[64];// Cumulative Distribution Function
    uint32_t x,y;
    int i,j,Mediana;

    /* Skip first frames */
    if(aec[0].FRAME<20) goto aec_tune_final;

    memset(CDF,0,64*sizeof(uint32_t));
    memset(H64,0,64*sizeof(uint32_t));
////////////////////////////////////////////////////////////////////////////////////
    get_histogram(ptl_luma_plane,H64,w,h);    
//////////////////////////////////////////////////////////////////////////////////////////
    
    /* get CDF */
    summ_histo = 0;
    for(i=0; i<64; i++) {
        summ_luma += (i+1)*H64[i]*4;
        summ_histo += H64[i];
        CDF[i] = summ_histo; 
    }
    /* Find median */
    x = 0;
    for(i=0; i<64; i++) {
        x += H64[i];
        if(x>((/*summ_histo*/w*h)/2))break;
    }
    Mediana=(i+1)*4;// Why "4": Histogram has 64 buckets, Video has 10 bits, Rendered video has 8 bits
    printf("summ_luma=%6d, summ_histo=%6d, Mean=%6d Mediana=%4d (%9d)\n",summ_luma,summ_histo,summ_histo==0?0:summ_luma/summ_histo,Mediana,x);
    
    /* Draw Histogramm */
    printf(" -- Histogramma -- %5.1f %5.1f %5.1f\n", 
                100.*((float)H64[0]/(float)(w*h)),
                100.*((float)y/(float)(w*h)),
                100.*((float)H64[63]/(float)(w*h)) );
                
    for(i=0; i<64; i++) {
        int stars;
        stars = (int)((((float)H64[i])/((float)w))*0.8 + 0.5); /* 0.8 is empirical value */
        printf( " %8d ",H64[i]);
        stars = stars>202?203:stars;
        for(j=0; j<stars; j++) {
            if(j<202) printf("*");
            else printf("##");
        }
        printf("\n");
    }

    /*
     * AUTO EXPOSURE CONTROL
     *
     * default integration_time = 512 * 1792 / 48000000 = 19.11466 mS
     * integration time big step is 1 line delta = 1792 / 48000000 = 37.3 uS
     * integration time base step is 8*24000000 = 0.33(3) uS
     */
//////////////////////////////////////////////////////////////////////
    aec_agc_algorithm_A(H64,Mediana,w,h);
    aec_agc_algorithm_B(CDF, 64);
/////////////////////////////////////////////////////////////////////////////////////          
    /*
     * Fill nonvideo lines with video data.
     * Copy second line into first and h-2 line into two lowest
     */
    /*
            printf("Line h-4\n");
            for(i=0;i<64;i++)printf(" %0X", (uint8_t)((ptl_luma_plane + (w*(h-4)))[i]) );
            printf("\nLine h-3\n");
            for(i=0;i<64;i++)printf(" %0X", (uint8_t)((ptl_luma_plane + (w*(h-3)))[i]) );
            printf("\nLine h-2\n");
            for(i=0;i<64;i++)printf(" %0X", (uint8_t)((ptl_luma_plane + (w*(h-2)))[i]) );
            printf("\nLine h-1\n");
            for(i=0;i<64;i++)printf(" %0X", (uint8_t)((ptl_luma_plane + (w*(h-1)))[i]) );
    */
#ifdef FIRST_LINE_OK
    memcpy(ptl_luma_plane, ptl_luma_plane + w, w);
    memcpy(ptl_luma_plane + (w*(h-2)), ptl_luma_plane + (w*(h-3)), w);
    memcpy(ptl_luma_plane + (w*(h-1)), ptl_luma_plane + (w*(h-3)), w);
#else
    memcpy(ptl_luma_plane + (w*(h-3)), ptl_luma_plane + (w*(h-4)), w);
    memcpy(ptl_luma_plane + (w*(h-2)), ptl_luma_plane + (w*(h-4)), w);
    memcpy(ptl_luma_plane + (w*(h-1)), ptl_luma_plane + (w*(h-4)), w);
#endif

    // на всякий случай, на будущее...
aec_tune_final:
    aec[0].FRAME++;// на всякий случай, на будущее...
    for(i=7; i>1; i--) memcpy( (void*)&aec[i], (void*)&aec[i-1], sizeof(aec[0]));
}
