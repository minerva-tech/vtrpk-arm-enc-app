/*
 * enc.h
 *
 *  Created on: Jan 8, 2013
 *      Author: a
 */

#ifndef ENC_H
#define ENC_H

#include "h264venc.h"
#include "cmem_buf.h"

struct XDM_Buf {
	XDM_Buf() {
		buf.bufSizes = NULL;
		buf.bufs = NULL;
	}
	~XDM_Buf() {
		delete[] buf.bufSizes;
		delete[] buf.bufs;
	}

	XDM_BufDesc* operator->() { return &buf; }

	XDM_BufDesc	buf;
};

class Enc {
public:
	Enc();
	~Enc();

	XDAS_Int8* encFrame(XDAS_Int8* in, int width, int height, int stride, size_t* out_size);

	static void rman_init(); // unfortunately i wasn't able to init/exit RMAN each time when encoder restarted. So it's init once, when app is started. Yes, it's ugly.

private:
	H264VENC_Handle			m_handle;
	IH264VENC_Fxns			m_fxns;
	H264VENC_Params			m_params;
	H264VENC_DynamicParams	m_dynamicparams;
	H264VENC_InArgs 		m_inargs;
	H264VENC_OutArgs 		m_outargs;

	int 					m_uiNumFramesToBeEncoded;

	int						m_uiExtWidth;
	int						m_uiExtHeight;
	int						m_uiFrmPitch;

	xdc_Char				m_bSEIFlagOut;
	XDAS_UInt8 				m_sliceFormat;

	IVIDEO1_BufDescIn		m_inBuf;
	XDM_Buf					m_outBuf;
	std::vector<CmemBuf::ptr> m_cmemBufs;

	void copyInputBuf(XDAS_Int8* in, int width, int height, int stride);

	void cache_init();
	void ires_init();
	void mem_init();
	void load_params(const std::string& fname);
	void dynamicparams_init();
	void enc_create();
	void dim_init();
	void check_warnings();
};

#endif // ENC_H
