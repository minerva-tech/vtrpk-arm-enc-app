#pragma once

#include <vector>
#include <fstream>
#include <memory>

#define 	AVIF_HASINDEX		0x00000010
#define 	AVIF_MUSTUSEINDEX   0x00000020
#define 	AVIF_ISINTERLEAVED  0x00000100
#define 	AVIF_TRUSTCKTYPE   	0x00000800
#define 	AVIF_WASCAPTUREFILE 0x00010000
#define 	AVIF_COPYRIGHTED   	0x00020000

class AviMux
{
	struct chunk_t {
		std::streamoff off;
		size_t sz;
	};
	
public:
	AviMux(const std::string& fname);
	~AviMux();

	bool add_frame(uint8_t* p, size_t s);

private:
	std::ofstream _fs;
	std::vector<chunk_t> _offsets;
	
	void write_idx1(){}
	void write_hdrl();
	void open_movi();
	void close_movi(){}
};

class FileWriter
{
public:
	FileWriter(const timeval& ts);
	~FileWriter();

	void add_frame(const v4l2_buffer& buf);
private:
	std::string gen_fname(const timeval& ts);
	void delete_old_files(size_t target_size);

	timeval _start;
	std::unique_ptr<AviMux> _muxer;
};
