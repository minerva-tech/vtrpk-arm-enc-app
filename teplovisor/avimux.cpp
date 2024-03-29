#include <ctime>
#include <linux/videodev2.h>

#include <mntent.h>

#include "log_to_file.h"
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

size_t AviMux::add_frame(uint8_t* p, size_t s)
{
//	_offsets.push_back(chunk_t{_fs.tellp(), s});

	struct chunk_hdr_t {
		char code[4] = {'0', '0', 'd', 'b'};
		uint32_t chunk_sz = 0;
	} chunk_hdr;

	chunk_hdr.chunk_sz = s;

	log() << "+Write chunk hdr";
	_fs.write(reinterpret_cast<const char*>(&chunk_hdr), sizeof(chunk_hdr));
	log() << "-Write chunk hdr";

	const size_t off = _fs.tellp();

	log() << "+Write data";
	_fs.write(reinterpret_cast<const char*>(p), s);
	log() << "-Write data";
	
	return off;
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
			int32_t  biHeight = -SRC_HEIGHT; // vertical flip
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

	log() << "+Write header";
	_fs.write(reinterpret_cast<const char*>(&header), sizeof(header));
	log() << "-Write header";
}

void AviMux::open_movi()
{
	struct movi_t {
		char list0[4] = {'L', 'I', 'S', 'T'};
		uint32_t list0_sz = 0;
		char movi[4] = {'m', 'o', 'v', 'i'};
	} movi;

	log() << "+Write movi";
	_fs.write(reinterpret_cast<const char*>(&movi), sizeof(movi));
	log() << "-Write movi";
}

FileWriter::FileWriter(const timeval& ts) :
	_start(ts),
	_media_is_ready(false),
	_media_was_checked(false)
{
//	char datetime[13]; // YYMMDD_HHmmss
//	std::strftime(datetime, 13, "%y%m%d_%H%M%S", std::localtime(&ts.tv_sec));

	std::strftime(_datec, 6, "%y%m%d", std::localtime(&ts.tv_sec));

	std::strftime(_time,  6, "%H%M%S", std::localtime(&ts.tv_sec));

	std::string name = gen_fname(AVI_FALLBACK_PATH);

	_copy_from = boost::filesystem::path(name);

	_muxer.reset(new AviMux(name));

	_streaming_mode = static_cast<utils::streaming_mode_t>(utils::get_streaming_mode());

	// check_that_media_is_available_and_set_the_flag();
}

FileWriter::~FileWriter()
{
	log() << "FileWriter::~FileWriter()";

	if (_copy_from) {
		if (_copy_to) {
			delete_old_files(target_size, AVI_PATH);

			log() << "Copy " << _copy_from->string() << " to " << _copy_to->string();

			boost::filesystem::copy_file(*_copy_from, *_copy_to);
		}

		log() << "Deleting " << *_copy_from;

		boost::filesystem::remove(*_copy_from);
	}
}

void FileWriter::check_media()
{
	_media_is_ready = false;

	FILE* mnt_file = setmntent(PROC_MOUNTS_PATH, "r");

	if (!mnt_file)
		return;

	mntent *ent = getmntent(mnt_file);
	while (true) {
		if (strcmp(ent->mnt_fsname, USB_DRIVE_DEV) == 0 && strcmp(ent->mnt_dir, AVI_PATH) == 0) {
			log() << "Media was mounted " << ent->mnt_fsname << " to " << ent->mnt_dir;
			break;
		}

		ent = getmntent(mnt_file);
		if (ent == NULL) {
			log() << "Media wasn't mounted, no files will be stored.";
			return;
		}
	}

	if (boost::filesystem::exists(AVI_PATH) && boost::filesystem::is_directory(AVI_PATH)) {
		const std::string fname = (boost::filesystem::path(AVI_PATH)/boost::filesystem::path("test.file")).native();
		const char test_string[] = "test string";
		{
			std::ofstream of(fname, std::ios_base::binary | std::ios_base::trunc);
			of.write(test_string, sizeof(test_string));
		}
		{
			std::ifstream fi(fname, std::ios_base::binary);
			char read_result[sizeof(test_string)] = {0};
			fi.read(read_result, sizeof(test_string));
			_media_is_ready = std::strcmp(test_string, read_result) == 0;
		}
	}
}

FileWriter::operator bool()
{
	// check_the_flag_which_was_set_in_ctor();
	return _media_is_ready;
}

std::string FileWriter::gen_fname(const char* path)
{
	std::string date = (boost::filesystem::path(path)/boost::filesystem::path(_datec)).native();

	if (boost::filesystem::exists(date)) {
		if (!boost::filesystem::is_directory(date)) {
			boost::filesystem::remove(date);
			// TODO : Clarify what we should do in this case
		}
	} else {
		log() << "+Create dir";
		boost::filesystem::create_directory(date);
		log() << "-Create dir";
	}

	char fragment_num[3] = "00";
	std::snprintf(fragment_num, 3, "%02i", _fragment_num);

	std::string fname = date + "/" + _time + "_" + fragment_num + ".avi";
	
	_fragment_num++;

	log() << "New segment file name: " << fname;

	if (boost::filesystem::exists(fname))
		boost::filesystem::remove_all(fname);

	_fnames.push_back(fname);

	return fname;
}

void FileWriter::delete_old_files(size_t target_size, const std::string& path)
{
	while (boost::filesystem::space(path).available < target_size) {
		log() << "No space available at " << path;

		boost::filesystem::path to_delete;
		std::time_t oldest = std::time(nullptr);

		boost::filesystem::recursive_directory_iterator dir_it(path);
		boost::system::error_code ec;

		for (; dir_it != boost::filesystem::recursive_directory_iterator(); ++dir_it) {
			boost::system::error_code ec;
			std::time_t t = boost::filesystem::last_write_time(*dir_it, ec);

			if (!ec && boost::filesystem::is_regular_file(*dir_it, ec) && t < oldest) {
				oldest = t;
				to_delete = dir_it->path();
			}
		}

		log() << "Trying to delete " << to_delete.string();

		const auto parent_path = to_delete.parent_path();

		log() << "+Delete file";
		boost::filesystem::remove(to_delete, ec);
		log() << "-Delete file";

		if (ec)
			log() << "Can't delete file : " << to_delete.string();

		if (boost::filesystem::is_empty(parent_path)) {
			log() << "+Delete dir";
			boost::filesystem::remove(parent_path, ec);
			log() << "-Delete dir";
		}

		return;
	}
}

void FileWriter::add_frame(const v4l2_buffer& buf)
{
	_cache.first = (uint8_t*)buf.m.userptr;
	_cache.second = buf.timestamp;

	bool force_segment = false;

	if (!_media_was_checked && difftime(buf.timestamp.tv_sec, _start.tv_sec) > AVI_FIRST_SEGMENT_DURATION) {
		::system((std::string("ntfs-3g ") + USB_DRIVE_DEV + std::string(" ") + AVI_PATH).c_str());

		check_media();

		_media_was_checked = true;

		if (_media_is_ready) {
			force_segment = true;

//			_copy_from = boost::filesystem::path(_fnames.front());
			_copy_to = boost::filesystem::path(AVI_PATH) /
				_copy_from->lexically_relative(boost::filesystem::path(AVI_FALLBACK_PATH));

			const boost::filesystem::path date = boost::filesystem::path(AVI_PATH) / 
					*std::next(_copy_from->rbegin());

			log() << "Creating directory " << date.string();

			if (boost::filesystem::exists(date) && !boost::filesystem::is_directory(date))
				boost::filesystem::remove(date);
			else
				boost::filesystem::create_directory(date);
		}// else {
//			_copy_from = boost::filesystem::path(_fnames.front()); // It's necessary for removing in the end even if flash isn't inserted
//		}

		auto hw_status = utils::get_hw_status();
		log() << "media was checked, hw state: " << (int)hw_status.last_run.data << ", " << (int)hw_status.this_run.data;
		hw_status.last_run.flash_avail 	= hw_status.this_run.flash_avail;
		hw_status.last_run.recovery 	= hw_status.this_run.recovery;

		hw_status.this_run.flash_avail 	= _media_is_ready;
		hw_status.this_run.recovery 	= utils::is_recovery(); // TODO : according to the mail of 16 oct 2016

		log() << "status was updated, hw state: " << (int)hw_status.last_run.data << ", " << (int)hw_status.this_run.data;
		utils::set_hw_status(hw_status);
	}

	if (_media_is_ready || !_media_was_checked) {
		if (force_segment || _media_is_ready && difftime(buf.timestamp.tv_sec, _start.tv_sec) > AVI_DURATION_MAX_SEC) {
			delete_old_files(target_size, AVI_PATH);
			_muxer.reset(new AviMux(gen_fname(AVI_PATH)));
			_start = buf.timestamp;
		}

		size_t off = _muxer->add_frame((uint8_t*)buf.m.userptr, SRC_WIDTH*SRC_HEIGHT/* buf.bytesused/3*2 */);

		_frames.push_back({_fnames.size()-1, off, buf.timestamp});
	}
}

FileWriter::buf_t FileWriter::next_frame()
{
//	if (_frames.size() == 1 || !_media_is_ready || _streaming_mode == streaming_mode_t::no_latency) { // < 1 probably if there is no storing device
	if (_frames.size() <= 1 || _streaming_mode == utils::streaming_mode_t::no_latency) { // < 1 probably if there is no storing device
		if (!_frames.empty())
			_frames.pop_front();
		return _cache;
	}

//	if (_frames.empty())
//		return {nullptr, {0,0}};

	auto& fr = _frames.front();

	if (fr.file_idx != _cur_file_idx) {
		_fstream.open(_fnames[fr.file_idx], std::ofstream::binary);
		_cur_file_idx = fr.file_idx;
	}

	if (_fstream.is_open()) {
		_fstream.seekg(fr.off);
		_fstream.read(reinterpret_cast<char*>(_cache.first), SRC_WIDTH*SRC_HEIGHT);
		_cache.second = fr.ts;
	} else {
		// TODO : ? skip till live edge?
		_cur_file_idx = -1;
	}
	
	_frames.pop_front();

	return _cache;
}
