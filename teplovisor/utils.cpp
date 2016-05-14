#include "utils.h"

namespace utils {

void LED_RXD(int on)
{
    FILE* fled = fopen("/sys/class/leds/rxd/brightness", "wt");
    if(NULL==fled) log()<< " - LED RXD open failed";

    if(on)
        fprintf(fled, "1");
    else
        fprintf(fled, "0");

	fflush(fled);
    fclose(fled);
    //boost::this_thread::sleep_for(boost::chrono::milliseconds(1750));
}

void LED_ERR(int on)
{
    FILE* fled = fopen("/sys/class/leds/error/brightness", "wt");
    if(NULL==fled) log()<< " - LED ERR open failed";

    if(on)
        fprintf(fled, "1");
    else
        fprintf(fled, "0");

	fflush(fled);
    fclose(fled);
    //boost::this_thread::sleep_for(boost::chrono::milliseconds(1750));
}

} // namespace utils