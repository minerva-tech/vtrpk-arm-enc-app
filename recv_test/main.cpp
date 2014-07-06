#include <termios.h>
#include <time.h>

#include <stdio.h>
#include <boost/asio.hpp>
#include <boost/cstdint.hpp>

int kbhit() {
    static bool inited = false;
    int left;

    if (!inited) {
        termios t;
        tcgetattr(0, &t);
        t.c_lflag &= ~ICANON;
        tcsetattr(0, TCSANOW, &t);
        setbuf(stdin, NULL);
        inited = true;
    }

    ioctl(0, FIONREAD, &left);

    return left;
}

int main(int argc, char **argv)
{
	if (argc != 5) {
		printf("a.out <port> <port bitrate> <output filename> <limit bitrate>\n");
		return 1;
	}

	FILE* f_out = fopen(argv[3], "wb");

	namespace asio = boost::asio;

	asio::io_service io_service;
	asio::serial_port port(io_service);
	boost::system::error_code ec;

	port.open(argv[1], ec);
	
	if (ec) {
		printf("Error:%s\n", ec.message().c_str());
		return 1;
	}

	const bool flow_control = true;

	port.set_option(asio::serial_port::baud_rate(atoi(argv[2])));
	port.set_option(asio::serial_port::character_size(8));
	port.set_option(asio::serial_port::flow_control(flow_control ? asio::serial_port::flow_control::hardware : asio::serial_port::flow_control::none));
	port.set_option(asio::serial_port::parity(asio::serial_port::parity::none));
	port.set_option(asio::serial_port::stop_bits(asio::serial_port::stop_bits::one));

	std::vector<uint8_t> buf;
	buf.resize(8);

	size_t overall_rcv = 0;
	
	struct timespec clock_res;
	clock_getres(CLOCK_REALTIME, &clock_res);
	
	struct timespec clock_start;
	clock_gettime(CLOCK_REALTIME, &clock_start); // anyway it's not portable due to ioctl
	
	const long long time_win = 20*1e6; // usec

	const size_t max_rcv = atoi(argv[4])*time_win/8/1e6; // bytes

	printf("max rcv per time frame: %i bytes\n", max_rcv);
	
	while (1) {
		if (kbhit())
			break;

		const size_t rcv = port.read_some(asio::buffer(buf), ec);
		
		fwrite(&buf[0], 1, rcv, f_out);

//		printf("received %i bytes\n", rcv);

		if (ec)
			printf("error : %s\n", ec.message().c_str());

		struct timespec clock_cur;
		clock_gettime(CLOCK_REALTIME, &clock_cur);
		
		const long long time_passed = (clock_cur.tv_nsec/1000 + clock_cur.tv_sec*1000000) -  // to usec
											(clock_start.tv_nsec/1000 + clock_start.tv_sec*1000000);

		if (time_passed >= time_win) {
			clock_start = clock_cur;
			overall_rcv = 0;
		}

		overall_rcv += rcv;
		
		printf("received %i bytes per %i us\n", overall_rcv, time_passed);

		if (overall_rcv > max_rcv) { // sleep till end of time win.
			const int hnd = port.native_handle();

			// termios should be more portable, but there is still no such header in win (even while it's POSIX? Not sure right now), so, ioctl
			int val = 0;
			ioctl(hnd, TIOCMGET, &val);
			val &= ~(TIOCM_CTS | TIOCM_RTS);
			ioctl(hnd, TIOCMSET, &val);
			
			printf("err: %i\n", errno);
			
			const timespec time_till_next_time_win = {
				(time_win - time_passed)/1000000,
				(time_win - time_passed)%1000000*1000,
			};
		
			printf("waiting %i sec, %i ns\n", time_till_next_time_win.tv_sec, time_till_next_time_win.tv_nsec);
	
			nanosleep(&time_till_next_time_win, nullptr); // i don't care about &rem
			
			val = 0;
			ioctl(hnd, TIOCMGET, &val);
			val |= TIOCM_CTS | TIOCM_RTS;
			ioctl(hnd, TIOCMSET, &val);
		}
	}
	
	fclose(f_out);
}
