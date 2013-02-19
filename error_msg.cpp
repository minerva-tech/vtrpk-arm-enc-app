/*
 * error_msg.cpp
 *
 *  Created on: Jan 22, 2013
 *      Author: a
 */

#include "error_msg.h"

/*===========================================================================*/
/*!
*@func   printErrorMsg
*
*@brief  This print the encoder error code string for the input error code
*
*@param  errorCode
*        input H264 encoder error code
*
*@return void
*
*@note
*/
/*===========================================================================*/
std::string printErrorMsg(XDAS_Int32 errorCode)
{
    if(0 == errorCode)
        return "";

    std::string str;

//    str = "CODEC_DEBUG_ENABLE:";
    if(XDM_ISFATALERROR(errorCode))
    {
        str += "\tFatal Error";
    }

    if(XDM_ISUNSUPPORTEDPARAM(errorCode))
    {
        str += "\tUnsupported Param Error";
    }

    if(XDM_ISUNSUPPORTEDINPUT(errorCode))
    {
        str += "\tUnsupported Input Error";
    }

    switch(errorCode)
    {
        case IH264VENC_ERR_MAXWIDTH :
            str += ("IH264VENC_ERR_MAXWIDTH\n");
            break;

        case IH264VENC_ERR_MAXHEIGHT :
            str += ("IH264VENC_ERR_MAXHEIGHT\n");
            break;

        case IH264VENC_ERR_ENCODINGPRESET :
            str += ("IH264VENC_ERR_ENCODINGPRESET\n");
            break;

        case IH264VENC_ERR_RATECONTROLPRESET :
            str += ("IH264VENC_ERR_RATECONTROLPRESET\n");
            break;

        case IH264VENC_ERR_MAXFRAMERATE :
            str += ("IH264VENC_ERR_MAXFRAMERATE\n");
            break;

        case IH264VENC_ERR_MAXBITRATE :
            str += ("IH264VENC_ERR_MAXBITRATE\n");
            break;

        case IH264VENC_ERR_DATAENDIANNESS :
            str += ("IH264VENC_ERR_DATAENDIANNESS\n");
            break;

        case IH264VENC_ERR_INPUTCHROMAFORMAT :
            str += ("IH264VENC_ERR_INPUTCHROMAFORMAT\n");
            break;

        case IH264VENC_ERR_INPUTCONTENTTYPE :
            str += ("IH264VENC_ERR_INPUTCONTENTTYPE\n");
            break;

        case IH264VENC_ERR_RECONCHROMAFORMAT :
            str += ("IH264VENC_ERR_RECONCHROMAFORMAT\n");
            break;

        case IH264VENC_ERR_INPUTWIDTH :
            str += ("IH264VENC_ERR_INPUTWIDTH\n");
            break;

        case IH264VENC_ERR_INPUTHEIGHT :
            str += ("IH264VENC_ERR_INPUTHEIGHT\n");
            break;

        case IH264VENC_ERR_MAX_MBS_IN_FRM_LIMIT_EXCEED :
            str += ("IH264VENC_ERR_MAX_MBS_IN_FRM_LIMIT_EXCEED\n");
            break;

        case IH264VENC_ERR_TARGETFRAMERATE :
            str += ("IH264VENC_ERR_TARGETFRAMERATE\n");
            break;

        case IH264VENC_ERR_TARGETBITRATE :
            str += ("IH264VENC_ERR_TARGETBITRATE\n");
            break;

        case IH264VENC_ERR_PROFILEIDC :
            str += ("IH264VENC_ERR_PROFILEIDC\n");
            break;

        case IH264VENC_ERR_LEVELIDC :
            str += ("IH264VENC_ERR_LEVELIDC\n");
            break;

        case IH264VENC_ERR_ENTROPYMODE_IN_BP :
            str += ("IH264VENC_ERR_ENTROPYMODE_IN_BP\n");
            break;

        case IH264VENC_ERR_TRANSFORM8X8FLAGINTRA_IN_BP_MP :
            str += ("IH264VENC_ERR_TRANSFORM8X8FLAGINTRA_IN_BP_MP\n");
            break;

        case IH264VENC_ERR_TRANSFORM8X8FLAGINTER_IN_BP_MP :
            str += ("IH264VENC_ERR_TRANSFORM8X8FLAGINTER_IN_BP_MP\n");
            break;

        case IH264VENC_ERR_SEQSCALINGFLAG_IN_BP_MP :
            str += ("IH264VENC_ERR_SEQSCALINGFLAG_IN_BP_MP\n");
            break;

        case IH264VENC_ERR_ASPECTRATIOX :
            str += ("IH264VENC_ERR_ASPECTRATIOX\n");
            break;

        case IH264VENC_ERR_ASPECTRATIOY :
            str += ("IH264VENC_ERR_ASPECTRATIOY\n");
            break;

        case IH264VENC_ERR_PIXELRANGE :
            str += ("IH264VENC_ERR_PIXELRANGE\n");
            break;

        case IH264VENC_ERR_TIMESCALE :
            str += ("IH264VENC_ERR_TIMESCALE\n");
            break;

        case IH264VENC_ERR_NUMUNITSINTICKS :
            str += ("IH264VENC_ERR_NUMUNITSINTICKS\n");
            break;

        case IH264VENC_ERR_ENABLEVUIPARAMS :
            str += ("IH264VENC_ERR_ENABLEVUIPARAMS\n");
            break;

        case IH264VENC_ERR_RESETHDVICPEVERYFRAME :
            str += ("IH264VENC_ERR_RESETHDVICPEVERYFRAME\n");
            break;

        case IH264VENC_ERR_MEALGO :
            str += ("IH264VENC_ERR_MEALGO\n");
            break;

        case IH264VENC_ERR_UNRESTRICTEDMV :
            str += ("IH264VENC_ERR_UNRESTRICTEDMV\n");
            break;

        case IH264VENC_ERR_ENCQUALITY :
            str += ("IH264VENC_ERR_ENCQUALITY\n");
            break;

        case IH264VENC_ERR_ENABLEARM926TCM :
            str += ("IH264VENC_ERR_ENABLEARM926TCM\n");
            break;

        case IH264VENC_ERR_ENABLEDDRBUFF :
            str += ("IH264VENC_ERR_ENABLEDDRBUFF\n");
            break;

        case IH264VENC_ERR_LEVEL_NOT_FOUND :
            str += ("IH264VENC_ERR_LEVEL_NOT_FOUND\n");
            break;

        case IH264VENC_ERR_REFFRAMERATE_MISMATCH :
            str += ("IH264VENC_ERR_REFFRAMERATE_MISMATCH\n");
            break;

        case IH264VENC_ERR_INTRAFRAMEINTERVAL :
            str += ("IH264VENC_ERR_INTRAFRAMEINTERVAL\n");
            break;

        case IH264VENC_ERR_GENERATEHEADER :
            str += ("IH264VENC_ERR_GENERATEHEADER\n");
            break;

        case IH264VENC_ERR_FORCEFRAME :
            str += ("IH264VENC_ERR_FORCEFRAME\n");
            break;

        case IH264VENC_ERR_RCALGO :
            str += ("IH264VENC_ERR_RCALGO\n");
            break;

        case IH264VENC_ERR_INTRAFRAMEQP :
            str += ("IH264VENC_ERR_INTRAFRAMEQP\n");
            break;

        case IH264VENC_ERR_INTERPFRAMEQP :
            str += ("IH264VENC_ERR_INTERPFRAMEQP\n");
            break;

        case IH264VENC_ERR_RCQMAX :
            str += ("IH264VENC_ERR_RCQMAX\n");
            break;

        case IH264VENC_ERR_RCQMIN :
            str += ("IH264VENC_ERR_RCQMIN\n");
            break;

        case IH264VENC_ERR_RCIQMAX :
            str += ("IH264VENC_ERR_RCIQMAX\n");
            break;

        case IH264VENC_ERR_RCIQMIN :
            str += ("IH264VENC_ERR_RCIQMIN\n");
            break;

        case IH264VENC_ERR_INITQ :
            str += ("IH264VENC_ERR_INITQ\n");
            break;

        case IH264VENC_ERR_MAXDELAY :
            str += ("IH264VENC_ERR_MAXDELAY\n");
            break;

        case IH264VENC_ERR_LFDISABLEIDC :
            str += ("IH264VENC_ERR_LFDISABLEIDC\n");
            break;

        case IH264VENC_ERR_ENABLEBUFSEI :
            str += ("IH264VENC_ERR_ENABLEBUFSEI\n");
            break;

        case IH264VENC_ERR_ENABLEPICTIMSEI :
            str += ("IH264VENC_ERR_ENABLEPICTIMSEI\n");
            break;

        case IH264VENC_ERR_SLICESIZE :
            str += ("IH264VENC_ERR_SLICESIZE\n");
            break;

        case IH264VENC_ERR_INTRASLICENUM :
            str += ("IH264VENC_ERR_INTRASLICENUM\n");
            break;

        case IH264VENC_ERR_AIRRATE :
            str += ("IH264VENC_ERR_AIRRATE\n");
            break;

        case IH264VENC_ERR_MEMULTIPART :
            str += ("IH264VENC_ERR_MEMULTIPART\n");
            break;

        case IH264VENC_ERR_INTRATHRQF :
            str += ("IH264VENC_ERR_INTRATHRQF\n");
            break;

        case IH264VENC_ERR_PERCEPTUALRC :
            str += ("IH264VENC_ERR_PERCEPTUALRC\n");
            break;

        case IH264VENC_ERR_IDRFRAMEINTERVAL :
            str += ("IH264VENC_ERR_IDRFRAMEINTERVAL\n");
            break;

        case IH264VENC_ERR_ENABLEROI :
            str += ("IH264VENC_ERR_ENABLEROI\n");
            break;

        case IH264VENC_ERR_MAXBITRATE_CVBR :
            str += ("IH264VENC_ERR_MAXBITRATE_CVBR\n");
            break;

        case IH264VENC_ERR_CVBR_SENSITIVITY :
            str += ("IH264VENC_ERR_CVBR_SENSITIVITY\n");
            break;

        case IH264VENC_ERR_CVBR_MAX_CMPX_INT :
            str += ("IH264VENC_ERR_CVBR_MAX_CMPX_INT\n");
            break;

        case IH264VENC_ERR_MVSADOUTFLAG :
            str += ("IH264VENC_ERR_MVSADOUTFLAG\n");
            break;

        case IH264VENC_ERR_MAXINTERFRAMEINTERVAL :
            str += ("IH264VENC_ERR_MAXINTERFRAMEINTERVAL\n");
            break;

        case IH264VENC_ERR_CAPTUREWIDTH :
            str += ("IH264VENC_ERR_CAPTUREWIDTH\n");
            break;

        case IH264VENC_ERR_INTERFRAMEINTERVAL :
            str += ("IH264VENC_ERR_INTERFRAMEINTERVAL\n");
            break;

        case IH264VENC_ERR_MBDATAFLAG :
            str += ("IH264VENC_ERR_MBDATAFLAG\n");
            break;

        case IH264VENC_ERR_IVIDENC1_DYNAMICPARAMS_SIZE_IN_CORRECT :
            str += ("IH264VENC_ERR_IVIDENC1_DYNAMICPARAMS_SIZE_IN_CORRECT\n");
            break;

        case IH264VENC_ERR_IVIDENC1_PROCESS_ARGS_NULL :
            str += ("IH264VENC_ERR_IVIDENC1_PROCESS_ARGS_NULL\n");
            break;

        case IH264VENC_ERR_IVIDENC1_INARGS_SIZE :
            str += ("IH264VENC_ERR_IVIDENC1_INARGS_SIZE\n");
            break;

        case IH264VENC_ERR_IVIDENC1_OUTARGS_SIZE :
            str += ("IH264VENC_ERR_IVIDENC1_OUTARGS_SIZE\n");
            break;

        case IH264VENC_ERR_IVIDENC1_INARGS_INPUTID :
            str += ("IH264VENC_ERR_IVIDENC1_INARGS_INPUTID\n");
            break;

        case IH264VENC_ERR_IVIDENC1_INARGS_TOPFIELDFIRSTFLAG :
            str += ("IH264VENC_ERR_IVIDENC1_INARGS_TOPFIELDFIRSTFLAG\n");
            break;

        case IH264VENC_ERR_IVIDENC1_INBUFS :
            str += ("IH264VENC_ERR_IVIDENC1_INBUFS\n");
            break;

        case IH264VENC_ERR_IVIDENC1_INBUFS_BUFDESC :
            str += ("IH264VENC_ERR_IVIDENC1_INBUFS_BUFDESC\n");
            break;

        case IH264VENC_ERR_IVIDENC1_OUTBUFS :
            str += ("IH264VENC_ERR_IVIDENC1_OUTBUFS\n");
            break;

        case IH264VENC_ERR_IVIDENC1_OUTBUFS_NULL :
            str += ("IH264VENC_ERR_IVIDENC1_OUTBUFS_NULL\n");
            break;

        case IH264VENC_ERR_INTERLACE_IN_BP :
            str += ("IH264VENC_ERR_INTERLACE_IN_BP\n");
            break;

        case IH264VENC_ERR_RESERVED :
            str += ("IH264VENC_ERR_RESERVED\n");
            break;

        case IH264VENC_ERR_INSERTUSERDATA :
            str += ("IH264VENC_ERR_INSERTUSERDATA\n");
            break;

        case IH264VENC_ERR_ROIPARAM :
            str += ("IH264VENC_ERR_ROIPARAM\n");
            break;

        case IH264VENC_ERR_LENGTHUSERDATA :
            str += ("IH264VENC_ERR_LENGTHUSERDATA\n");
            break;

        case IH264VENC_ERR_PROCESS_CALL :
            str += ("IH264VENC_ERR_PROCESS_CALL\n");
            break;

        case IH264VENC_ERR_HANDLE_NULL :
            str += ("IH264VENC_ERR_HANDLE_NULL\n");
            break;

        case IH264VENC_ERR_INCORRECT_HANDLE :
            str += ("IH264VENC_ERR_INCORRECT_HANDLE\n");
            break;

        case IH264VENC_ERR_MEMTAB_NULL :
            str += ("IH264VENC_ERR_MEMTAB_NULL\n");
            break;

        case IH264VENC_ERR_IVIDENC1_INITPARAMS_SIZE :
            str += ("IH264VENC_ERR_IVIDENC1_INITPARAMS_SIZE\n");
            break;

        case IH264VENC_ERR_MEMTABS_BASE_NULL :
            str += ("IH264VENC_ERR_MEMTABS_BASE_NULL\n");
            break;

        case IH264VENC_ERR_MEMTABS_BASE_NOT_ALIGNED :
            str += ("IH264VENC_ERR_MEMTABS_BASE_NOT_ALIGNED\n");
            break;

        case IH264VENC_ERR_MEMTABS_SIZE :
            str += ("IH264VENC_ERR_MEMTABS_SIZE\n");
            break;

        case IH264VENC_ERR_MEMTABS_ATTRS :
            str += ("IH264VENC_ERR_MEMTABS_ATTRS\n");
            break;

        case IH264VENC_ERR_MEMTABS_SPACE :
            str += ("IH264VENC_ERR_MEMTABS_SPACE\n");
            break;

        case IH264VENC_ERR_MEMTABS_OVERLAP :
            str += ("IH264VENC_ERR_MEMTABS_OVERLAP\n");
            break;

        case IH264VENC_ERR_CODEC_INACTIVE :
            str += ("IH264VENC_ERR_CODEC_INACTIVE\n");
            break;

        case IH264VENC_WARN_LEVELIDC :
            str += ("IH264VENC_WARN_LEVELIDC\n");
            break;

        case IH264VENC_WARN_RATECONTROLPRESET :
            str += ("IH264VENC_WARN_RATECONTROLPRESET\n");
            break;

        case IH264VENC_WARN_H241_SLICE_SIZE_EXCEEDED :
            str += ("IH264VENC_WARN_H241_SLICE_SIZE_EXCEEDED\n");
            break;

        case IH264VENC_ERR_STATUS_BUF :
            str += ("IH264VENC_ERR_STATUS_BUF\n");
            break;

        case IH264VENC_ERR_METADATAFLAG :
            str += ("IH264VENC_ERR_METADATAFLAG\n");
            break;

        default:
            str += ("Unknown Error code\n");
    }

    return str;
}
