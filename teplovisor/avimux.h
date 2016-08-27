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
//	struct chunk_t {
//		std::streamoff off;
//		size_t sz;
//	};
	
public:
	AviMux(const std::string& fname);
	~AviMux();

	size_t add_frame(uint8_t* p, size_t s);

private:
	std::ofstream _fs;
//	std::vector<chunk_t> _offsets;
	
	void write_idx1(){}
	void write_hdrl();
	void open_movi();
	void close_movi(){}
};

class FileWriter
{
public:
	typedef std::pair<uint8_t*, timeval> buf_t;

	FileWriter(const timeval& ts);
	~FileWriter();

	operator bool();

	void add_frame(const v4l2_buffer& buf);
	buf_t next_frame();
private:
	struct cached_frame_t {
		int file_idx;
		size_t off;
		timeval ts;
	};

	void check_media();
	std::string gen_fname(const timeval& ts, const char* path);
	void delete_old_files(size_t target_size, const std::string& path);

	bool _media_is_ready;
	bool _media_was_checked;

	timeval _start;
	std::unique_ptr<AviMux> _muxer;

	buf_t _cache; // current frame if file isn't used (and if it's used as well). May be there should be std::circular_buffer
	std::deque<cached_frame_t> _frames; // i don't want to parse avis and moreover 1 hour of video at 8 fps is 28800 frames 
						// which means like 200Kb
	std::vector<std::string> _fnames;
	
	int _cur_file_idx = -1;
	std::ifstream _fstream;
};
