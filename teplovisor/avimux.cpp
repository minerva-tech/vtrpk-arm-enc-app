#include <ctime>
#include <linux/videodev2.h>

#include <boost/filesystem.hpp>

#include "defines.h"
#include "avimux.h"

AviMux::AviMux(const std::string& fname)
{
	_fs.open(fname, std::ofstream::binary);
	
	write_hdrl();
	
	open_movi();
}

AviMux::~AviMux()
{
	close_movi();
	
	write_idx1();
}

bool AviMux::add_frame(uint8_t* p, size_t s)
{
	_offsets.push_back(chunk_t{_fs.tellp(), s});

	struct chunk_hdr_t {
		char code[4] = {'0', '0', 'd', 'b'};
		uint32_t chunk_sz = 0;
	} chunk_hdr;
	
	chunk_hdr.chunk_sz = s;
	
	_fs.write(reinterpret_cast<const char*>(&chunk_hdr), sizeof(chunk_hdr));
	
	_fs.write(reinterpret_cast<const char*>(p), s);
	
	return _offsets.size()>AVI_LENGTH_MAX;
}

void AviMux::write_hdrl()
{
	struct hdr_t {
		char riff[4] = {'R', 'I', 'F' ,'F'};
		uint32_t riff_sz = 0;
		char avi[4] = {'A', 'V', 'I', ' '};
		char list0[4] = {'L', 'I', 'S', 'T'};
		uint32_t list0_sz = 0;
		char hdrl[4] = {'h', 'd', 'r', 'l'};
		char avih_name[4] = {'a', 'v', 'i', 'h'};
		uint32_t avih_sz = sizeof(avih);
		struct avih_t {
			uint32_t dwMicroSecPerFrame = 1000000 / FPS_MAX;
			uint32_t dwMaxBytesPerSec = 0;
			uint32_t dwPaddingGranularity = 1;
			uint32_t dwFlags = AVIF_HASINDEX;
			uint32_t dwTotalFrames = 0;
			uint32_t dwInitialFrames = 0;
			uint32_t dwStreams = 1;
			uint32_t dwSuggestedBufferSize = SRC_WIDTH*SRC_HEIGHT;
			uint32_t dwWidth = SRC_WIDTH;
			uint32_t dwHeight = SRC_HEIGHT;
			uint32_t dwReserved[4];
		} avih;

		char list1[4] = {'L', 'I', 'S', 'T'};
		uint32_t list1_sz = 0;
		char strl[4] = {'s', 't', 'r', 'l'};
		char strh_name[4] = {'s', 't', 'r', 'h'};
		uint32_t strh_sz = sizeof(strh);
		struct strh_t {
			char fccType[4] = {'v', 'i', 'd', 's'};
			char fccHandler[4] = {'R', 'G', 'B', '2'};
			uint32_t dwFlags = 0;
			uint16_t wPriority = 0;
			uint16_t wLanguage = 0;
			uint32_t dwInitialFrames = 0;
			uint32_t dwScale = 1;
			uint32_t dwRate = FPS_MAX;
			uint32_t dwStart = 0;
			uint32_t dwLength = 0;
			uint32_t dwSuggestedBufferSize = SRC_WIDTH*SRC_HEIGHT;
			uint32_t dwQuality = 0;
			uint32_t dwSampleSize = SRC_WIDTH*SRC_HEIGHT;
			struct RECT {
				int32_t left = 0;
				int32_t top = 0;
				int32_t right = SRC_WIDTH;
				int32_t bottom = SRC_HEIGHT;
			} rcFrame;
		} strh;
		char strf_name[4] = {'s', 't', 'r', 'f'};
		uint32_t strf_sz = sizeof(strf);
		struct strf_t { // bitmapinfoheader
			uint32_t biSize = sizeof(strf_t);
			int32_t  biWidth = SRC_WIDTH;
			int32_t  biHeight = SRC_HEIGHT;
			uint16_t biPlanes = 1;
			uint16_t biBitCount = 8;
			uint32_t biCompression = 0;
			uint32_t biSizeImage = SRC_WIDTH*SRC_HEIGHT;
			int32_t  biXPelsPerMeter = 0;
			int32_t  biYPelsPerMeter = 0;
			uint32_t biClrUsed = 0;
			uint32_t biClrImportant = 0;
		} strf;
	} header;

	_fs.write(reinterpret_cast<const char*>(&header), sizeof(header));
}

void AviMux::open_movi()
{
	struct movi_t {
		char list0[4] = {'L', 'I', 'S', 'T'};
		uint32_t list0_sz = 0;
		char movi[4] = {'m', 'o', 'v', 'i'};
	} movi;
	
	_fs.write(reinterpret_cast<const char*>(&movi), sizeof(movi));
}

FileWriter::FileWriter(const timeval& ts) :
	_start(ts),
	_muxer(new AviMux(gen_fname(ts)))
{
	::system("hwclock");
}

FileWriter::~FileWriter()
{	
}

std::string FileWriter::gen_fname(const timeval& ts)
{
//	char datetime[13]; // YYMMDD_HHmmss
//	std::strftime(datetime, 13, "%y%m%d_%H%M%S", std::localtime(&ts.tv_sec));

	char date[7] = {0};
	std::strftime(date, 6, "%y%m%d", std::localtime(&ts.tv_sec));

	char time[7] = {0};
	std::strftime(time, 6, "%H%M%S", std::localtime(&ts.tv_sec));

	if (boost::filesystem::exists(date)) {
		if (!boost::filesystem::is_directory(date)) {
			// TODO : Clarify what we should do in this case
		}
	} else {
		boost::filesystem::create_directory(date);
	}
	
	return std::string(date)+"/"+time+".avi";
}

void FileWriter::delete_old_files(size_t target_size)
{
	
}

void FileWriter::add_frame(const v4l2_buffer& buf)
{
	if (difftime(buf.timestamp.tv_sec, _start.tv_sec) > AVI_DURATION_MAX_SEC) {
		const size_t target_size = 0;
		delete_old_files(target_size);
		_muxer.reset(new AviMux(gen_fname(buf.timestamp)));
		_start = buf.timestamp;
	}

	_muxer->add_frame((uint8_t*)buf.m.userptr, SRC_WIDTH*SRC_HEIGHT/* buf.bytesused/3*2 */);
}
