#include "ext_headers.h"

#include "log_to_file.h"
#include "comm.h"
#include "proto.h"

int main(int argc, char **argv)
{
	if (argc != 5) {
		log() << "Usage: a.out <port> <baud rate> <enc config filename> <md config filename>";
		return 0;
	}

	log() << Comm::instance().open(argv[1], atoi(argv[2]), 0);

	Server server(nullptr);

	std::ifstream enc_f_in(argv[3]);
	const std::string enc_cfg((std::istreambuf_iterator<char>(enc_f_in)), std::istreambuf_iterator<char>());
	server.SendEncCfg(enc_cfg);

	std::ifstream md_f_in(argv[4]);
	const std::string md_cfg((std::istreambuf_iterator<char>(md_f_in)), std::istreambuf_iterator<char>());
	server.SendMDCfg(md_cfg);
	
	while (1) {}

    return 0;
}
