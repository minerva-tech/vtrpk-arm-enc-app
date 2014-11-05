/*
 * enc.cpp
 *
 *  Created on: Jan 8, 2013
 *      Author: a
 */

#include <xdc/std.h>
#include <ti/xdais/xdas.h>
#include <ti/xdais/ires.h>

extern "C" {
#include "h264venc.h"
#include "h264venc_tii.h"
}
#include <ti/xdais/idma.h>

#include <ti/sdo/fc/rman/rman.h>
#include <ti/sdo/fc/ires/iresman.h>
#include <ti/sdo/fc/ires/edma3chan/ires_edma3Chan.h>
#include <ti/sdo/fc/ires/edma3chan/iresman_edma3Chan.h>
#include <ti/sdo/fc/ires/vicp/ires_vicp2.h>
#include <ti/sdo/fc/ires/vicp/iresman_vicp2.h>
#include <ti/sdo/fc/ires/hdvicp/iresman_hdvicp.h>
#include <ti/sdo/fc/ires/hdvicp/ires_hdvicp.h>

#include <ti/xdais/idma.h>
#include <ti/sdo/fc/utils/api/_alg.h>
#include <ti/sdo/utils/trace/gt.h>
#include <ti/sdo/linuxutils/cmem/include/cmem.h>

#include <ti/sdo/fc/ires/memtcm/ires_memtcm.h>
#include <ti/sdo/fc/ires/memtcm/iresman_memtcm.h>

#ifdef DEVICE_ID_CHECK
/* Link modules used for DEVICE ID check */
#include <ti/sdo/fc/ires/addrspace/ires_addrspace.h>
#include <ti/sdo/fc/ires/addrspace/iresman_addrspace.h>
#endif

#include "testapp_arm926intc.h"

#include "read_params.h"

#include "exception.h"

#include "log_to_file.h"

#include "enc.h"
#include "error_msg.h"

#include "defines.h"

#define _ENABLE_IRES_EDMA3


extern "C" void LockMP_init();
extern "C" void SemMP_init();
extern "C" void Memory_init();
extern "C" void GT_init();
extern "C" void Global_init();
extern "C" void Sem_init();

extern "C" XDAS_Int32 getEncodedSliceProvideSpace(IH264VENC_TI_DataSyncHandle dataSyncHandle,
                                       IH264VENC_TI_DataSyncDesc *dataSyncDesc);


extern "C" IRES_Fxns H264VENC_TI_IRES;
/*
extern "C" Int H264VENC_control(H264VENC_Handle handle, H264VENC_Cmd cmd, H264VENC_DynamicParams *params, H264VENC_Status *status);
extern "C" H264VENC_Handle H264VENC_create(const IH264VENC_Fxns *fxns, const H264VENC_Params *prms);
extern "C" Void H264VENC_delete(H264VENC_Handle handle);
extern "C" Int H264VENC_encode(H264VENC_Handle handle, IVIDEO1_BufDescIn *Input, XDM_BufDesc *Output, H264VENC_InArgs *inargs, H264VENC_OutArgs *outarg);
  */

CMEM_AllocParams memParams;

void ARM926_Icache_Disable()
{
	return;
}
void ARM926_Dcache_Disable()
{
	return;
}

Enc::Enc(const std::string& config)
{
//	init();

	cache_init();

	ires_init();

	enc_create(config);
}

Enc::~Enc()
{
	H264VENC_Status status;
	H264VENC_control(m_handle, XDM_RESET, &m_dynamicparams/*NULL*/, &status/*NULL*/);

#ifdef _ENABLE_IRES_EDMA3
    if (IRES_OK != RMAN_freeResources((IALG_Handle)(m_handle), &H264VENC_TI_IRES, 1))
        log() << "CODEC_DEBUG_ENABLE: Free Resource Failed";
#endif
    if(m_handle)
    {
		H264VENC_delete(m_handle);
		m_handle = NULL;
	}

#ifdef _ENABLE_IRES_EDMA3
    if (IRES_OK != RMAN_unregister(&IRESMAN_EDMA3CHAN))
		log() << "Unregister Protocol Failed";
/*	if (IRES_OK != RMAN_unregister(&IRESMAN_VICP2))
		log() << "Unregister Protocol Failed";
	if (IRES_OK != RMAN_unregister(&IRESMAN_HDVICP))
		log() << "Unregister Protocol Failed";
	if (IRES_OK != RMAN_unregister(&IRESMAN_ADDRSPACE))
		log() << "Unregister Protocol Failed";
	if (IRES_OK != RMAN_unregister(&IRESMAN_MEMTCM))
        log() << "Unregister Protocol Failed";*/

//    RMAN_exit();

//	log() << "RMAN exited";
#endif
}

XDAS_Int8* Enc::encFrame(XDAS_Int8* in, int width, int height, int stride, size_t* out_size)
{
	copyInputBuf(in, width, height, stride);

	if (H264VENC_encode (m_handle, &m_inBuf, &m_outBuf.buf, &m_inargs, &m_outargs) == XDM_EFAIL) {
		H264VENC_Status status;
	    H264VENC_control(m_handle, XDM_GETSTATUS, &m_dynamicparams, &status);
		throw ex("Frame encoding failure" + printErrorMsg(status.videncStatus.extendedError));
	}

    if(m_outargs.videncOutArgs.inputFrameSkip == IVIDEO_FRAME_SKIPPED)
        log() << "CODEC_DEBUG_ENABLE: Frame skipped";

    *out_size = m_outargs.videncOutArgs.bytesGenerated;

	return m_outargs.videncOutArgs.encodedBuf.buf;
}

static int range(int v, int l, int r) {
	return v<l?l:v>r?r:v;
}

int Enc::changeBitrate(int delta_bitrate, int max_pos_step, int max_neg_step)
{	
	if (abs(delta_bitrate) > BITRATE_BIAS) {
		delta_bitrate = range(delta_bitrate, -abs(max_neg_step), abs(max_pos_step));
		int new_bitrate = m_dynamicparams.videncDynamicParams.targetBitRate + delta_bitrate;

		new_bitrate = range(new_bitrate, BITRATE_MIN, BITRATE_MAX);

		m_dynamicparams.videncDynamicParams.targetBitRate = new_bitrate;

		H264VENC_Status status;
		status.videncStatus.size = sizeof(IH264VENC_Status);

		if(H264VENC_control(m_handle, XDM_SETPARAMS, &m_dynamicparams, &status) == XDM_EFAIL) { // TODO: Is it possible to change only one dynamic parameter? I believe yes, but have no doc here to check.
			log() << "Error code : 0x" << std::hex << (int)status.videncStatus.extendedError << std::dec;
			throw ex("Set Encoder parameters Command Failed " + printErrorMsg(status.videncStatus.extendedError));
		}
		
		log() << "Due to data channel state, AVC target bitrate was changed to " << new_bitrate << " bps";
		
		return new_bitrate;
	}
	
	return m_dynamicparams.videncDynamicParams.targetBitRate;
}

void Enc::copyInputBuf(XDAS_Int8* in, int width, int height, int stride)
{
	width 	= std::min(width,  m_uiExtWidth);
	height = std::min(height, m_uiExtHeight);

	XDAS_Int8* Y  = m_inBuf.bufDesc[0].buf;
	XDAS_Int8* UV = m_inBuf.bufDesc[1].buf;

	for (int y=0; y<height; y++) {
		memcpy(Y, in, width);
		in += stride;
		Y  += m_uiFrmPitch;
	}

	for (int y=0; y<height/2; y++) {
		memcpy(UV, in, width);
		in += stride;
		UV += m_uiFrmPitch;
	}
}

void Enc::rman_init()
{
    LockMP_init();
    SemMP_init();
    Memory_init();
    GT_init();
    Global_init();
    Sem_init();
	
	log() << "RMAN_init()";

	const IRES_Status iresStatus = (IRES_Status) RMAN_init();
	if (IRES_OK != iresStatus)
		throw ex("RMAN initialization Failed");

	log() << "RMAN initialization ok";
}

void Enc::cache_init()
{
#ifndef SINGLE_PROC_CODE
#ifdef ENABLE_CACHE
    ARM926_Set_MMU_Base();
    ARM926_Set_Domains();
    ARM926_Enable_MMU();
    ARM926_Icache_Enable();
    ARM926_Flush_Icache();
    ARM926_Cache_Set_RoundRobin();
    ARM926_Dcache_Enable();
    ARM926_CleanFlush_Dcache();
#else
    ARM926_Icache_Disable();
    ARM926_Dcache_Disable();
#endif //ENABLE_CACHE
#endif //SINGLE_PROC_CODE
}

void Enc::mem_init()
{
    CMEM_init();

//    CMEM_AllocParams memParams;

    memParams.type=CMEM_POOL;
    memParams.alignment=256;

    H264VENC_Status status;
    status.videncStatus.size = sizeof(IH264VENC_Status);

    if (H264VENC_control(m_handle, XDM_GETBUFINFO, &m_dynamicparams, &status) == XDM_EFAIL)
		throw ex("CODEC_DEBUG_ENABLE: Get Buffer Info Command Failed");

    /*--------------------------------------------------------------------*/
    /*            GET H.264 ENCODER INPUT/OUTPUT BUFFER INFORMATION       */
    /*--------------------------------------------------------------------*/
    /* Based on the Num of buffers requested by the algorithm, the
     * application will allocate for the same here
     */
    memParams.flags=CMEM_NONCACHED;

    m_inBuf.numBufs = status.videncStatus.bufInfo.minNumInBufs;

    for(int i = 0; i < status.videncStatus.bufInfo.minNumInBufs; i++)
    {
        /* Assign size for the current buffer */
        m_inBuf.bufDesc[i].bufSize = status.videncStatus.bufInfo.minInBufSize[i];

        /* Allocate memory for buffer pointer */
        m_cmemBufs.push_back(CmemBuf::ptr(new CmemBuf(m_inBuf.bufDesc[i].bufSize * sizeof(xdc_Char), memParams)));
        m_inBuf.bufDesc[i].buf = static_cast<XDAS_Int8*>(m_cmemBufs.back()->user_addr);

        log() << "CODEC_DEBUG_ENABLE: Size of I/P buffer: " << m_inBuf.bufDesc[i].bufSize;

        if(!m_inBuf.bufDesc[i].buf)
        	throw ex("Memory for input buffer is not allocated. Not enough memory?");
    }

    /*------------------------------------------------------------------------*/
    /*                          ALLOCATE OUTPUT BUFFERS                       */
    /*------------------------------------------------------------------------*/
    m_outBuf->numBufs = status.videncStatus.bufInfo.minNumOutBufs;

    m_outBuf->bufSizes = new XDAS_Int32[m_outBuf->numBufs];

    if(!m_outBuf->bufSizes)
    	throw ex("Memory not allocated for outBuf->bufSizes");

    m_outBuf->bufs = new XDAS_Int8*[m_outBuf->numBufs];

    if(!m_outBuf->bufs)
    	throw ex("Memory not allocated for outBuf->bufs");

    for(int i = 0; i < m_outBuf->numBufs; i++)
    {
        /* Assign size for the current buffer */
        m_outBuf->bufSizes[i] = status.videncStatus.bufInfo.minOutBufSize[i];

        /* Allocate memory for buffers */
        m_cmemBufs.push_back(CmemBuf::ptr(new CmemBuf(m_outBuf->bufSizes[i] * sizeof(xdc_Char), memParams)));
        m_outBuf->bufs[i] = static_cast<XDAS_Int8*>(m_cmemBufs.back()->user_addr);

        log() << "CODEC_DEBUG_ENABLE: Output stream buffer size:" << m_outBuf->bufSizes[i];

        if(!m_outBuf->bufs[i])
        	throw ex("Memory not allocated");
    }

    memParams.flags=CMEM_CACHED;

    m_inBuf.frameHeight = m_uiExtHeight;
    m_inBuf.frameWidth = m_uiExtWidth;
    m_inBuf.framePitch = m_uiFrmPitch;

    if(m_dynamicparams.videncDynamicParams.captureWidth == 0 || m_dynamicparams.videncDynamicParams.captureWidth < m_uiExtWidth)
    	m_inBuf.framePitch = m_uiExtWidth;

	if(m_params.enableVUIparams & 8)
	{
	 /* initialize the VUI buffer with User defined encoded VUI parameters */
/*     memcpy(inobj.bufDesc[inobj.numBufs - 1].buf,
             &gtCustomizedVUIbuffer,
             sizeof(tCustomizedVUIbuffer_Interface) );*/
	}
}

void Enc::enc_create(const std::string& config)
{
	m_fxns = H264VENC_TI_IH264VENC;

	load_params(config);

	dim_init();

    dynamicparams_init();

	log() << "Encoder creation";

	if(!(m_handle = H264VENC_create(&m_fxns, &m_params)))
		throw ex("CODEC_DEBUG_ENABLE: ERROR! - Encoder Creation Failed");

	log() << "Encoder was created : " << std::hex << m_handle << std::dec;

#ifdef _ENABLE_IRES_EDMA3
    if (IRES_OK != RMAN_assignResources((IALG_Handle)m_handle, &H264VENC_TI_IRES, 1))
		throw ex("Failed in assignign resources. Exiting for this configuration.");
#endif

	log() << "IRES activate resources";

//    RMAN_activateAllResources((IALG_Handle)m_handle, &m_iresFxns, 1);
    H264VENC_TI_IRES.activateAllResources((IALG_Handle)m_handle);

	log() << "Check warnings";

    check_warnings();

	log() << "Mem init";

	mem_init();

#ifdef BASE_PARAMS
	m_inargs.videncInArgs.size = sizeof(IVIDENC1_InArgs);
#else
	m_inargs.videncInArgs.size = sizeof(IH264VENC_InArgs);
#ifdef SEI_USERDATA_INSERTION
	/* Input to the codec, to provide the space for adding SEI userdata unregistered
	 * into the bitstream */
	m_inargs.insertUserData = (1 * m_bSEIFlagOut);
	m_inargs.lengthUserData = (60 * m_bSEIFlagOut);
#endif
#endif
	m_inargs.videncInArgs.inputID = 1;
	m_inargs.videncInArgs.topFieldFirstFlag = XDAS_TRUE;
	m_inargs.timeStamp = 0;

#ifdef LOW_LATENCY_FEATURE
	/* If outputDataMode is chosen as slice mode(0), than codec produces */
	/* encoded slice after numOutputDataUnits slices are encode          */
	m_inargs.numOutputDataUnits = 1;

	m_sliceFormat = m_params.sliceFormat;
//	gFpOutBitStream = pfOutBitStreamFile;
//	gOutObj = &outobj;
#endif //LOW_LATENCY_FEATURE

	/* Initialize Output Arguments */
#ifdef BASE_PARAMS
	m_outargs.videncOutArgs.size = sizeof(IVIDENC1_OutArgs);
#else
	m_outargs.videncOutArgs.size = sizeof(IH264VENC_OutArgs);
#endif
	m_outargs.videncOutArgs.extendedError = 0;
	m_outargs.videncOutArgs.bytesGenerated = 0;
	m_outargs.videncOutArgs.encodedFrameType = IVIDEO_NA_FRAME;
	m_outargs.videncOutArgs.inputFrameSkip = IVIDEO_FRAME_ENCODED;
	m_outargs.videncOutArgs.outputID = 1;
	m_outargs.numPackets = 0;

	H264VENC_Status status;
    status.videncStatus.size = sizeof(IH264VENC_Status);

	log() << "Set dynamic params";

    if(H264VENC_control(m_handle, XDM_SETPARAMS, &m_dynamicparams, &status) == XDM_EFAIL) {
        log() << "Error code : 0x" << std::hex << (int)status.videncStatus.extendedError << std::dec;
    	throw ex("Set Encoder parameters Command Failed " + printErrorMsg(status.videncStatus.extendedError));
    }

    if(status.videncStatus.extendedError) {
        log() << "Error code : 0x" << std::hex << (int)status.videncStatus.extendedError << std::dec;
    	log() << printErrorMsg(status.videncStatus.extendedError);
    }
}

void Enc::dim_init()
{
	/* Compute extended width and extended height */
	m_uiExtWidth = (m_dynamicparams.videncDynamicParams.inputWidth + 15) & ~0x0F;

	if(m_params.videncParams.inputContentType == 0)
		m_uiExtHeight = (m_dynamicparams.videncDynamicParams.inputHeight + 15) & ~0x0F; // Progressive case
	else
		m_uiExtHeight = (m_dynamicparams.videncDynamicParams.inputHeight + 31) & ~0x1F; // Interlaced case

	m_uiFrmPitch = m_uiExtWidth;
}

void Enc::check_warnings()
{
	XDAS_Int8  lib_version[400] = "LIB";            /* Library version number */

    H264VENC_Status status;
    status.videncStatus.size = sizeof(IH264VENC_Status);
    status.videncStatus.data.bufSize = sizeof(lib_version);
    status.videncStatus.data.buf = lib_version;

    log() << "Encoder instance : " << std::hex << m_handle << std::dec;

    XDAS_Int32 iErrorFlag = H264VENC_control(
        m_handle,         // Instance Handle
        XDM_GETSTATUS,    // Command
        &m_dynamicparams, // Pointer to Dynamic Params structure -Input
        &status           // Pointer to the status structure - Output
    );

    if(iErrorFlag == XDM_EFAIL)
    	throw ex("Get Status Info Command Failed " + printErrorMsg(status.videncStatus.extendedError));

    if(status.videncStatus.extendedError)
    	log() << printErrorMsg(status.videncStatus.extendedError);

    iErrorFlag = H264VENC_control(
        m_handle,         // Instance Handle
        XDM_GETVERSION,   // Command
        &m_dynamicparams, // Pointer to Dynamic Params structure -Input
        &status           // Pointer to the status structure - Output
    );

    if(iErrorFlag == XDM_EFAIL)
    	throw ex("Get Version Command Failed " + printErrorMsg(status.videncStatus.extendedError));

    log() << "CODEC_DEBUG_ENABLE: 2nd Library Version " << lib_version;
}

void Enc::ires_init()
{
	log() << "ires_init()";
#ifdef _ENABLE_IRES_EDMA3
//    const IRES_Status iresStatus = (IRES_Status) RMAN_init();
//    if (IRES_OK != iresStatus)
//    	throw ex("RMAN initialization Failed");

//    log() << "RMAN initialization ok";

    {
    IRESMAN_Edma3ChanParams configParams;
    /*
     * Supply initialization information for the EDMA3 RESMAN while registering
     */
    configParams.baseConfig.allocFxn = RMAN_PARAMS.allocFxn;
    configParams.baseConfig.freeFxn = RMAN_PARAMS.freeFxn;
    configParams.baseConfig.size = sizeof(IRESMAN_Edma3ChanParams);

    log() << "RMAN_PARAMS.allocFxn: 0x" << std::hex << (int)RMAN_PARAMS.allocFxn << " RMAN_PARAMS.freeFxn: 0x" << (int)RMAN_PARAMS.freeFxn << std::dec;

    /* Register the EDMA3CHAN protocol/resource manager with the
     * generic resource manager */

    const IRES_Status iresStatus = (IRES_Status) RMAN_register(&IRESMAN_EDMA3CHAN, (IRESMAN_Params *)&configParams);
    if (IRES_OK != iresStatus)
    	throw ex("EDMA3 Protocol Registration Failed");
    }

    log() << "EDMA3 registration ok";

    {
    IRESMAN_VicpParams configParams;
    /*
     * Supply initialization information for the RESMAN while registering
     */
    configParams.baseConfig.allocFxn = RMAN_PARAMS.allocFxn;
    configParams.baseConfig.freeFxn = RMAN_PARAMS.freeFxn;
    configParams.baseConfig.size = sizeof(IRESMAN_VicpParams);

    /* Register the VICP protocol/resource manager with the
     * generic resource manager */
    const IRES_Status iresStatus = (IRES_Status)RMAN_register(&IRESMAN_VICP2, (IRESMAN_Params *)&configParams);

    if (IRES_OK != iresStatus)
        throw ex("VICP Protocol Registration Failed");
    }

    log() << "VICP registration ok";

    {
    IRESMAN_HdVicpParams configParams;
    /*
     * Supply initialization information for the RESMAN while registering
     */
    configParams.baseConfig.allocFxn = RMAN_PARAMS.allocFxn;
    configParams.baseConfig.freeFxn = RMAN_PARAMS.freeFxn;
    configParams.baseConfig.size = sizeof(IRESMAN_HdVicpParams);
    configParams.numResources = 1;
    /* Register the VICP protocol/resource manager with the
     * generic resource manager */
    const IRES_Status iresStatus = (IRES_Status)RMAN_register(&IRESMAN_HDVICP,
                                            (IRESMAN_Params *)&configParams);

    if (IRES_OK != iresStatus)
        throw ex("VICP Protocol Registration Failed");
    }

    log() << "HDVICP registration ok";

#ifdef DEVICE_ID_CHECK
	{
	IRESMAN_AddrSpaceParams addrspaceConfigParams;
    /*
     * Supply initialization information for the ADDRSPACE RESMAN while registering
     *           */
    addrspaceConfigParams.baseConfig.allocFxn = RMAN_PARAMS.allocFxn;
    addrspaceConfigParams.baseConfig.freeFxn = RMAN_PARAMS.freeFxn;
    addrspaceConfigParams.baseConfig.size = sizeof(IRESMAN_AddrSpaceParams);

    const IRES_Status iresStatus = RMAN_register(&IRESMAN_ADDRSPACE, (IRESMAN_Params *)&addrspaceConfigParams);

    if (IRES_OK != iresStatus)
        throw ex("ADDRSPACE Protocol Registration Failed ");

    log() << "ADDRSPACE Protocol Registration Success";
	}
#endif
#endif

#ifdef ON_LINUX
    {
    IRESMAN_MemTcmParams memTcmConfigParams;
    /*
     * Supply initialization information for the ADDRSPACE RESMAN while registering
     *           */
    memTcmConfigParams.baseConfig.allocFxn = RMAN_PARAMS.allocFxn;
    memTcmConfigParams.baseConfig.freeFxn = RMAN_PARAMS.freeFxn;
    memTcmConfigParams.baseConfig.size = sizeof(IRESMAN_MemTcmParams);

    const IRES_Status iresStatus = RMAN_register(&IRESMAN_MEMTCM, (IRESMAN_Params *)&memTcmConfigParams);

    if (IRES_OK != iresStatus)
        throw ex("CODEC_DEBUG_ENABLE: MEMTCM Protocol Registration Failed");

    log() << "CODEC_DEBUG_ENABLE: MEMTCM Protocol Registration Success";
    }
#endif

#ifdef DM365_IPC_INTC_ENABLE
        /*--------------------------------------------------------------------*/
        /* Interrupt enable related function calls please refer to User       */
        /* guide for a detailed description of these functions and the        */
        /* DM365_IPC_INTC_ENABLE macro usage                                  */
        /* Call the functions to enable ARM926 FIQ and do some basic setup to */
        /* AINTC to accept KLD INTC (arm968) interupt in FIQ pin of Arm926    */
        /*--------------------------------------------------------------------*/
#ifndef ON_LINUX
        log() << "Setup of AINTC";

        ARM926_enable_FIQ();  /* SWI call to enable interrupts */
        ARM926_INTC_init();   /* Init ARM INTC */
#endif
#endif //DM365_IPC_INTC_ENABLE
}

void Enc::load_params(const std::string& config)
{
    /* Point the param pointer to default parameters from encoder */
    m_params = H264VENC_PARAMS;

    m_dynamicparams.VUI_Buffer = &VUIPARAMSBUFFER;
	m_dynamicparams.CustomScaleMatrix_Buffer = &CUSTOMSEQSCALEMATRICES;

	int uiTokenCtr = 0;

    sTokenMapping sTokenMap[CFG_MAX_ITEMS_TO_PARSE];

    /* Set up Token Map for all the input parameters to be read from the
     * configuration file
     */
    sTokenMap[uiTokenCtr].tokenName = "FramesToEncode";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_uiNumFramesToBeEncoded;

    sTokenMap[uiTokenCtr].tokenName = "ImageHeight";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.videncDynamicParams.inputHeight;

    sTokenMap[uiTokenCtr].tokenName = "ImageWidth";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.videncDynamicParams.inputWidth;

    sTokenMap[uiTokenCtr].tokenName = "FrameRate";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.videncDynamicParams.targetFrameRate;

    sTokenMap[uiTokenCtr].tokenName = "BitRate";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.videncDynamicParams.targetBitRate;

    sTokenMap[uiTokenCtr].tokenName = "ChromaFormat";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.videncParams.inputChromaFormat;

    sTokenMap[uiTokenCtr].tokenName = "InterlacedVideo";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.videncParams.inputContentType;

    sTokenMap[uiTokenCtr].tokenName = "IntraPeriod";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.videncDynamicParams.intraFrameInterval;

    sTokenMap[uiTokenCtr].tokenName = "IDRFramePeriod";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.idrFrameInterval;

    sTokenMap[uiTokenCtr].tokenName = "ME_Type";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.meAlgo;

    sTokenMap[uiTokenCtr].tokenName = "RC_PRESET";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.videncParams.rateControlPreset;

	sTokenMap[uiTokenCtr].tokenName = "ENC_PRESET";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.videncParams.encodingPreset;

    sTokenMap[uiTokenCtr].tokenName = "SliceSize";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.sliceSize;

    sTokenMap[uiTokenCtr].tokenName = "RateControl";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.rcAlgo;

    sTokenMap[uiTokenCtr].tokenName = "MaxDelay";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.maxDelay;

    sTokenMap[uiTokenCtr].tokenName = "AspectRatioWidth";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.aspectRatioX;

    sTokenMap[uiTokenCtr].tokenName = "AspectRatioHeight";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.aspectRatioY;

    sTokenMap[uiTokenCtr].tokenName = "EnableVUIParam";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.enableVUIparams;

    sTokenMap[uiTokenCtr].tokenName = "EnableBufSEI";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.enableBufSEI;

    sTokenMap[uiTokenCtr].tokenName = "QPInit";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.initQ;

    sTokenMap[uiTokenCtr].tokenName = "QPISlice";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.intraFrameQP;

    sTokenMap[uiTokenCtr].tokenName = "QPSlice";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.interPFrameQP;

    sTokenMap[uiTokenCtr].tokenName = "MaxQP";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.rcQMax;

    sTokenMap[uiTokenCtr].tokenName = "MinQP";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.rcQMin;

    sTokenMap[uiTokenCtr].tokenName = "MaxQPI";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.rcQMaxI;

    sTokenMap[uiTokenCtr].tokenName = "MinQPI";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.rcQMinI;

    sTokenMap[uiTokenCtr].tokenName = "ProfileIDC";
    sTokenMap[uiTokenCtr].bType     = 0;
    sTokenMap[uiTokenCtr++].place   = &m_params.profileIdc;

    sTokenMap[uiTokenCtr].tokenName = "LevelIDC";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.levelIdc;

    sTokenMap[uiTokenCtr].tokenName = "Log2MaxFrameNumMinus4";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.Log2MaxFrameNumMinus4;

    sTokenMap[uiTokenCtr].tokenName = "ConstraintFlag";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.ConstraintSetFlag;

    sTokenMap[uiTokenCtr].tokenName = "AirRate";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.airRate;

    sTokenMap[uiTokenCtr].tokenName = "LoopFilterDisable";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.lfDisableIdc;

    sTokenMap[uiTokenCtr].tokenName = "PerceptualRC";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.perceptualRC;

    sTokenMap[uiTokenCtr].tokenName = "EntropyCodingMode";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.entropyMode;

    sTokenMap[uiTokenCtr].tokenName = "Transform8x8FlagIntra";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.transform8x8FlagIntraFrame;

    sTokenMap[uiTokenCtr].tokenName = "Transform8x8FlagInter";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.transform8x8FlagInterFrame;

    sTokenMap[uiTokenCtr].tokenName = "SequenceScalingFlag";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.seqScalingFlag;

    sTokenMap[uiTokenCtr].tokenName = "EncoderQuality";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.encQuality;

    sTokenMap[uiTokenCtr].tokenName = "mvSADout";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.mvSADoutFlag;

    sTokenMap[uiTokenCtr].tokenName = "useARM926Tcm";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.enableARM926Tcm;

    sTokenMap[uiTokenCtr].tokenName = "enableROI";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.enableROI;

    sTokenMap[uiTokenCtr].tokenName = "mapIMCOPtoDDR";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.enableDDRbuff;

#ifdef ENABLE_ROI

    sTokenMap[uiTokenCtr].tokenName = "NumOfROI";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_inargs.roiParameters.numOfROI;

    sTokenMap[uiTokenCtr].tokenName = "ROI_1_Xmin";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_inargs.roiParameters.listROI[0].topLeft.x;

    sTokenMap[uiTokenCtr].tokenName = "ROI_1_Ymin";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_inargs.roiParameters.listROI[0].topLeft.y;

    sTokenMap[uiTokenCtr].tokenName = "ROI_1_Xmax";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_inargs.roiParameters.listROI[0].bottomRight.x;

    sTokenMap[uiTokenCtr].tokenName = "ROI_1_Ymax";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_inargs.roiParameters.listROI[0].bottomRight.y;

    sTokenMap[uiTokenCtr].tokenName = "ROI_1_prty";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_inargs.roiParameters.roiPriority[0];

    sTokenMap[uiTokenCtr].tokenName = "ROI_1_type";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_inargs.roiParameters.roiType[0];

    sTokenMap[uiTokenCtr].tokenName = "ROI_2_Xmin";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_inargs.roiParameters.listROI[1].topLeft.x;

    sTokenMap[uiTokenCtr].tokenName = "ROI_2_Ymin";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_inargs.roiParameters.listROI[1].topLeft.y;

    sTokenMap[uiTokenCtr].tokenName = "ROI_2_Xmax";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_inargs.roiParameters.listROI[1].bottomRight.x;

    sTokenMap[uiTokenCtr].tokenName = "ROI_2_Ymax";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_inargs.roiParameters.listROI[1].bottomRight.y;

    sTokenMap[uiTokenCtr].tokenName = "ROI_2_prty";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_inargs.roiParameters.roiPriority[1];

    sTokenMap[uiTokenCtr].tokenName = "ROI_2_type";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_inargs.roiParameters.roiType[1];

#endif
#ifdef SIMPLE_TWO_PASS
    sTokenMap[uiTokenCtr].tokenName = "metaDataGenerateConsume";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.metaDataGenerateConsume;
#endif
    sTokenMap[uiTokenCtr].tokenName = "sliceMode";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.sliceMode;

    sTokenMap[uiTokenCtr].tokenName = "numTemporalLayers";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.numTemporalLayers;

    sTokenMap[uiTokenCtr].tokenName = "svcSyntaxEnable";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.svcSyntaxEnable;

//#ifdef LOW_LATENCY_FEATURE
    sTokenMap[uiTokenCtr].tokenName = "outputDataMode";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.outputDataMode;

    sTokenMap[uiTokenCtr].tokenName = "sliceFormat";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.sliceFormat;
//#endif //LOW_LATENCY_FEATURE

    sTokenMap[uiTokenCtr].tokenName = "interlaceRefMode";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.interlaceRefMode;

    sTokenMap[uiTokenCtr].tokenName = "maxBitrateCVBR";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.maxBitrateCVBR;


    sTokenMap[uiTokenCtr].tokenName = "enableGDR";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.enableGDR;

    sTokenMap[uiTokenCtr].tokenName = "GDRinterval";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.GDRinterval;

    sTokenMap[uiTokenCtr].tokenName = "GDRduration";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.GDRduration;

    sTokenMap[uiTokenCtr].tokenName = "LongTermRefreshInterval";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_dynamicparams.LongTermRefreshInterval;

    sTokenMap[uiTokenCtr].tokenName = "EnableLongTermFrame";
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = &m_params.EnableLongTermFrame;


    sTokenMap[uiTokenCtr].tokenName = NULL;
    sTokenMap[uiTokenCtr].bType     = 0 ;
    sTokenMap[uiTokenCtr++].place   = NULL;

    /*--------------------------------------------------------------------*/
    /*                    READ INPUT CONFIG FILE                          */
    /*--------------------------------------------------------------------*/

//    throw ex("CODEC_DEBUG_ENABLE: ERROR! - Could Not open Config File"); // test

    log() << "CODEC_DEBUG_ENABLE: Reading Configuration file";

    if(!readparamfile(config, sTokenMap))
    	throw ex("CODEC_DEBUG_ENABLE: ERROR! - Unable to read Config File");

#ifdef BASE_PARAMS
	m_params.videncParams.size = sizeof(IVIDENC1_Params);
#else
	m_params.videncParams.size = sizeof(IH264VENC_Params);
#endif
	const int MAX_WIDTH = 2048;
	const int MAX_HEIGHT = 2048;

	m_params.videncParams.maxWidth = MAX_WIDTH;//uiExtWidth;
	m_params.videncParams.maxHeight = MAX_HEIGHT;//uiExtHeight;
	m_params.videncParams.maxBitRate = 20000000;
	m_params.videncParams.maxFrameRate = 120000;
	m_params.videncParams.reconChromaFormat = IH264VENC_YUV_420IUV;

	log() << "Config file was read";
}

void Enc::dynamicparams_init()
{
	m_bSEIFlagOut = 0;
#ifdef BASE_PARAMS
	m_dynamicparams.videncDynamicParams.size = sizeof(IVIDENC1_DynamicParams);
	m_bSEIFlagOut = m_bSEIFlagOut & 0;
#else
	m_dynamicparams.videncDynamicParams.size = sizeof(IH264VENC_DynamicParams);
	m_bSEIFlagOut = m_bSEIFlagOut & 1;
#endif
	m_dynamicparams.videncDynamicParams.refFrameRate =
			m_dynamicparams.videncDynamicParams.targetFrameRate;
	m_dynamicparams.videncDynamicParams.generateHeader = 0;
	m_dynamicparams.videncDynamicParams.captureWidth =
			(m_uiFrmPitch << m_params.videncParams.inputContentType);
	m_dynamicparams.videncDynamicParams.forceFrame = IVIDEO_NA_FRAME;
	m_dynamicparams.videncDynamicParams.interFrameInterval = 0;
	m_dynamicparams.videncDynamicParams.mbDataFlag = 0;
	m_dynamicparams.enablePicTimSEI = m_dynamicparams.enableBufSEI;
	m_dynamicparams.resetHDVICPeveryFrame = 0;
	m_dynamicparams.disableMVDCostFactor = 0;
	m_dynamicparams.UseLongTermFrame = 0;
	m_dynamicparams.SetLongTermFrame = 0;
	m_dynamicparams.LBRmaxpicsize = 0;
	m_dynamicparams.LBRminpicsize = 0;
	m_dynamicparams.LBRskipcontrol = 0;
	m_dynamicparams.CVBRsensitivity = 0;
	m_dynamicparams.maxHighCmpxIntCVBR = 0;
//    m_dynamicparams.maxBitrateCVBR = 1000000;

#ifdef LOW_LATENCY_FEATURE
	m_dynamicparams.putDataGetSpaceFxn = getEncodedSliceProvideSpace;
	m_dynamicparams.dataSyncHandle = NULL;
#endif //LOW_LATENCY_FEATURE
}

#ifdef LOW_LATENCY_FEATURE
/*===========================================================================*/
/**
*@func   getEncodedSliceProvideSpace
*
*@brief  Callback module for low latency inetrface. Codec calls this module to
*        provide encoded slice and get memory space for future encoded stream
*
*@param  IH264VENC_TI_DataSyncHandle
*        handle to DataSync provided by App to codec at the time of instance
*         creation
*
*        IH264VENC_TI_DataSyncDesc
*        Descriptor carrying information on pointer to number of encoded slice,
*        pointer to bitstream,  slice sizes
*@return XDAS_Int32
*        Status
*
*@note
*/
/*===========================================================================*/
extern "C" XDAS_Int32 getEncodedSliceProvideSpace(IH264VENC_TI_DataSyncHandle dataSyncHandle,
                                       IH264VENC_TI_DataSyncDesc *dataSyncDesc)
{
	// todo: implement

    XDAS_UInt8 i, numBlocks;
    FILE *pfOutBitStreamFile = gFpOutBitStream;
    XDAS_UInt32 startCodePattern = 0x01000000;
    numBlocks = dataSyncDesc->numBlocks;
    for (i = 0; i < numBlocks; i++)
    {
        if(gSliceFormat == IH264VENC_TI_NALSTREAM)
        {
            fwrite(&startCodePattern, 1, 4, pfOutBitStreamFile);
        }
        fwrite(dataSyncDesc->baseAddr[i], 1,
            dataSyncDesc->blockSizes[i], pfOutBitStreamFile);
#ifdef ENABLE_CACHE
        MEMUTILS_cacheWbInv(dataSyncDesc->baseAddr[i],
            dataSyncDesc->blockSizes[i]);
#endif
    }
    dataSyncDesc->numBlocks = 1;
    dataSyncDesc->baseAddr[0]   = (XDAS_Int32 *)gOutObj->bufs[0];
    dataSyncDesc->blockSizes[0] = gOutObj->bufSizes[0];
    return(XDM_EOK);
}
#endif //LOW_LATENCY_FEATURE
