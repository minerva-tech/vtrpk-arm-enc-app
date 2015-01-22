/*
 * cap.h
 *
 *  Created on: Jan 8, 2013
 *      Author: a
 */

#ifndef CAP_H
#define CAP_H

#include <memory>

#include <linux/videodev2.h>
#include <linux/media.h>

#include "cmem.h"

const int APP_NUM_BUFS = 3;

struct CaptureBuf {
	typedef std::auto_ptr<CaptureBuf> ptr;

	CaptureBuf(size_t size, CMEM_AllocParams& alloc_params);
	~CaptureBuf();

	size_t				size;
	void*				user_addr;
	unsigned long 		phy_addr;
	CMEM_AllocParams 	alloc_params;
};

struct CaptureFd {
	CaptureFd(const std::string& name);
	~CaptureFd();

	void open(int flags);

	operator int() {return id;}

	int id;

	std::string name;
};

class Cap {
public:
	Cap(int w, int h, int dst_w, int dst_h);
	~Cap();

	void start_streaming();
	void stop_streaming();

	void putFrame(const v4l2_buffer& buf);
	v4l2_buffer getFrame();

private:
	int m_width;
	int m_height;
	int m_dst_width;
	int m_dst_height;
	
	long long m_prev_time;
	long long m_diff_time;

	CaptureBuf::ptr m_capture_buffers[APP_NUM_BUFS];

	CaptureFd m_rsz_fd;
	CaptureFd m_prv_fd;
	CaptureFd m_ccdc_fd;
//	CaptureFd m_tvp_fd;
	CaptureFd m_teplovisor_fd;
	CaptureFd m_capt_fd;
	CaptureFd m_media_fd;

	media_entity_desc	m_entity[15];
	int 			m_entities_count;

	int E_VIDEO;
//	int E_TVP514X;
	int E_TEPLOVISOR;
	int E_VRTVK_EV76C661_FPGA;
	int E_CCDC;
	int E_PRV;
	int E_RSZ;

	void allocate_user_buffers(int buf_size);
	void open_media_device(const std::string& name);
	void enumerate_media_entities();
	void enumerate_links();
	void enable_links();
	void init_resizer();
	void init_prv();
	void set_capture_inputs();
	void set_formats();
	void init_streaming_bufs();

	void break_links();
};

#endif //CAP_H
