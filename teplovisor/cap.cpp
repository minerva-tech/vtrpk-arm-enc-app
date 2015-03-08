/*
 * cap.cpp
 *
 *  Created on: Jan 8, 2013
 *      Author: a
 */

//#include <stdio.h>
//#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
//#include <unistd.h>
//#include <stdlib.h>
#include <linux/videodev.h>
#include <linux/videodev2.h>
#include <media/davinci/imp_previewer.h>
#include <media/davinci/imp_resizer.h>
#include <media/davinci/dm365_ipipe.h>
#include <media/davinci/vpfe_capture.h>
#include <linux/v4l2-subdev.h>
#include <media/davinci/videohd.h> //NAG

#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include "common.h"

#include "exception.h"
#include "log_to_file.h"
#include "cap.h"

#include "defines.h"

#define ALIGN(x, y)	(((x + (y-1))/y)*y)

#if VIDEO_SENSOR
const int CODE  = V4L2_MBUS_FMT_YUYV10_1X20;
const int CODE2 = V4L2_MBUS_FMT_YUYV8_2X8;
#else
const int CODE  = V4L2_MBUS_FMT_YUYV8_2X8;
const int CODE2 = V4L2_MBUS_FMT_YUYV8_2X8;
#endif

Cap::Cap(int w, int h, int dst_w, int dst_h) :
m_width(w),
m_height(h),
m_dst_width(dst_w),
m_dst_height(dst_h),
m_prev_time(0),
m_diff_time(0),
m_rsz_fd("/dev/v4l-subdev3"),
m_prv_fd("/dev/v4l-subdev2"),
m_ccdc_fd("/dev/v4l-subdev1"),
//m_tvp_fd("/dev/v4l-subdev0"),
m_teplovisor_fd("/dev/v4l-subdev0"),
m_capt_fd("/dev/video4"),
m_media_fd("/dev/media0")
{
	log() << "src_w : " << m_width << " src_h : " << m_height;
	log() << "dst_w : " << m_dst_width << " dst_h : " << m_dst_height;
	
	m_media_fd.open(O_RDWR);

	break_links();

	allocate_user_buffers(ALIGN((m_width*m_height*2), 4096)); // ? *3/2

	enumerate_media_entities();

	enumerate_links();

	enable_links();

	m_rsz_fd.open(O_RDWR);

	m_prv_fd.open(O_RDWR);

	m_capt_fd.open(O_RDWR | O_NONBLOCK);

	init_resizer();

	init_prv();

	set_capture_inputs();

	set_formats();

	init_streaming_bufs();
}

Cap::~Cap()
{
	stop_streaming();

	break_links();
}

extern "C" CMEM_AllocParams memParams;

void Cap::allocate_user_buffers(int buf_size)
{
	void *pool;
	int i;

/*	CMEM_AllocParams  alloc_params;
	alloc_params.type = CMEM_POOL;
	alloc_params.flags = CMEM_NONCACHED;
	alloc_params.alignment = 32;
*/
//    alloc_params = memParams;

	log() << "calling cmem utilities for allocating frame buffers";

//	CMEM_init();

/*    int pool_idx = CMEM_getPool(APP_NUM_BUFS*buf_size);

    if (pool_idx<0)
        throw ex("CMEM_getPool failed");

	pool = CMEM_allocPool(pool_idx, &alloc_params);

	if (!pool)
		throw ex("Failed to allocate cmem pool");
*/
	log() << "Allocating capture buffers, buf size = " << buf_size;

	for (int i=0; i<APP_NUM_BUFS; i++) {
//		m_capture_buffers[i] = CaptureBuf::ptr(new CaptureBuf(buf_size, alloc_params));
		m_capture_buffers[i] = CaptureBuf::ptr(new CaptureBuf(buf_size, memParams));

		log() << "Got " << m_capture_buffers[i]->user_addr << " from CMEM, phy = " << (void *)m_capture_buffers[i]->phy_addr;
	}
}

void Cap::enumerate_media_entities()
{
	log() << "enumerating media entities";

	int index = 0;

	int ret = 0;

	do {

/* Entities can be enumerated by or'ing the id with the MEDIA_ENT_ID_FLAG_NEXT flag. The driver will return information about the entity with the smallest id strictly larger than the requested one ('next entity'), or the EINVAL error code if there is none.

Entity IDs can be non-contiguous. Applications must not try to enumerate entities by calling MEDIA_IOC_ENUM_ENTITIES with increasing id's until they get an error.

// Probably i don't understand something but code below works slightly another way.
*/
	log() << "index = " << index;

	memset(&m_entity[index], 0, sizeof(m_entity[index]));

	m_entity[index].id = index | MEDIA_ENT_ID_FLAG_NEXT;

	ret = ioctl(m_media_fd, MEDIA_IOC_ENUM_ENTITIES, &m_entity[index]);

	log() << "ioctl ok";

	if (ret < 0) {
		if (errno == EINVAL)
			break;
		else
			throw ex("Media entities enum error");
	} else {
		if (!strcmp(m_entity[index].name, E_VIDEO_RSZ_OUT_NAME)) {
			E_VIDEO = m_entity[index].id;
		}
		else if (!strcmp(m_entity[index].name, E_TEPLOVISOR_NAME)) {
			E_TEPLOVISOR = m_entity[index].id;
		}
		else if (!strcmp(m_entity[index].name, E_VRTVK_EV76C661_FPGA_NAME)) {
			E_VRTVK_EV76C661_FPGA = m_entity[index].id;
		}
		else if (!strcmp(m_entity[index].name, E_CCDC_NAME)) {
			E_CCDC = m_entity[index].id;
		}
		else if (!strcmp(m_entity[index].name, E_PRV_NAME)) {
			E_PRV = m_entity[index].id;
		}
		else if (!strcmp(m_entity[index].name, E_RSZ_NAME)) {
			E_RSZ = m_entity[index].id;
		}
		log() << "[" << m_entity[index].id << "]:" << m_entity[index].name;
	}

	index++;

	} while (ret == 0 && index<16);

	m_entities_count = index;
	log() << "total number of entities: " << m_entities_count;
}

void Cap::enumerate_links()
{
	log() << "enumerating links/pads for entities";

	for(int index = 0; index < m_entities_count; index++) {

		media_links_enum links;

		links.entity = m_entity[index].id;

		links.pads  = new media_pad_desc[m_entity[index].pads];
		links.links = new media_link_desc[m_entity[index].links];

		int ret = ioctl(m_media_fd, MEDIA_IOC_ENUM_LINKS, &links);

		if (ret < 0) {
			if (errno == EINVAL)
				break;
		}else{
			/* display pads info first */
			if(m_entity[index].pads)
				log() << "pads for entity " << m_entity[index].id;

			for(int i = 0; i < m_entity[index].pads; i++)
				log() << "(" << links.pads[i].index << ", " << ((links.pads[i].flags & MEDIA_PAD_FL_INPUT) ? "INPUT" : "OUTPUT") << ")";

			/* display links now */
			for(int i = 0; i < m_entity[index].links; i++)
			{
				log() << "[" << links.links[i].source.entity << ":" << links.links[i].source.index << "]-------------->[" <<
						links.links[i].sink.entity << ":" << links.links[i].sink.index << "]";

				if(links.links[i].flags & MEDIA_LNK_FL_ENABLED)
					log() << "ACTIVE";
				else
					log() << "INACTIVE";
			}
		}

		delete[] links.links;
		delete[] links.pads;
	}
}

void Cap::enable_links() // todo: these links wont break if exception will occur later. RAII?
{
	log() << "ENABLEing link [teplovisor]----------->[ccdc]";

	media_link_desc link;
	memset(&link, 0, sizeof(link));

#if VIDEO_SENSOR
	link.flags |=  MEDIA_LNK_FL_ENABLED;
	link.source.entity = E_VRTVK_EV76C661_FPGA;
	link.source.index = P_VRTVK_EV76C661_FPGA;
	link.source.flags = MEDIA_PAD_FL_OUTPUT;
#else
	link.flags |=  MEDIA_LNK_FL_ENABLED;
	link.source.entity = E_TEPLOVISOR;
	link.source.index = P_TEPLOVISOR;
	link.source.flags = MEDIA_PAD_FL_OUTPUT;
#endif

	link.sink.entity = E_CCDC;
	link.sink.index = P_CCDC_SINK;
	link.sink.flags = MEDIA_PAD_FL_INPUT;

	if(ioctl(m_media_fd, MEDIA_IOC_SETUP_LINK, &link))
		throw ex("failed to enable link between teplovisor and ccdc");
	else
		log() << "[teplovisor]----------->[ccdc] ENABLED";

	log() << "ENABLEing link [CCDC]----------->[PRV]";

	memset(&link, 0, sizeof(link));

	link.flags |=  MEDIA_LNK_FL_ENABLED;
	link.source.entity = E_CCDC;
	link.source.index = P_CCDC_SOURCE;
	link.source.flags = MEDIA_PAD_FL_OUTPUT;

	link.sink.entity = E_PRV;
	link.sink.index = P_PRV_SINK;
	link.sink.flags = MEDIA_PAD_FL_INPUT;

	if(ioctl(m_media_fd, MEDIA_IOC_SETUP_LINK, &link))
		throw ex("failed to enable link between ccdc and previewer");
	else
		log() << "[ccdc]----------->[prv] ENABLED";

	log() << "ENABLEing link [prv]----------->[rsz]";
	memset(&link, 0, sizeof(link));

	link.flags |=  MEDIA_LNK_FL_ENABLED;
	link.source.entity = E_PRV;
	link.source.index = P_PRV_SOURCE;
	link.source.flags = MEDIA_PAD_FL_OUTPUT;

	link.sink.entity = E_RSZ;
	link.sink.index = P_RSZ_SINK;
	link.sink.flags = MEDIA_PAD_FL_INPUT;

	if(ioctl(m_media_fd, MEDIA_IOC_SETUP_LINK, &link)) {
		throw ex("failed to enable link between prv and rsz\n");
	} else
		log() << "[prv]----------->[rsz] ENABLED";

	log() << "ENABLEing link [rsz]----------->[video_node]";
	memset(&link, 0, sizeof(link));

	link.flags |=  MEDIA_LNK_FL_ENABLED;
	link.source.entity = E_RSZ;
	link.source.index = P_RSZ_SOURCE;
	link.source.flags = MEDIA_PAD_FL_OUTPUT;

	link.sink.entity = E_VIDEO;
	link.sink.index = P_VIDEO;
	link.sink.flags = MEDIA_PAD_FL_INPUT;

	if(ioctl(m_media_fd, MEDIA_IOC_SETUP_LINK, &link))
		throw ex("failed to enable link between rsz and video node");
	else
		log() << "[rsz]----------->[video_node] ENABLED";
}

void Cap::init_resizer()
{
	rsz_channel_config rsz_chan_config;
	rsz_continuous_config rsz_cont_config;

	/* 14. set default configuration in rsz */
	//rsz_chan_config.oper_mode = IMP_MODE_CONTINUOUS;
	rsz_chan_config.chain = 1;
	rsz_chan_config.len = 0;
	rsz_chan_config.config = NULL;
	if (ioctl(m_rsz_fd, RSZ_S_CONFIG, &rsz_chan_config) < 0)
		throw ex("failed to set default configuration in resizer");
	else
		log() << "default configuration setting in resizer successful";

	/* 15. get configuration from rsz */
	bzero(&rsz_cont_config, sizeof(rsz_continuous_config));
	//rsz_chan_config.oper_mode = IMP_MODE_CONTINUOUS;
	rsz_chan_config.chain = 1;
	rsz_chan_config.len = sizeof(rsz_continuous_config);
	rsz_chan_config.config = &rsz_cont_config;

	if (ioctl(m_rsz_fd, RSZ_G_CONFIG, &rsz_chan_config) < 0)
		throw ex("failed to get resizer channel configuration");
	else
		log() << "confgiration got from resizer successfully";

	/* 16. set configuration in rsz */
	rsz_cont_config.output1.enable = 1;
	rsz_cont_config.output2.enable = 0;

	//rsz_chan_config.oper_mode = IMP_MODE_CONTINUOUS;
	rsz_chan_config.chain = 1;
	rsz_chan_config.len = sizeof(struct rsz_continuous_config);
	rsz_chan_config.config = &rsz_cont_config;
	if (ioctl(m_rsz_fd, RSZ_S_CONFIG, &rsz_chan_config) < 0)
		throw ex("failed to set configuration in resizer");
	else
		log() << "successfully set configuration in rsz";
}

void Cap::init_prv()
{
	prev_channel_config prev_chan_config;

	/* 21. set default configuration in PRV */
	//prev_chan_config.oper_mode = IMP_MODE_CONTINUOUS;
	prev_chan_config.len = 0;
	prev_chan_config.config = NULL; /* to set defaults in driver */
	if (ioctl(m_prv_fd, PREV_S_CONFIG, &prev_chan_config) < 0)
		throw ex("failed to set default configuration on prv\n");
	else
		log() << "default configuration is set successfully on prv";

	prev_continuous_config prev_cont_config;

	/* 22. get configuration from prv */
	//prev_chan_config.oper_mode = IMP_MODE_CONTINUOUS;
	prev_chan_config.len = sizeof(struct prev_continuous_config);
	prev_chan_config.config = &prev_cont_config;

	if (ioctl(m_prv_fd, PREV_G_CONFIG, &prev_chan_config) < 0)
		throw ex("failed to get default configuration from prv\n");
	else
		log() << "got configuration from prv successfully";

	/* 23. set configuration in prv */
	//prev_chan_config.oper_mode = IMP_MODE_CONTINUOUS;
	prev_chan_config.len = sizeof(prev_continuous_config);
	prev_chan_config.config = &prev_cont_config;

	if (ioctl(m_prv_fd, PREV_S_CONFIG, &prev_chan_config) < 0)
		throw ex("failed to set configuration on prv");
	else
		log() << "configuration is set successfully on prv";

	prev_cap cap;
	prev_module_param mod_param;

	/* 24. setting default prv-params */
	cap.index=0;
	while (1) {
		if (ioctl(m_prv_fd , PREV_ENUM_CAP, &cap) < 0) {
			break;
		}

		strcpy(mod_param.version, cap.version);
		mod_param.module_id = cap.module_id;

		log() << "setting default for prv-module " << cap.module_name;

		mod_param.param = NULL;

		if (ioctl(m_prv_fd, PREV_S_PARAM, &mod_param) < 0)
			throw ex("error in Setting prv-params from driver");

		cap.index++;
	}
}

void Cap::set_capture_inputs()
{
	log() << "enumerating INPUTS";

	v4l2_input input;

	memset(&input, 0, sizeof(input));

	input.type = V4L2_INPUT_TYPE_CAMERA;
	input.index = 0;
	int index = 0;
  	while (1) {
		int ret = ioctl(m_capt_fd, VIDIOC_ENUMINPUT, &input);
		if(ret != 0)
			break;

		log() << "[" << index << "]." << input.name;
//		sleep(1); // ehm. ok.

		memset(&input, 0, sizeof(input));
		index++;
		input.index = index;
  	}

	log() << "setting COMPOSITE input.";
	memset(&input, 0, sizeof(input));
	input.type = V4L2_INPUT_TYPE_CAMERA;
	input.index = 0;
	if (-1 == ioctl (m_capt_fd, VIDIOC_S_INPUT, &input.index))
		throw ex("failed to set COMPOSITE with capture device");
	else
		log() << "successfully set COMPOSITE input";

#if not VIDEO_SENSOR
	log() << "setting NTSC std.";

	v4l2_std_id cur_std;
	memset(&cur_std, 0, sizeof(cur_std));

	cur_std = V4L2_STD_NTSC;
	if (-1 == ioctl(m_capt_fd, VIDIOC_S_STD, &cur_std))
		throw ex("failed to set NTSC std on capture device\n");
	else
		log() << "successfully set NTSC std.";
#endif
}

static v4l2_subdev_format fmt(__u32 pad, __u32 which, __u32 code, __u32 w, __u32 h, __u32 colorspace, __u32 field)
{
	v4l2_subdev_format fmt;

	memset(&fmt, 0, sizeof(fmt));

	fmt.pad = pad;
	fmt.which = which;
	fmt.format.code = code;
	fmt.format.width = w;
	fmt.format.height = h;
	fmt.format.colorspace = colorspace;
	fmt.format.field = field;

	return fmt;
}

void Cap::set_formats()
{
	log() << "setting format on pad of teplovisor entity.";

//	m_tvp_fd.open(O_RDWR);
	m_teplovisor_fd.open(O_RDWR);

	v4l2_subdev_format f;

//	f = fmt(P_TVP514X, V4L2_SUBDEV_FORMAT_ACTIVE, CODE, m_width, m_height, V4L2_COLORSPACE_SMPTE170M, V4L2_FIELD_INTERLACED);
#if VIDEO_SENSOR
	f = fmt(P_VRTVK_EV76C661_FPGA, V4L2_SUBDEV_FORMAT_ACTIVE, CODE, m_width, m_height, V4L2_COLORSPACE_SMPTE170M, V4L2_FIELD_NONE);
#else
	f = fmt(P_TEPLOVISOR, V4L2_SUBDEV_FORMAT_ACTIVE, CODE, m_width, m_height, V4L2_COLORSPACE_SMPTE170M, V4L2_FIELD_NONE);
#endif
	int ret = ioctl(m_teplovisor_fd, VIDIOC_SUBDEV_S_FMT, &f);
	if(ret)
		throw ex("failed to set format on pad");// %x\n", fmt.pad);
	else
		log() << "successfully format is set on pad %x" << f.pad;

	m_ccdc_fd.open(O_RDWR);

	log() << "setting format on sink-pad of ccdc entity.";

	f = fmt(P_CCDC_SINK, V4L2_SUBDEV_FORMAT_ACTIVE, CODE, m_width, m_height, V4L2_COLORSPACE_SMPTE170M, V4L2_FIELD_NONE);
	ret = ioctl(m_ccdc_fd, VIDIOC_SUBDEV_S_FMT, &f);
	if(ret)
		throw ex("failed to set format on pad");// %x\n", fmt.pad);
	else
		log() << "successfully format is set on pad " << f.pad;

	log() << "setting format on OF-pad of ccdc entity.";

	f = fmt(P_CCDC_SOURCE, V4L2_SUBDEV_FORMAT_ACTIVE, CODE2, m_width, m_height, V4L2_COLORSPACE_SMPTE170M, V4L2_FIELD_NONE);
	ret = ioctl(m_ccdc_fd, VIDIOC_SUBDEV_S_FMT, &f);
	if(ret)
		throw ex("failed to set format on pad");// %x\n", fmt.pad);
	else
		log() << "successfully format is set on pad " << f.pad;

	log() << "setting format on sink-pad of prv entity.";

	f = fmt(P_PRV_SINK, V4L2_SUBDEV_FORMAT_ACTIVE, CODE2, m_width, m_height, V4L2_COLORSPACE_SMPTE170M, V4L2_FIELD_NONE);
	ret = ioctl(m_prv_fd, VIDIOC_SUBDEV_S_FMT, &f);
	if(ret)
		throw ex("failed to set format on pad");// %x\n", fmt.pad);
	else
		log() << "successfully format is set on pad " << f.pad;

	log() << "setting format on source-pad of prv entity.";

	f = fmt(P_PRV_SOURCE, V4L2_SUBDEV_FORMAT_ACTIVE, CODE2, m_width, m_height, V4L2_COLORSPACE_SMPTE170M, V4L2_FIELD_NONE);
	ret = ioctl(m_prv_fd, VIDIOC_SUBDEV_S_FMT, &f);
	if(ret)
		throw ex("failed to set format on pad");// %x\n", fmt.pad);
	else
		log() << "successfully format is set on pad " << f.pad;

	log() << "setting format on sink-pad of rsz entity.";

	f = fmt(P_RSZ_SINK, V4L2_SUBDEV_FORMAT_ACTIVE, CODE2, m_width, m_height, V4L2_COLORSPACE_SMPTE170M, V4L2_FIELD_NONE);
	ret = ioctl(m_rsz_fd, VIDIOC_SUBDEV_S_FMT, &f);
	if(ret)
		throw ex("failed to set format on pad");//%x\n", fmt.pad);
	else
		log() << "successfully format is set on pad " << f.pad;

	log() << "setting format on source-pad of rsz entity.";

	f = fmt(P_RSZ_SOURCE, V4L2_SUBDEV_FORMAT_ACTIVE, V4L2_MBUS_FMT_NV12_1X20, m_dst_width, m_dst_height, V4L2_COLORSPACE_SMPTE170M, V4L2_FIELD_NONE);
	ret = ioctl(m_rsz_fd, VIDIOC_SUBDEV_S_FMT, &f);
	if(ret)
		throw ex("failed to set format on pad");// %x\n", f.pad);
	else
		log() << "successfully format is set on pad " << f.pad;

	log() << "15.setting format V4L2_PIX_FMT_UYVY";

	v4l2_format v4l2_fmt;
	memset(&v4l2_fmt, 0, sizeof(v4l2_fmt));

	v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	v4l2_fmt.fmt.pix.width = m_dst_width;
	v4l2_fmt.fmt.pix.height = m_dst_height;
	//v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
	v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
	v4l2_fmt.fmt.pix.field = V4L2_FIELD_NONE;

	if (-1 == ioctl(m_capt_fd, VIDIOC_S_FMT, &v4l2_fmt))
		throw ex("failed to set format on captute device");
	else
		log() << "successfully set the format";

	if (-1 == ioctl(m_capt_fd, VIDIOC_G_FMT, &v4l2_fmt))
		throw ex("failed to get format from captute device");
	else
		log() << "capture_pitch: " << v4l2_fmt.fmt.pix.bytesperline;
//		capture_pitch = v4l2_fmt.fmt.pix.bytesperline;
}


void Cap::init_streaming_bufs() // todo: do it via putFrame()
{
	log() << "make sure 3 buffers are supported for streaming";

	v4l2_requestbuffers req;

	memset(&req, 0, sizeof(req));

	req.count = APP_NUM_BUFS;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	if (-1 == ioctl(m_capt_fd, VIDIOC_REQBUFS, &req))
		throw ex("call to VIDIOC_REQBUFS failed");

	if (req.count != APP_NUM_BUFS)
		throw ex("3 buffers not supported by capture device");
	else
		log() << "3 buffers are supported for streaming";

	/* 34.queue the buffers */
	for (int i = 0; i < APP_NUM_BUFS; i++) {
		v4l2_buffer buf;
		memset(&buf, 0, sizeof(buf));

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;
		buf.index = i;
		buf.length = m_capture_buffers[i]->size;
		buf.m.userptr = (unsigned long)m_capture_buffers[i]->user_addr;

		if (-1 == ioctl(m_capt_fd, VIDIOC_QBUF, &buf))
			throw ex("call to VIDIOC_QBUF failed");
	}
}

void Cap::start_streaming()
{
	log() << "start streaming";

	const v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == ioctl(m_capt_fd, VIDIOC_STREAMON, &type))
		throw ex("failed to start streaming on capture device");
	else
		log() << "streaming started successfully";
}

v4l2_buffer Cap::getFrame()
{
	v4l2_buffer buf;

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_USERPTR;

	int ret = -1;
	do {
		ret = ioctl(m_capt_fd, VIDIOC_DQBUF, &buf);
		if (ret<0 && errno != EAGAIN)
			if (errno == EIO)
				log() << "EIO was received in Cap::getFrame. Were some frames lost?";
			else
				throw ex("Failed to DQ buffer from capture device. errno = " + boost::lexical_cast<std::string>(errno));
		boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
	} while (ret < 0);

	const long long curr_time = buf.timestamp.tv_sec*1000000 + buf.timestamp.tv_usec;

//	if (curr_time - m_prev_time >= 3*m_diff_time/2)
//		log() << "Frame duration : " << (curr_time-m_prev_time)/1000 << " ms, instead of : " << m_diff_time/1000 << " ms. Frame was dropped, probably";

	m_diff_time = curr_time-m_prev_time;
	m_prev_time = curr_time;

	return buf;
}

void Cap::putFrame(const v4l2_buffer& buf)
{
	if (ioctl(m_capt_fd, VIDIOC_QBUF, &buf) < 0)
		throw ex("failed to Q buffer onto capture device. errno = " + errno);
}

void Cap::stop_streaming()
{
	log() << "stop streaming";

	const v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == ioctl(m_capt_fd, VIDIOC_STREAMOFF, &type))
		throw ex("failed to stop streaming on capture device");
	else
		log() << "streaming stopped successfully";
}

void Cap::break_links()
{
	for(int index = 0; index < m_entities_count; index++) {

		media_links_enum links;

		links.entity = m_entity[index].id;

		links.pads  = new media_pad_desc[m_entity[index].pads];
		links.links = new media_link_desc[m_entity[index].links];

		if (ioctl(m_media_fd, MEDIA_IOC_ENUM_LINKS, &links) < 0) {
			if (errno == EINVAL)
				break;
		}else{
			for(int i = 0; i < m_entity[index].links; i++)
			{
				media_link_desc link;

				if(links.links[i].flags & MEDIA_LNK_FL_ENABLED) {
					/* de-enable the link */
					memset(&link, 0, sizeof(link));

					link.flags |= ~MEDIA_LNK_FL_ENABLED;
					link.source.entity = links.links[i].source.entity;
					link.source.index = links.links[i].source.index;
					link.source.flags = MEDIA_PAD_FL_OUTPUT;

					link.sink.entity = links.links[i].sink.entity;
					link.sink.index = links.links[i].sink.index;
					link.sink.flags = MEDIA_PAD_FL_INPUT;

					if(ioctl(m_media_fd, MEDIA_IOC_SETUP_LINK, &link))
						log() << "failed to de-enable link";
				}
			}
		}

		delete[] links.links;
		delete[] links.pads;
	}
}

CaptureBuf::CaptureBuf(size_t size_, CMEM_AllocParams& alloc_params_) :
		size(size_),
		user_addr(NULL),
		phy_addr(0),
		alloc_params(alloc_params_)
{
	user_addr = CMEM_alloc(size, &alloc_params);

	if (user_addr) {
		phy_addr = CMEM_getPhys(user_addr);
		if (!phy_addr)
			throw ex("Failed to get phy cmem buffer address");
	} else {
		throw ex("Failed to allocate cmem buffer.");
	}
}

CaptureBuf::~CaptureBuf()
{
	if (user_addr)
		CMEM_free(user_addr, &alloc_params);
}

CaptureFd::CaptureFd(const std::string& name_) :
	id(-1),
	name(name_)
{
//	id = open(name.c_str(), O_RDWR | O_NONBLOCK, 0);
//	if(id == -1)
//		throw ex("failed to open " + name);
}

void CaptureFd::open(int flags) {
	id = ::open(name.c_str(), flags);
	if(id < 0)
		throw ex("failed to open " + name);

	log() << "file has been opened " << name << " id " << id;
}

CaptureFd::~CaptureFd()
{
	if (id>=0) {
		close(id);
		log() << "file has been closed " << id;
	}
}
