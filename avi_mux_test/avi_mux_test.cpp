#include "../teplovisor/avimux.h"

int main()
{
    AviMux mux("video.avi");

    const size_t s = 352*288;
    uint8_t p[s];
    for (int i=0; i<200; i++)
        mux.add_frame(p, s);
	
    return 0;
}
