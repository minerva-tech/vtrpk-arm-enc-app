#include <sys/mount.h>
#include <sys/mman.h>
#include "ext_headers.h"
#include "log_to_file.h"

#include "comm.h"
#include "auxiliary.h"
#include "vsensor.h"

#define MINIMAL_FPS ((float)8.0)
#define DEFAULT_ANALOG_GAIN ((uint16_t)0) /* 0..7 */
#define DEFAULT_DIGITAL_GAIN ((uint16_t)0) /* 0..255 */

#define EV76C661_CLK_CTRL (24000000.0) /* [Hz], defined by sensor's pll configuration */
#define EV76C661_LINE_LENGTH (112.0 * 8.0) /* [CLK_CTRL cycles], register 0x04 content */
#define EV76C661_MULT_FACTOR (8.0) /* t_int_clk_mult_factor, register 0x0A */
#define EV76C661_MIN_TIME (0.0000015) /* 1.5 uS */
#define EV76C661_MAX_GAIN_D (15.875)
#define EV76C661_MAX_GAIN_A (8.0)
#define EV76C661_MAX_GAIN (EV76C661_MAX_GAIN_D * EV76C661_MAX_GAIN_A)
#define EV76C661_MAX_EXPOSURE ( 1.0 / MINIMAL_FPS )
#define EV76C661_DIGITAL_GAIN_STEP ((15.875-1.0)/255.0)

#define DO_GAIN (2)
#define DO_EXPOSURE (1)

/*
 * Line length quant is 8 CLK_CTRL's tacts.
 * 
 * Exposure_time = 112*8*Lines_Number + 8*Tacts_Number; Dimmention: [Tacts of CLK_CTRL clock]
 * 112*8 Tacts is line length
 * Maximum Exposure = 65534 * 112 * 8 + 255 * 8 = 58718464 + 2040 = 58 720 504;
 * 24000000 / (8 * 16) = 187500 steps of exposure control for 16Hz frame rate (0.333uS step)
 * 187500 / 112 = 1674.10714286 lines. 
 * Minimum exposure is 1.5 uS or ceil(1.5/0.333)= 5 points.
 * Exposure range is 5 to 187500 for 16 fps.
 * 
 */

static const double analog_gain_table[8] = { 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 8.0 };
static const double max_fps_table[8] = { 24.0, 24.0, 20.0, 20.0, 16.0, 8.0, 8.0, 8.0 };

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
    uint64_t NEXT;           // frame number for next correction step
    uint32_t BINNING;        // 0-disable,1,2,4
    double EXPOSURE;        // integration time, in seconds
    double GAIN;            // total sensor gain (analog * digital)
};

static aec_parameters_t aec_state[8];

const reg_val_t init_reg_list[] = {
    /* Best settings
{3A, 0A03},
{38,0287},
{3A,189A},
{38,2B9},
{3A,9430},
{38,2C1},
{3A,A7B5},
{38,2C5},
{3A,200},
{38,2C6},
{3A,13},
{38,2C7},
{3A,7D55},
{38, 2C8},
{3A,8EAF},
{38,2CA},
{3A,0A1E},
{38,2CC},
{3A,016F},
{38,2CD},
{3A,798D},
{38,2CE},
{3A,565A},
{38,2CF},
{3A,0A0A},
{38,2D0},
{3A,207},
{38,2D1},
{3A,017B},
{38,2D2},
{3A,053E},
{38,2D3},
{3A,053C},
{38,2D5},
{3A,3F59},
{38,2D6},
{3A,3F44},
{38,2D7},
{3A,053C},
{38,2D8},
{3A,073D},
{38,2DA},
{3A,053E},
{38,2DB},
{3A,4},
{38,2FA},
*/     
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
    {0x3A, 0x0200},// Reg.0x0E: roi1_t_int_II.
    {0x38, 0x028E},
    {0x3A, 0x0000},// Reg.0x0F: roi1_t_int_clk[7:0]
    {0x38, 0x028F}
};

reg_val_t agc_reg_list[] = {
    {0x3A, 0x0000|(DEFAULT_ANALOG_GAIN<<8)|DEFAULT_DIGITAL_GAIN},// Reg.0x11
    {0x38, 0x0280|0x0011}
};

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

void VSensor::set_time(int lines, int fraction)
{
    uint16_t integer_part = (uint16_t)lines;
    uint16_t fractional_part = (uint16_t)fraction;
    
    assert( integer_part < 0xFFFF );
    assert( fractional_part < 256 );
    
    aec_reg_list[0].val = integer_part;
    aec_reg_list[2].val = (aec_reg_list[2].val&0xFF00)|fractional_part;
    set_regs(aec_reg_list);
}

void VSensor::set_gain(int analog, int digital)
{
   uint16_t a = (uint16_t)analog;
   uint16_t d = (uint16_t)digital;

   assert( a < 8 );
   assert( d < 256 );

   agc_reg_list[0].val = (a<<8)|d;
   set_regs(agc_reg_list);
}

void VSensor::set_state(char type, int value)
{
    switch(type){
        case 'A': /* analog gain */
            aec_state[0].ROI_ANA_GAIN = (uint16_t)value;
            break;
        case 'D': /* digital gain */
            aec_state[0].ROI_DIG_GAIN = (uint16_t)value;
            break;
        case 'I': /* integration time integer part */
            aec_state[0].ROI_T_INT_II = (uint16_t)value;
            break;
        case 'F': /* integration time fractional part */
            aec_state[0].ROI_T_INT_CLK = (uint16_t)value;
            break;
        default:
            assert(0);
            return;
    }
    aec_state[0].EXPOSURE = aec_time(0);
    aec_state[0].GAIN = aec_gain(0);
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
        aec_state[i].ROI_T_INT_II = 0x0200;
        aec_state[i].ROI_T_INT_CLK = 0;
        aec_state[i].ROI_ANA_GAIN = DEFAULT_ANALOG_GAIN;
        aec_state[i].ROI_DIG_GAIN = 0;
        aec_state[i].FRAME = 0;
        aec_state[i].NEXT = 1;
        aec_state[i].BINNING = (uint32_t)binning;
        aec_state[i].EXPOSURE = aec_time(0);
        aec_state[i].GAIN = aec_gain(0);
    }
}

//
// aec_gain(int i)
// i - index of frame: i=0 this frame(0), i=1 previouse frame(-1) etc.
double VSensor::aec_gain(int i)
{
    double binning_gain;
    double gain;
    
    assert( aec_state[0].ROI_ANA_GAIN<7 );
    assert( i<8 );
    
    if(0 == aec_state[i].BINNING) binning_gain = 1.0;
    else binning_gain = 4.0 / (double)aec_state[i].BINNING;
    
    gain = analog_gain_table[aec_state[i].ROI_ANA_GAIN];
    gain *= binning_gain;
    gain *= (1.0 + ((double) aec_state[i].ROI_DIG_GAIN * EV76C661_DIGITAL_GAIN_STEP));
    return  gain;
}

//
// aec_time(int i) return exposure time in seconds
// i - index of frame: i=0 this frame(0), i=1 previouse frame(-1) etc.
double VSensor::aec_time(int i)
{
    double time;
    
    assert( i<8 );
    
    time = EV76C661_LINE_LENGTH * aec_state[i].ROI_T_INT_II;
    time += (EV76C661_MULT_FACTOR * aec_state[i].ROI_T_INT_CLK);
    time /= EV76C661_CLK_CTRL;
    
    if( time < EV76C661_MIN_TIME ) time = EV76C661_MIN_TIME;
    
    return time;
}

static uint16_t e2v_magic[8/2];
static uint16_t e2v_header[26/2];
static uint16_t e2v_histogram[512/2];
static uint16_t e2v_footer[ 6/2];

inline static uint16_t swap16(uint16_t x)
{
    return ((x<<8)&0xFF00)|((x>>8)&0x00FF);    
}

static int read_metadata_emif(void)
{
	int fd,i,show=1;
    volatile int delay;

	if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
		log() << "cannot open /dev/mem. su?";
		return 0;
	}

	void * volatile map_base = mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0x04000000);

	uint8_t* regs = (uint8_t*)map_base;
    uint16_t datum, tmp;

    datum = *(uint16_t*)(regs+0x30);

reset_fifo_and_exit:    
    if( 0x4652 != datum ){
        if( 0xFAAC != datum )
            *(uint16_t*)(regs+0x30) = 0x0000;// write any value to reset fifo and statemachine
        munmap((void*)map_base, 1024);
        close(fd);
        return 0;
    }
    
    /*
     * all printfs below can not be removed because they are 
     * delay elements
    */
    printf("\n ************ Read Meta Data ***********\n");
    printf("\n *** FRAME COUNTER ***\n");
    e2v_magic[0] = datum;
    printf(" %04x", e2v_magic[0]);
	for(i=1; i<4; i++){ 
        datum = *(uint16_t*)(regs+0x30);
        tmp = *(uint16_t*)(regs+0x2E);
        if( datum == 0xFAAC || datum==0x4652 ){
            printf("\n ERR: %d %04x\n",i,datum);
            goto reset_fifo_and_exit;
        }
        e2v_magic[i] = datum; 
        e2v_magic[i] = swap16(e2v_magic[i]);
        printf(" %04x", e2v_magic[i]);
    }
    printf("\n");
	
    printf(" *** HEADER ***\n");
	for(i=0; i<13; i++){ 
        datum = *(uint16_t*)(regs+0x30);
        tmp = *(uint16_t*)(regs+0x2E);
        if( datum == 0xFAAC || datum==0x4652 ){
            printf("\n ERR: %d %04x\n",i,datum);
            goto reset_fifo_and_exit;
        }
        e2v_header[i] = datum;
        e2v_header[i] = swap16(e2v_header[i]);
        printf(" %04x", e2v_header[i]);
    }
    printf("\n");
	
    if(show)printf(" *** HISTOGRAM ***\n");
	for(i=0; i<256; i++){ 
        datum = *(uint16_t*)(regs+0x30);
        tmp = *(uint16_t*)(regs+0x2E);
        if( datum == 0xFAAC || datum==0x4652 ){
            printf("\n ERR: %d %04x\n",i,datum);
            goto reset_fifo_and_exit;
        }
        e2v_histogram[i] = datum;
        if(show)printf(" %04x", e2v_histogram[i]);
        //for(delay=100;delay>0;delay--);
    }
    if(show)printf("\n");
    
    printf(" *** FOOTER ***\n");
	for(i=0; i<3; i++){
        datum = *(uint16_t*)(regs+0x30);
        tmp = *(uint16_t*)(regs+0x2E);
        if( datum==0xFAAC || datum==0x4652 ){ 
            printf("\n ERR: %d %04x\n",i,datum);
            goto reset_fifo_and_exit;
        }
        e2v_footer[i] = datum;
        e2v_footer[i] = swap16(e2v_footer[i]);
        printf(" %04x", e2v_footer[i]);
    }
    printf("\n");
    
    // total 4+13+256+3 = 276 

	munmap((void*)map_base, 1024);
	close(fd);
    return 1;
}

// get_histogram(const void *luma_plane, uint32_t *histogram, int w, int h)
// w - image width
// h - image height
//#define FIRST_LINE_OK
static int get_histogram(void *luma_plane, uint32_t *histogram, int w, int h)
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
    assert( (1280==w && (1024==h ||512==h)) || (640==w && (512==h || 256==h)) );
  
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
    return 1;
}

static int get_histogram_emif(uint32_t *pframe_number, uint32_t *histogram, int w, int h)
{
    uint8_t* ptr_footer_line;
    uint8_t* ptr_magic_line;
    
    uint16_t Blacks,Whites;
    uint16_t tmp;
    uint32_t x,y;
    uint32_t *H64 = histogram;

    int i,j,oe,ue,Mediana;

    assert(histogram);
    assert( (1280==w && (1024==h ||512==h)) || (640==w && (512==h || 256==h)) );

    *pframe_number = 0;
    
    if( !read_metadata_emif() ) return 0;
    
    ptr_magic_line = (uint8_t*)e2v_magic;
    ptr_footer_line = (uint8_t*)e2v_footer;
    
    uint32_t frame_number;
    frame_number = ((((uint32_t)e2v_magic[3])<<16)&0xFFFF0000) | (((uint32_t)e2v_magic[2])&0x0000FFFF);
    *pframe_number = frame_number;
    
    /*
    printf("\n *** MAGIC ***\n");
    for(i=0;i<8;i++) {
        printf(" %02X", ptr_magic_line[i]);
    }
    printf("\n");
    printf(" FRM# %08x ( %10d )\n", frame_number, frame_number);
    */
    
    printf(" -- Footer -- w=%d h=%d\n", w, h);
    for(i=0; i<5; i++) printf(" %02X",ptr_footer_line[i]);
    printf("\n");
    
    printf("Error flags: 0x%02X  ",ptr_footer_line[0]);
    tmp = ((uint16_t)ptr_footer_line[1]<<8 | (uint16_t)ptr_footer_line[2]);
    printf("0x00 pixels: %d  ",tmp);
    Blacks = tmp;
    tmp = ((uint16_t)ptr_footer_line[3]<<8 | (uint16_t)ptr_footer_line[4]);
    printf("0xFF pixels: %d\n",tmp);
    
    Whites = tmp;
    oe = 0;
    ue = 0;
    for(i=0; i<64; i++) {
        uint32_t bin=0;
        /* use all 4 gistorgamms */
        tmp = e2v_histogram[i+64*0];
        if(tmp==0xFFFF && i==0)oe++;
        if(tmp==0xFFFF && i==63)ue++;
        bin += (int)tmp;
        tmp = e2v_histogram[i+64*1];;
        if(tmp==0xFFFF && i==0)oe++;
        if(tmp==0xFFFF && i==63)ue++;
        bin += (int)tmp;
        tmp = e2v_histogram[i+64*2];;
        if(tmp==0xFFFF && i==0)oe++;
        if(tmp==0xFFFF && i==63)ue++;
        bin += (int)tmp;
        tmp = e2v_histogram[i+64*3];;
        if(tmp==0xFFFF && i==0)oe++;
        if(tmp==0xFFFF && i==63)ue++;
        bin += (int)tmp;
        H64[63-i] = bin;
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
    return 1;
}


// get_cdf(uint32_t *histogram, uint32_t *CDF, int buckets)
// create CDF for histogram with 64 buckets and 10-bits value size
// find average cumulative value
// Carefully use *histogram and *CDF arrays! 
static void get_cdf(uint32_t *histogram, uint32_t *CDF, uint32_t *cv)
{
    uint32_t summ_histo = 0;
    uint32_t summ_luma = 0;
    int i=0;
    
    for(i; i<64; i++) {
        summ_luma += (i+1)*histogram[i]*4;
        summ_histo += histogram[i];
        CDF[i] = summ_histo;
    } 
    *cv = summ_luma;
}

static int get_mediana(uint32_t *H64, int w, int h)
{
    int x;;
    int i;
    for(i=0,x=0; i<64; i++) {
        x += H64[i];
        if(x>((/*CDF[63]*/w*h)/2))break;
    }
    //printf(" GetMediana() %d %d\n",i,x);
    return (i+1)*4;// Why "4": Histogram has 64 buckets, Video has 10 bits, Rendered video has 8 bits    
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


#define Y_TOP_LIM (133+3)
#define Y_BOT_LIM (125-3)
#define Y_MEDIAN  ( (float)(Y_TOP_LIM + Y_BOT_LIM) / 2.0 )
#define FPS_MINIMUM ((float)2.5)
#define EXPOSURE_DELTA_INCR ((uint16_t)(4*6))
#define EXPOSURE_DELTA_DECR ((uint16_t)(4*4))
#define EXPOSURE_TOP_LIMIT (3200)

inline float get_min_fps(int index)
{
    assert(index<7);
    return max_fps_table[index];
}

void VSensor::aec_agc_algorithm_A(uint32_t *H64, int Mediana, int w, int h)
{
    char exposure_command = ' ';
    static int need_gain_correction = 0;
    uint32_t h4[4] = {0,0,0,0};
    int i;
    
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
            if( aec_state[0].ROI_T_INT_II < EXPOSURE_TOP_LIMIT ) {
                /*
                uint16_t delta = EXPOSURE_TOP_LIMIT - aec_state[0].ROI_T_INT_II;
                if(delta>1) delta = (uint16_t)((float)delta*0.125);
                */
                uint16_t delta;

                if( (Y_BOT_LIM-Mediana)<((Y_TOP_LIM-Y_BOT_LIM)/2) ) delta = 1;
                else delta = EXPOSURE_DELTA_INCR;

                aec_state[0].ROI_T_INT_II += delta;
                increment_integration_time(delta);
                need_gain_correction = 0;
            } else {
                // TODO: NEED Gain Correction (increase gain)
                
                if(aec_state[0].ROI_ANA_GAIN < 6 ){
                    aec_state[0].ROI_ANA_GAIN++;
                    aec_state[0].ROI_T_INT_II = 512;
                    increment_analog_gain();
                    set_integration_time(512);
                }
                
                need_gain_correction = 1;
            }
        } else {
            // shortest exposure limited by zero
            if( aec_state[0].ROI_T_INT_II > 1 ) {
                /*
                uint16_t delta = (uint16_t)((float)aec_state[0].ROI_T_INT_II*0.125);
                */
                uint16_t delta;
                if( (Mediana-Y_TOP_LIM)<((Y_TOP_LIM-Y_BOT_LIM)/2) ) delta = 1;
                else delta = EXPOSURE_DELTA_INCR;

                if( !(aec_state[0].ROI_T_INT_II>delta) ) delta=1;

                aec_state[0].ROI_T_INT_II -= delta;

                decrement_integration_time(delta);
                need_gain_correction = 0;
            } else {
                // TODO: NEED Gain Correction (decrease gain)
                
                if(aec_state[0].ROI_ANA_GAIN > 0){
                   aec_state[0].ROI_ANA_GAIN--;
                   aec_state[0].ROI_T_INT_II = 512;
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
                if(aec_state[0].ROI_ANA_GAIN > 0) {
                    aec_state[0].ROI_ANA_GAIN--;
                    aec_state[0].ROI_T_INT_II = 512;
                    decrement_analog_gain();
                    set_integration_time(512);
                }
            }else{
                if( h4[0] < 0.03*(w*h) ){
                    if(aec_state[0].ROI_ANA_GAIN < 6 ){
                        aec_state[0].ROI_ANA_GAIN++;
                        aec_state[0].ROI_T_INT_II = 512;
                        increment_analog_gain();
                        set_integration_time(512);
                    }
                }else{
                    if(aec_state[0].ROI_ANA_GAIN > 0) {
                        aec_state[0].ROI_ANA_GAIN--;
                        aec_state[0].ROI_T_INT_II = 512;
                        decrement_analog_gain();
                        set_integration_time(512);
                    }
                }
            }
        }
    }
    aec_state[0].EXPOSURE = aec_time(0);
    aec_state[0].GAIN = aec_gain(0);
                   
    printf(" -- FPS %4.1f    EXPOSURE(%c) %5d (%8.3f ms) Ga=%d (%8.3f) %s\n",
           get_framerate(),
           exposure_command,
           aec_state[0].ROI_T_INT_II, aec_state[0].EXPOSURE * 1000.0,
           aec_state[0].ROI_ANA_GAIN, aec_state[0].GAIN,
           need_gain_correction? " GAIN!" : " "
          );    
}



/* aec_agc_algorithm_AA()           */
/* !!! Only for 64-bin histogram    */
/* Если следить за медианой в границах Y_TOP_LIM (155), Y_BOT_LIM (135) то при снижении освещенности
 * до некоторого уровня а за тем возвращении не происходит корректировка экспозиции - она увеличивается
 * при снижении освещенности, но не уменьшается при восстановлении освещенности. Это происходит потому,
 * что медиан остается в заданных пределах. попробую поправить сужением допустимого диапазона медианы.
 * Для 150-140 стало лучше. Для 148-142 медленнее настаивается.
 */
bool VSensor::aec_agc_algorithm_AA(uint32_t *H64, uint32_t *CDF, int Mediana, int w, int h)
{
    char exposure_command = ' ';
    static int need_gain_correction = 0;
 
    int i;
    float exposure;// [seconds]
    float gain;
    float Yratio;
    
    if(aec_state[0].FRAME < aec_state[0].NEXT) {
        exposure_command = ' ';
        goto quit_aec_AA;
    }

    Yratio = Y_MEDIAN/Mediana;
    
    if(Mediana<Y_BOT_LIM)
        exposure_command = '+';
    else if(Mediana>Y_TOP_LIM)
        exposure_command = '-';
    else{
        exposure_command = ' ';
        goto quit_aec_AA;
    }
        
    /* smooth change */
    if( Yratio > 4) Yratio = 4;
    if( Yratio < 0.25) Yratio = 0.25;
    
    exposure = aec_state[0].EXPOSURE * Yratio;
    gain = aec_gain(0);
        
    if( get_min_fps(aec_state[0].ROI_ANA_GAIN) > (1.0/exposure) ) {
        printf("  gain Up [e %8.3f]\n", 1000.0*exposure);
        need_gain_correction = 1;
        exposure = 1.0/get_min_fps(aec_state[0].ROI_ANA_GAIN);
    }else if(exposure < 0.0000015){// minimum exposure is 1.5 uS
        printf("  gain Down [e %8.3f]\n", 1000.0*exposure );
        need_gain_correction = -1;
        exposure = 0.0000015;
    }else{
        need_gain_correction = 0;
    }
    
    if(exposure < 0.0000015) exposure = 0.0000015;// minimum exposure is 1.5 uS
    if(gain < 1.0) gain = 1.0;
    if(gain > 8*15.875) gain = 8*15.875;
      
    aec_state[0].EXPOSURE  = exposure;
    aec_state[0].GAIN      = gain; 
    aec_state[0].NEXT = aec_state[0].FRAME + 1/* .FRAME will be incremented later */ + 1/* video pipe depth */;
        
quit_aec_AA:        
    printf(" -- FPS %4.1f    EXPOSURE(%c) %8.3f ms [x %6.3f] G=%6.3f (%8.3f)  Gain%s\n",
           get_framerate(),
           exposure_command,
           aec_state[0].EXPOSURE * 1000.0,Yratio,
           aec_state[0].GAIN,gain,
           (need_gain_correction>0)? " +" : ((need_gain_correction<0)?" -":" OK")
          ); 

    return ' '==exposure_command ? 0 : 1;   
}

// aec_agc_algorithm_B(uint32_t *cdf, int b)
// Reference is http://fcam.garage.maemo.org/apiDocs.html
// b - histogram buckets number
// smoothness 1.0 .... 0.0
bool VSensor::aec_agc_algorithm_B(uint32_t *cdf, int Ymean, int b, float smoothness)
{
    char exposure_command;
    float exposure;
    float gain;
    
    assert(64==b);
    assert(cdf);
    
    int brightPixels = cdf[b-1] - cdf[b-21]; // top 20 buckets
    int targetBrightPixels = cdf[b-1]/50;
    int maxSaturatedPixels = cdf[b-1]/100;//200
    int saturatedPixels = cdf[b-1] - cdf[b-6]; // top 5 buckets  
    
    float brightness;
    float desiredBrightness;
    float shotBrightness;
    float exposureKnee;
    float adjustment,delta;
    
    printf("AutoExposure: totalPixels: %d,"
            "brightPixels: %d, targetBrightPixels: %d,"
            "saturatedPixels: %d, maxSaturatedPixels: %d\n",
            cdf[b-1], brightPixels, targetBrightPixels,
            saturatedPixels, maxSaturatedPixels);
     
    // how much should I change brightness by
    adjustment = 1.0f;
    
    if(aec_state[0].FRAME < aec_state[0].NEXT) {
        exposure_command = ' ';
        goto quit_aec_B;
    }
            
     if (saturatedPixels > maxSaturatedPixels) {
         // first don't let things saturate too much
         adjustment = 1.0f - ((float)(saturatedPixels - maxSaturatedPixels))/cdf[b-1];
         exposure_command = '-';
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
         exposure_command = '+';
     } else {
         /* experiment: correct mean or median
            possible variants (note: Ymean=(i+1)*4 ):
            a+) Ymean/4-1 < Y_BOT_LIM/4-1
                adjustment = 1 + ( cdf[Y_BOT_LIM/4-1] - cdf[Ymean/4-1] ) / cdf[63]
            b+) Ymean/4-1 = Y_BOT_LIM/4-1
                adjustment = 1 + 0.05;
            a-) Ymean/4-1 > Y_TOP_LIM/4-1
                adjustment = 1 - ( cdf[Ymean/4-1] - cdf[Y_TOP_LIM/4-1] ) / cdf[63]
            b-) Ymean/4-1 = Y_TOP_LIM/4-1
                adjustment = 1 - 0.05;
         */
        /*
         * hard to undestend why adjusment in such way
        if(Ymean<Y_BOT_LIM){
            //Ymean=(i+1)*4   
            adjustment = 1.0f + (float)cdf[((Y_TOP_LIM - Y_BOT_LIM) / 4)-1] / (float)cdf[63];
            exposure_command = '^';
        }else if(Ymean>Y_TOP_LIM){
            adjustment = 1.0f - (float)cdf[((Y_TOP_LIM - Y_BOT_LIM) / 4)-1] / (float)cdf[63];
            exposure_command = 'v';
        }else{
            // we're not oversaturated, and we have enough bright pixels. Do nothing.
            exposure_command = ' ';
        }
        */
        /*
        if(Ymean<Y_BOT_LIM){ 
            adjustment = 1.0 + 0.05;
            exposure_command = '^';
        }else if(Ymean>Y_TOP_LIM){
            adjustment = 1.0f - 0.05;
            exposure_command = 'v';
        }else{
            exposure_command = ' ';
        }
        */
        if(Ymean<Y_BOT_LIM){ 
            if( Ymean/4-1 < Y_BOT_LIM/4-1 )
                adjustment = 1.0 + (float)(cdf[Y_BOT_LIM/4-1] - cdf[Ymean/4-1]) / (float)cdf[63];
            else
                adjustment = 1.0 + 0.06;
                
            exposure_command = '^';
        }else if(Ymean>Y_TOP_LIM){
            if( Ymean/4-1 > Y_TOP_LIM/4-1 )
                adjustment = 1.0 - (float)( cdf[Ymean/4-1] - cdf[Y_TOP_LIM/4-1] ) / (float)cdf[63];
            else
                adjustment = 1.0 - 0.06;
                
            exposure_command = 'v';
        }else{
            exposure_command = ' ';
        }
        
         
     }
 
     if (adjustment > 4.0) { adjustment = 4.0; }
     if (adjustment < 1/16.0f) { adjustment = 1/16.0f; }
     
     if (adjustment>1.0) delta = adjustment-1.0;
     else delta = 1.0-adjustment;
     
    if( delta < 0.05 ) exposure_command = ' '; 

    brightness = aec_gain(0) * aec_time(0);
    desiredBrightness = brightness * adjustment;

    // Apply the smoothness constraint
    //shotBrightness = aec_state[0].GAIN * aec_state[0].EXPOSURE;
    shotBrightness = aec_gain(0) * aec_time(0);
    desiredBrightness = shotBrightness * smoothness + desiredBrightness * (1-smoothness);
    
    /*

    desiredBrightness = aec_gain(0) * aec_time(0) * smoothness + 
                        aec_gain(0) * aec_time(0) * (1-smoothness) * adjustment;
        
    */
 
    // whats the largest we can raise exposure without negatively
    // impacting frame-rate or introducing handshake.
    exposureKnee = 1.0/get_min_fps(aec_state[0].ROI_ANA_GAIN);
    
    if (desiredBrightness > exposureKnee) {
        exposure = exposureKnee;
        gain = desiredBrightness / exposureKnee;
    } else {
        gain = 1.0f;
        exposure = desiredBrightness / gain;
    }
    
quit_aec_B:
    printf("AutoExposure: old e %8.3f, g %8.3f. new: e %8.3f, g %8.3f adjustment %6.3f [%c]\n",
             aec_state[0].EXPOSURE*1000.0, aec_state[0].GAIN, 
             exposure*1000.0, gain, adjustment,
             exposure_command);
             
    if( ' '==exposure_command ){
        return 0;
    }else{
        aec_state[0].EXPOSURE  = exposure;
        aec_state[0].GAIN      = gain;
        aec_state[0].NEXT = aec_state[0].FRAME + 1/* .FRAME will be incremented later */ + 1/* video pipe depth */;
        return 1;
    }
}

/*
 * 
 */
float VSensor::aec_agc_algorithm_BB(uint32_t *cdf, int Ymean, int b)
{
    char exposure_command;
    float current_exposure, desired_exposure;
    float current_gain, desired_gain;
    float gain, exposure;
    float adjustment,delta;
    
    assert(64==b);
    assert(cdf);
    
    int brightPixels = cdf[b-1] - cdf[b-21]; // top 20 buckets
    int targetBrightPixels = cdf[b-1]/50;
    int maxSaturatedPixels = cdf[b-1]/100;//200
    int saturatedPixels = cdf[b-1] - cdf[b-6]; // top 5 buckets  
    
    printf("AutoExposure: totalPixels: %d,"
            "brightPixels: %d, targetBrightPixels: %d,"
            "saturatedPixels: %d, maxSaturatedPixels: %d\n",
            cdf[b-1], brightPixels, targetBrightPixels,
            saturatedPixels, maxSaturatedPixels);
     
    // how much should I change brightness by
    adjustment = 1.0f;
    
    if(aec_state[0].FRAME < aec_state[0].NEXT) {
        return adjustment;
    }
            
     if (saturatedPixels > maxSaturatedPixels) {
         // first don't let things saturate too much
         adjustment = 1.0f - ((float)(saturatedPixels - maxSaturatedPixels))/cdf[b-1];
         exposure_command = '-';
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
         exposure_command = '+';
     } else {
        if(Ymean<Y_BOT_LIM){ 
            if( Ymean/4-1 < Y_BOT_LIM/4-1 )
                adjustment = 1.0 + (float)(cdf[Y_BOT_LIM/4-1] - cdf[Ymean/4-1]) / (float)cdf[63];
            else
                adjustment = 1.0 + 0.06;
                
            exposure_command = '^';
        }else if(Ymean>Y_TOP_LIM){
            if( Ymean/4-1 > Y_TOP_LIM/4-1 )
                adjustment = 1.0 - (float)( cdf[Ymean/4-1] - cdf[Y_TOP_LIM/4-1] ) / (float)cdf[63];
            else
                adjustment = 1.0 - 0.06;
                
            exposure_command = 'v';
        }else{
            exposure_command = ' ';
        }
     }
 
     if (adjustment > 4.0) { adjustment = 4.0; }
     if (adjustment < 1/16.0f) { adjustment = 1/16.0f; }
     
     if (adjustment>1.0) delta = adjustment-1.0;
     else delta = 1.0-adjustment;
     
    if( delta < 1/16.0f ) {
        exposure_command = ' '; 
        adjustment = 1.0;
    }
    
    /*
    desired_exposure = aec_gain(0) * aec_time(0) * smoothness + 
                       aec_gain(0) * aec_time(0) * (1-smoothness) * adjustment;
    
    desired_exposure = aec_gain(0) * aec_time(0) * ( smoothness + (1-smoothness) * adjustment );
    desired_exposure = aec_gain(0) * aec_time(0) * ( smoothness + adjustment - smoothness * adjustment );            
    desired_adjustment = smoothness + (1-smoothness) * adjustment;
    хуйня эта гладкость.
    */
    
    printf("AutoExposure: old e %8.3f, g %8.3f  ", aec_state[0].EXPOSURE*1000.0, aec_state[0].GAIN);

    
    current_gain = aec_gain(0);
    current_exposure = aec_time(0);
    
    desired_exposure = current_exposure * adjustment;
    desired_gain = current_gain * adjustment;
/*
 * правильно распределить приоритеты: что меняем в первую очередь?
 * повышаем кадровую или понижаем коэф. усиления?
*/  
if( /*exposure_command != ' '*/0 ){ // этот кот не очень хорошо работает. осциллирует экспозиция. недоделан.
    if( desired_exposure > 1.0/get_min_fps(aec_state[0].ROI_ANA_GAIN) ){
        /* increase gain */
        if( aec_state[0].ROI_ANA_GAIN >= 6 /* current analog gain is maximum */ ){
            /*
            if( aec_state[0].ROI_DIG_GAIN < 255 ){
                ;// TODO: increase digital gain and increase exposure if need
            }else{
                ;// TODO: set maximum exposure
            }
            */
        }else{
            /* increment analog gain */
            aec_state[0].ROI_ANA_GAIN++;
            aec_state[0].GAIN = aec_gain(0); /* recalculate gain */
            /* Do we need to adjust exposure ? */
        }
        set_gain(aec_state[0].ROI_ANA_GAIN, aec_state[0].ROI_DIG_GAIN);
        /* exposure will be corrected at the next frame(s) */
    }else if(desired_exposure < 0.0000015){
        /* decrease gain */
        if( aec_state[0].ROI_ANA_GAIN == 0/* current analog gain is minimum */ ){
            /* decrease digital gain */
            if( aec_state[0].ROI_DIG_GAIN == 0 ){
                ;// TODO: ...
            }else{
                ;// TODO: ...
            }
        }else{
            /* decrement analog gain */
            aec_state[0].ROI_ANA_GAIN--;
            aec_state[0].GAIN = aec_gain(0); /* recalculate gain */
        }
        set_gain(aec_state[0].ROI_ANA_GAIN, aec_state[0].ROI_DIG_GAIN);
        /* exposure will be corrected at the next frame(s) */
    }else{
        if( current_gain == 1 ){ /* уменьшать усиление некуда */
            /* Keep gain */
            aec_state[0].EXPOSURE = desired_exposure; /* смело используем новую экспозицию т.к. уже проверили ее */
            exposure_set(desired_exposure); /* setup sensors's registers */    
        }else{
            /* Try to decrease gain */
            if( '-' == exposure_command || 'v' == exposure_command ){
                if( desired_gain > 1 ){
                    aec_agc_set(desired_gain, current_exposure, DO_GAIN);
                    /* exposure will be corrected at the next frame(s) */
                }else{
                    /* set gain 1.0 */
                    desired_gain = 1.0;
                    aec_agc_set(desired_gain, current_exposure, DO_GAIN);
                    /* exposure will be corrected at the next frame(s) */
                }
            }
            /* R.F.U, do not deleate
            if( '+' == exposure_command || '^' == exposure_command ){
                if( desired_gain > 8 ){
                    // increase digital gain
                }else{
                    // increase analog dain
                }
            }
            */
            /* Keep gain */
            aec_state[0].EXPOSURE = desired_exposure; /* смело используем новую экспозицию т.к. уже проверили ее */
            exposure_set(desired_exposure); /* setup sensors's registers */              
        }
                
    }
    /* apply new gain and exposure */
}
    
/* limit frame rate to 16 fps for all gains */
if(exposure_command != ' '){
    if( exposure_command == '+' || exposure_command == '^' ){
        if( desired_exposure < 1.0/16.0 ){
            exposure = desired_exposure;
            gain = current_gain;
            aec_agc_set(gain, exposure, DO_EXPOSURE);//exposure_set(exposure);            
        }else{
            exposure = 1.0/16.0;
            gain = current_exposure * current_gain * adjustment / exposure;
            aec_agc_set(gain, exposure, DO_GAIN|DO_EXPOSURE);            
        }
    }
    if( exposure_command == '-' || exposure_command == 'v' ){
        if( desired_gain < 1.0 ){
            gain = 1.0;
            exposure = current_exposure * current_gain * adjustment / gain;
            aec_agc_set(gain, exposure, DO_GAIN|DO_EXPOSURE);
        }else{
            gain = current_gain * adjustment;
            exposure = current_exposure;
            aec_agc_set(gain, exposure, DO_GAIN);
        }
    }
}
    printf("New: e %8.3f, g %8.3f adjustment %6.3f [%c]\n",
             aec_state[0].EXPOSURE*1000.0, aec_state[0].GAIN, 
             adjustment, exposure_command);
    
    return adjustment;
}

bool VSensor::aec_agc_algorithm_C(uint32_t *cdf, int Mediana, int b, float smoothness)
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
     
     if(aec_state[0].FRAME < aec_state[0].NEXT) return 0;
         
     float adjustment = 1.0f;
 
    // Meadiana and Histogram within appropriate bins
    
    //(float)(cdf[63-2]-cdf[2])>(cdf[63]*0.75)
    
    if(Mediana<Y_BOT_LIM){
    //Mediana=(i+1)*4   
        adjustment = 1.0f + (float)cdf[((Y_TOP_LIM - Y_BOT_LIM) / 4)-1] / (float)cdf[63];
    }else if(Mediana>Y_TOP_LIM){
        adjustment = 1.0f - (float)cdf[(Mediana / 4)-1] / (float)cdf[63];
    }else{
        //Do nothing.
    }
    
    /*
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
     */
 
     if (adjustment > 4.0) { adjustment = 4.0; }
     if (adjustment < 1/16.0f) { adjustment = 1/16.0f; }
 
     printf("AutoExposure: adjustment: %f\n", adjustment);
     
     float brightness = aec_gain(0) * aec_time(0);
     float desiredBrightness = brightness * adjustment;
     float exposure;
     float gain;
      
     // Apply the smoothness constraint
     float shotBrightness = aec_state[0].GAIN * aec_state[0].EXPOSURE;
     desiredBrightness = shotBrightness * smoothness + desiredBrightness * (1-smoothness);
 
     // whats the largest we can raise exposure without negatively
     // impacting frame-rate or introducing handshake. We use 8 fps
    float exposureKnee;
    
    exposureKnee = 1.0 / get_min_fps(aec_state[0].ROI_ANA_GAIN); 

     if (desiredBrightness > exposureKnee) {
         exposure = exposureKnee;
         gain = desiredBrightness / exposureKnee;
     } else {
         gain = 1.0f;
         exposure = desiredBrightness;
     }
 
     // Clamp the gain at max, and try to make up for it with exposure
     if (gain > EV76C661_MAX_GAIN) {
         exposure = desiredBrightness/EV76C661_MAX_GAIN;
         gain = EV76C661_MAX_GAIN;
     }
 
     // Finally, clamp the exposure at max
     if (exposure > EV76C661_MAX_EXPOSURE) {
         exposure = EV76C661_MAX_EXPOSURE;
     }
 
     printf("AutoExposure: old exposure, gain: %8.3f, %8.3f. new: %8.3f, %8.3f\n",
             aec_state[0].EXPOSURE*1000.0, aec_state[0].GAIN, exposure*1000.0, gain);
 
     aec_state[0].EXPOSURE  = exposure;
     aec_state[0].GAIN      = gain; 
     
     aec_state[0].NEXT = aec_state[0].FRAME + 1 + 1;
     
     return 1;
}

//aec_agc_set() - translate exposure time and gain from phisical units
// to sensor's units and send these data to sensor
//command == 0  NOP, do nothing
//command == 1 do exposure
//command == 2 do gain
//command == 1 | 2 do exposure and gain
void VSensor::aec_agc_set(float Gain, float Exposure, int command)
{
    float gain = Gain;
    float exposure = Exposure;
    // 1. select gain: analog gain should be minimal
    float  a;
    float  binning_gain; // TODO: CHECK fucking binning !!!
    int    a_int;
    int    ana_gain_index;
    int    dig_gain_index;
        
    /* 1. Compute GAIN */
    if(0 == aec_state[0].BINNING) binning_gain = 1.0;
    else binning_gain = 4.0 / (float)aec_state[0].BINNING;
        
    if( gain/binning_gain <= 1.0 ){
        ana_gain_index = 0;
        dig_gain_index = 0;
    }else{
        if(gain/binning_gain > 8){//Use digital gain
            //Set analog gain to maximum
            ana_gain_index = 6;
            dig_gain_index = floor((gain / (analog_gain_table[ana_gain_index]*binning_gain)) - 1) / EV76C661_DIGITAL_GAIN_STEP;
        }else{
            int i;
            for(i=0;i<5;i++){
                if( ceil((gain/binning_gain)*100.0) > (100.0*analog_gain_table[i]) &&
                    ceil((gain/binning_gain)*100.0) <= (100.0*analog_gain_table[i+1]) ) break;
            }
            ana_gain_index = i+1;
            dig_gain_index = 0;
        }
    }
    if( 255 < (int)dig_gain_index ) {
        printf(" WARNING: dig_gain_index %d\n",(int)dig_gain_index );
        dig_gain_index = 255;
    }
        
    /* 2. Compute EXPOSURE */
    
    // correct exposure with new gain
    exposure = (exposure * gain) / (analog_gain_table[ana_gain_index]*binning_gain*(1.0 + ((float)dig_gain_index * EV76C661_DIGITAL_GAIN_STEP)));
    
    int int_part = (int)(exposure * EV76C661_CLK_CTRL) / (int)EV76C661_LINE_LENGTH;
    int frac_part = (int)((exposure * EV76C661_CLK_CTRL) - (int_part*(int)EV76C661_LINE_LENGTH))/EV76C661_MULT_FACTOR;

    if(65534 < int_part){
        printf(" WARNING: ROI_T_INT_II limited to 65534\n");
        int_part = 65534;
    }
    if(255 < frac_part){
        printf(" WARNING: ROI_T_INT_CLK limited to 255\n");
        frac_part = 255;
    }
                
    if( command & DO_GAIN ){
        aec_state[0].GAIN      = analog_gain_table[ana_gain_index]*binning_gain*(1.0 + ((float)dig_gain_index * EV76C661_DIGITAL_GAIN_STEP));
        aec_state[0].ROI_ANA_GAIN = ana_gain_index;
        aec_state[0].ROI_DIG_GAIN = dig_gain_index;
        set_gain(aec_state[0].ROI_ANA_GAIN,aec_state[0].ROI_DIG_GAIN);
    }
        
    if( command & DO_EXPOSURE ){
        aec_state[0].EXPOSURE  = exposure;
        aec_state[0].ROI_T_INT_II = int_part;
        aec_state[0].ROI_T_INT_CLK = frac_part;
        set_time(aec_state[0].ROI_T_INT_II,aec_state[0].ROI_T_INT_CLK);
    }
}

void VSensor::exposure_set(float exposure)
{
    int    int_part;
    int    frac_part;

    int_part = (int)(exposure * EV76C661_CLK_CTRL) / (int)EV76C661_LINE_LENGTH;
    frac_part = (int)((exposure * EV76C661_CLK_CTRL) - (int_part*(int)EV76C661_LINE_LENGTH))/EV76C661_MULT_FACTOR;

    aec_state[0].ROI_T_INT_II = int_part;
    aec_state[0].ROI_T_INT_CLK = frac_part;
        
    set_time(aec_state[0].ROI_T_INT_II,aec_state[0].ROI_T_INT_CLK);
}

void VSensor::aec(int h, int w, uint8_t *pYplane)
{
    uint8_t* ptl_luma_plane = pYplane;

    uint32_t summ_luma= 0;
    uint32_t H64[64];// Histogram with 64  backets
    uint32_t CDF[64];// Cumulative Distribution Function
    uint32_t x,frame_number;
    int i,j,Mediana;

    /* Skip first frames */
    if(aec_state[0].FRAME<20) goto aec_tune_final;

    memset(CDF,0,64*sizeof(uint32_t));
    memset(H64,0,64*sizeof(uint32_t));
////////////////////////////////////////////////////////////////////////////////////
    //if( !get_histogram(ptl_luma_plane,H64,w,h) ) goto aec_tune_final;
    if( !get_histogram_emif(&frame_number,H64,w,h) ) goto aec_tune_final;
    get_cdf(H64,CDF, &summ_luma);
////////////////////////////////////////////////////////////////////////////////////
    
    /* Find median */
    x = 0;
    for(i=0; i<64; i++) {
        x += H64[i];
        if(x>((/*CDF[63]*/w*h)/2))break;
    }
    Mediana=(i+1)*4;// Why "4": Histogram has 64 buckets, Video has 10 bits, Rendered video has 8 bits
    printf("summ_luma=%6d, summ_histo=%6d, Mean=%6d Mediana=%4d (%9d)\n",summ_luma,CDF[63],CDF[63]==0?0:summ_luma/CDF[63],Mediana,x);
    
    /* Draw Histogramm */
    printf(" -- Histogramma -- %5.1f %5.1f %5.1f\n", 
                100.*((float)H64[0]/(float)(w*h)),
                100.*((float)(CDF[62]-H64[0])/(float)(w*h)),
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
    //aec_agc_algorithm_A(H64,Mediana,w,h);
    
    /* FrancenCam algorithm */
    //if( aec_agc_algorithm_B(CDF, 64, 0.8) ) aec_agc_set(aec_state[0].GAIN, aec_state[0].EXPOSURE, 3/* set gain and exposure */);
    if( aec_agc_algorithm_B(CDF, Mediana, 64, 0.25) ) aec_agc_set(aec_state[0].GAIN, aec_state[0].EXPOSURE, 3/* set exposure only*/);
    
    /* Modified FrankenCam: Y mediana used */
    //if( aec_agc_algorithm_C(CDF,Mediana,64,0.5) ) aec_agc_set(aec_state[0].GAIN, aec_state[0].EXPOSURE, 3);
    
    /* Controls integration time only (AA), Ga and Gd fixed */
    //if( aec_agc_algorithm_AA(H64,CDF,Mediana,w,h) ) aec_agc_set(aec_state[0].GAIN, aec_state[0].EXPOSURE, 3); 
//////////////////////////////////////////////////////////////////////

    printf("AutoExposure e2v: AG %2d   DG %3d [GAIN %6.3f / %6.3f]  EL %5d   EC %3d [EXPOSURE %8.3f / %8.3f]  FPS %4.1f Frame# %10d\n",
            aec_state[0].ROI_ANA_GAIN,aec_state[0].ROI_DIG_GAIN, aec_state[0].GAIN, aec_gain(0),
            aec_state[0].ROI_T_INT_II,aec_state[0].ROI_T_INT_CLK,aec_state[0].EXPOSURE*1000.0, aec_time(0)*1000.0,
            get_framerate(), frame_number 
          ); 
        
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
aec_tune_final:
    /*
     * TODO: for h=1024 it is better hide these lines.
     * Main task - to avoid blinking of these lines.
#ifdef FIRST_LINE_OK
    memcpy(ptl_luma_plane, ptl_luma_plane + w, w);
    memcpy(ptl_luma_plane + (w*(h-2)), ptl_luma_plane + (w*(h-3)), w);
    memcpy(ptl_luma_plane + (w*(h-1)), ptl_luma_plane + (w*(h-3)), w);
#else
    memcpy(ptl_luma_plane + (w*(h-3)), ptl_luma_plane + (w*(h-4)), w);
    memcpy(ptl_luma_plane + (w*(h-2)), ptl_luma_plane + (w*(h-4)), w);
    memcpy(ptl_luma_plane + (w*(h-1)), ptl_luma_plane + (w*(h-4)), w);
#endif
    */

    aec_state[0].FRAME++;// на всякий случай, на будущее...
    for(i=7; i>1; i--) memcpy( (void*)&aec_state[i], (void*)&aec_state[i-1], sizeof(aec_state[0]));
}

void VSensor::aec_II(int h, int w)
{
    uint32_t summ_luma= 0;
    uint32_t H64[64];// Histogram with 64  backets
    uint32_t CDF[64];// Cumulative Distribution Function
    uint32_t x,frame_number;
    int i,j;
    int Mediana, Mean;
    
    memset(CDF,0,64*sizeof(uint32_t));
    memset(H64,0,64*sizeof(uint32_t));

    if( get_histogram_emif(&frame_number,H64,w,h) ){
        
        if( frame_number - aec_state[1].FRAME > 2){
            aec_state[0].FRAME = frame_number;
            aec_state[0].NEXT = frame_number;//to enable aec_agc_algorithm_B() processing
            get_cdf(H64,CDF, &summ_luma);
            Mean = CDF[63]==0?0:summ_luma/CDF[63];
            Mediana = get_mediana(H64,w,h);
            printf("Frame %10d : Ymean=%4d(%4.1f) Ymediana=%4d(%4.1f)\n",frame_number,Mean,(float)CDF[Mean/4-1]/CDF[63] ,Mediana, (float)CDF[Mediana/4-1]/CDF[63]);
            
            //if( aec_agc_algorithm_B(CDF, Mediana, 64, 0.9) ) 
            //    aec_agc_set(aec_state[0].GAIN, aec_state[0].EXPOSURE, 3);
            aec_agc_algorithm_BB(CDF, Mediana, 64);
            
            printf("AutoExposure e2v: AG %2d   DG %3d [GAIN %6.3f / %6.3f]  EL %5d   EC %3d [EXPOSURE %8.3f / %8.3f]  FPS %4.1f Frame# %10d\n",
                aec_state[0].ROI_ANA_GAIN,aec_state[0].ROI_DIG_GAIN, aec_state[0].GAIN, aec_gain(0),
                aec_state[0].ROI_T_INT_II,aec_state[0].ROI_T_INT_CLK,aec_state[0].EXPOSURE*1000.0, aec_time(0)*1000.0,
                get_framerate(), frame_number 
            );
 
            for(i=7; i>1; i--) memcpy( (void*)&aec_state[i], (void*)&aec_state[i-1], sizeof(aec_state[0]));
        
        }
    }
    
    
}
