#include <xdc/std.h>

#include "ext_headers.h"
#include "log_to_file.h"
#include "exception.h"
#include "comm.h"
#include "cap.h"
#include "enc.h"

int main(int argc, char *argv[])
{
	try {
		if (!Comm::instance().open("/dev/ttyS0"))
			throw ex("Cannot open serial port");

		CMEM_init();

		Enc enc;

		Cap cap(720, 480);

		cap.start_streaming();

     //   uint8_t buf[384*144*3/2];

		while(1) {
			v4l2_buffer buf = cap.getFrame();

//			throw ex("test ex");

			size_t coded_size=0;

			XDAS_Int8* bs = enc.encFrame((XDAS_Int8*)buf.m.userptr, 384, 144, 384, &coded_size);
	//		XDAS_Int8* bs = enc.encFrame((XDAS_Int8*)buf, 384, 144, 384, &coded_size);

//            log() << "Frame encoded, size: " << coded_size;

			cap.putFrame(buf);

			if (coded_size)
				Comm::instance().transmit(0, 0, coded_size, (uint8_t*)bs);

//			cap.putFrame(buf);
//			buf = cap.getFrame();
//			cap.putFrame(buf);
//			buf = cap.getFrame();

//			Comm::instance().transmit(0, 0, buf.length, (uint8_t*)buf.m.userptr); // test

//			enc.addFrame((uint8_t*)buf.m.userptr);
//			Payload p = end.getFrame();

//			comm().transmit(0, 0, p.l, p.p);
		}

//		cap.stop_streaming();
//	Enc enc;

//	cap.init();
//	enc.init();

//	enc.run();
	}
	catch (ex& e) {
		log() << e.str();
	}

	return 0;
}
