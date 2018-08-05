
#include "utils/log.h"
#include "HiDecoder.h"
#include "DVDClock.h"


#define __MODULE_NAME__ "13HisiAvDecoder"

extern void _Z42_107f1b933a62cea021c088004790e758008a3e49Av();
extern bool _Z42_d2050ca4b20719cc67b22956b468894d54294ff8Cv();


/*static*/ HisiAvDecoder* HisiAvDecoder::GetInstance()
{
  static HisiAvDecoder rThis;
  return &rThis;
}

  
HisiAvDecoder::HisiAvDecoder()
{
  m_opened = false;
  m_video_opend = false;
  m_audio_opend = false;
  m_dump = false;
  m_hAvplay = 0;
  m_hWin = 0;
  m_disp = HI_UNF_DISPLAY1;
  m_audio_full = false;
  m_video_full = false;
}


HisiAvDecoder::~HisiAvDecoder()
{
}

HI_S32 AVEventReport(HI_HANDLE hAvplay, HI_UNF_AVPLAY_EVENT_E enEvent, HI_VOID *pPara)
{
  //TODO
}

bool HisiAvDecoder::Init()
{
  CSingleLock lock(m_section);

  HI_UNF_AVPLAY_ATTR_S stAvAttr;
  HI_UNF_SYNC_ATTR_S stSyncAttr;

  if (m_opened)
  {
    return true;
  }

  HI_S32 res = HI_SYS_Init();
  if (res != HI_SUCCESS)
  {
    goto label_1c4;
  }

  res = HI_UNF_OTP_Init();
  if (res != HI_SUCCESS)
  {
    goto label_1c4;
  }

  _Z42_107f1b933a62cea021c088004790e758008a3e49Av();
  if (!_Z42_d2050ca4b20719cc67b22956b468894d54294ff8Cv())
  {
    goto label_1c4;
  }
  if (!_Z42_d2050ca4b20719cc67b22956b468894d54294ff8Cv())
  {
    goto label_1c4;
  }
  if (!_Z42_d2050ca4b20719cc67b22956b468894d54294ff8Cv())
  {
    goto label_1c4;
  }
  if (!_Z42_d2050ca4b20719cc67b22956b468894d54294ff8Cv())
  {
    goto label_1c4;
  }

  res = HIADP_VO_Init(HI_UNF_VO_DEV_MODE_NORMAL);
  if (res != HI_SUCCESS)
  {
    goto label_280;
  }

  res = HI_UNF_AVPLAY_Init();
  if (res != HI_SUCCESS)
  {
    goto label_27c;
  }

  res = HIADP_Snd_Init();
  if (res != HI_SUCCESS)
  {
    goto label_278;
  }

  res = HIADP_AVPlay_RegADecLib();
  if (res != HI_SUCCESS)
  {
    goto label_278;
  }

  res = HI_UNF_AVPLAY_GetDefaultConfig(&stAvAttr, HI_UNF_AVPLAY_STREAM_TYPE_ES);
  if (res != HI_SUCCESS)
  {
    goto label_27c;
  }

  stAvAttr.u32DemuxId = 0;

  res = HI_UNF_AVPLAY_Create(&stAvAttr, &m_hAvplay);
  if (res != HI_SUCCESS)
  {
    goto label_2fc;
  } 

  for (HI_UNF_AVPLAY_EVENT_E enEvent = HI_UNF_AVPLAY_EVENT_EOS; enEvent < HI_UNF_AVPLAY_EVENT_BUTT; /*++enEvent*/)
  {
    res = HI_UNF_AVPLAY_RegisterEvent64(m_hAvplay, enEvent, AVEventReport);
    if (res != HI_SUCCESS)
    {
      goto label_2fc;
    } 
    enEvent = (HI_UNF_AVPLAY_EVENT_E)((int)enEvent + 1);
  }

  res = HI_UNF_AVPLAY_GetAttr(m_hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &stSyncAttr);
  if (res != HI_SUCCESS)
  {
    goto label_2fc;
  } 

  stSyncAttr.enSyncRef = HI_UNF_SYNC_REF_AUDIO;
  stSyncAttr.stSyncStartRegion.s32VidPlusTime = 60;
  stSyncAttr.stSyncStartRegion.s32VidNegativeTime = -4;
  stSyncAttr.u32PreSyncTimeoutMs = 1000;
  stSyncAttr.bQuickOutput = HI_TRUE;

  res = HI_UNF_AVPLAY_SetAttr(m_hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &stSyncAttr);
  if (res != HI_SUCCESS)
  {
    return false;
  } 

  res = HIADP_VO_CreatWin(0, &m_hWin);
  if (res != HI_SUCCESS)
  {
    goto label_3b4;
  } 

  CLog::Log(/*LOGDEBUG*/LOGINFO, "HISIAV[%s:%d]HisiAvDecoder::Init success", __MODULE_NAME__, 231);

  m_opened = true;
  m_video_opend = false;
  m_audio_opend = false;
  return true;

label_3b4:
  HI_UNF_VO_DestroyWindow(m_hWin);
  m_hWin = 0;

label_2fc:
  HI_UNF_AVPLAY_Destroy(m_hAvplay);
  m_hAvplay = 0;

label_278:
  HIADP_Snd_DeInit();

label_27c:
  HI_UNF_AVPLAY_DeInit();

label_280:
  HIADP_VO_DeInit();

label_1c4:
  HI_UNF_OTP_DeInit();
  HI_SYS_DeInit();
  m_opened = false;
  return false;
}


bool HisiAvDecoder::Deinit()
{
  CSingleLock lock(m_section);

  if (m_video_opend || m_audio_opend)
  {
    return false;
  }

  for (HI_UNF_AVPLAY_EVENT_E enEvent = HI_UNF_AVPLAY_EVENT_EOS; enEvent < HI_UNF_AVPLAY_EVENT_BUTT; /*++enEvent*/)
  {
    HI_UNF_AVPLAY_UnRegisterEvent(m_hAvplay, enEvent);
    enEvent = (HI_UNF_AVPLAY_EVENT_E)((int)enEvent + 1);
  }

  if (m_hWin)
  {
    HI_UNF_VO_DestroyWindow(m_hWin);
    m_hWin = 0;
  }

  if (m_hAvplay)
  {
    HI_UNF_AVPLAY_Destroy(m_hAvplay);
    m_hAvplay = 0;
  }

  HI_UNF_AVPLAY_DeInit();
  HIADP_Snd_DeInit();
  HIADP_VO_DeInit();
  HI_UNF_OTP_DeInit();
  HI_SYS_DeInit();
  m_opened = false;
  return true;
}


bool HisiAvDecoder::VideoOpen(CDVDStreamInfo &hints)
{
  HI_S32 res;
  HI_UNF_AVPLAY_OPEN_OPT_S stAvOpen;

  Init();

  if (m_video_opend)
  {
    VideoClose();
  }

  CSingleLock lock(m_section);

  HI_UNF_VCODEC_TYPE_E r6;
  switch (hints.codec)
  {
    case AV_CODEC_ID_MPEG1VIDEO: //1:
    case AV_CODEC_ID_MPEG2VIDEO: //2:
      r6 = HI_UNF_VCODEC_TYPE_MPEG2; //0;
      break;
    case AV_CODEC_ID_H261: //3
      r6 = HI_UNF_VCODEC_TYPE_H261; //26;
      break;
    case AV_CODEC_ID_H263: //4
      r6 = HI_UNF_VCODEC_TYPE_H263; //3;
      break;
    case AV_CODEC_ID_RV10: //5
      r6 = HI_UNF_VCODEC_TYPE_RV10; //22;
      break;
    case AV_CODEC_ID_RV20: //6
      r6 = HI_UNF_VCODEC_TYPE_RV20; //23;
      break;
    case AV_CODEC_ID_MJPEG: //7
      r6 = HI_UNF_VCODEC_TYPE_MJPEG; //11;
      break;
    case AV_CODEC_ID_MJPEGB: //8
      r6 = HI_UNF_VCODEC_TYPE_MJPEGB; //34;
      break;
    case AV_CODEC_ID_LJPEG: //9
      r6 = HI_UNF_VCODEC_TYPE_JPEG; //15;
      break;
    case AV_CODEC_ID_MPEG4: //12
      r6 = HI_UNF_VCODEC_TYPE_MPEG4; //1;
      break;
    case AV_CODEC_ID_RAWVIDEO: //13
      r6 = HI_UNF_VCODEC_TYPE_RAW; //14;
      break;
    case AV_CODEC_ID_MSMPEG4V1: //14
      r6 = HI_UNF_VCODEC_TYPE_MSMPEG4V1; //17;
      break;
    case AV_CODEC_ID_MSMPEG4V2: //15
      r6 = HI_UNF_VCODEC_TYPE_MSMPEG4V2; //18;
      break;
    case AV_CODEC_ID_MSMPEG4V3: //16
      r6 = HI_UNF_VCODEC_TYPE_DIVX3; //13;
      break;
    case AV_CODEC_ID_WMV1: //17
      r6 = HI_UNF_VCODEC_TYPE_WMV1; //20;
      break;
    case AV_CODEC_ID_WMV2: //18
      r6 = HI_UNF_VCODEC_TYPE_WMV2; //21
      break;
    case AV_CODEC_ID_FLV1: //21
      r6 = HI_UNF_VCODEC_TYPE_SORENSON; //12
      break;
    case AV_CODEC_ID_SVQ1: //22
      r6 = HI_UNF_VCODEC_TYPE_SVQ1; //24
      break;
    case AV_CODEC_ID_SVQ3: //23
      r6 = HI_UNF_VCODEC_TYPE_SVQ3; //25
      break;
    case AV_CODEC_ID_DVVIDEO: //24
      r6 = HI_UNF_VCODEC_TYPE_DV; //37
      break;
    case AV_CODEC_ID_H264: //27
      r6 = HI_UNF_VCODEC_TYPE_H264; //4
      break;
    case AV_CODEC_ID_INDEO3: //28
      r6 = HI_UNF_VCODEC_TYPE_INDEO3; //31
      break;
    case AV_CODEC_ID_VP3: //29
      r6 = HI_UNF_VCODEC_TYPE_VP3; //27
      break;
    case AV_CODEC_ID_CINEPAK: //43
      r6 = HI_UNF_VCODEC_TYPE_CINEPAK; //29
      break;
    case AV_CODEC_ID_MSVIDEO1: //46
      r6 = HI_UNF_VCODEC_TYPE_MSVIDEO1; //19
      break;
    case AV_CODEC_ID_RV30: //68
      r6 = HI_UNF_VCODEC_TYPE_REAL8; //5
      break;
    case AV_CODEC_ID_RV40: //69
      r6 = HI_UNF_VCODEC_TYPE_REAL9; //6
      break;
    case AV_CODEC_ID_VC1: //70
      r6 = HI_UNF_VCODEC_TYPE_VC1; //7
      break;
    case AV_CODEC_ID_INDEO2: //75
      r6 = HI_UNF_VCODEC_TYPE_INDEO2; //30
      break;
    case AV_CODEC_ID_AVS: //82
      r6 = HI_UNF_VCODEC_TYPE_AVS; //2
      break;
    case AV_CODEC_ID_KMVC: //85?
      r6 = HI_UNF_VCODEC_TYPE_MVC; //35;
      break;
    case AV_CODEC_ID_CAVS: //87?
      r6 = HI_UNF_VCODEC_TYPE_AVS2; //39;
      break;
    case AV_CODEC_ID_JPEG2000: //88?
      r6 = HI_UNF_VCODEC_TYPE_JPEG; //15;
      break;
    case AV_CODEC_ID_VP5: //90
      r6 = HI_UNF_VCODEC_TYPE_VP5; //28
      break;
    case AV_CODEC_ID_VP6: //91
      r6 = HI_UNF_VCODEC_TYPE_VP6; //8
      break;
    case AV_CODEC_ID_VP6F: //92
      r6 = HI_UNF_VCODEC_TYPE_VP6F; //9
      break;
    case AV_CODEC_ID_VP6A: //106
      r6 = HI_UNF_VCODEC_TYPE_VP6A; //10
      break;
    case AV_CODEC_ID_INDEO4: //111
      r6 = HI_UNF_VCODEC_TYPE_INDEO4; //32
      break;
    case AV_CODEC_ID_INDEO5: //112
      r6 = HI_UNF_VCODEC_TYPE_INDEO5; //33
      break;
    case AV_CODEC_ID_VP8: //139
      r6 = HI_UNF_VCODEC_TYPE_VP8; //16
      break;
    case AV_CODEC_ID_VP9: //167
      r6 = HI_UNF_VCODEC_TYPE_VP9; //38
      break;
    case AV_CODEC_ID_HEVC: //173
      r6 = HI_UNF_VCODEC_TYPE_HEVC; //36
      break;
    default:
      r6 = HI_UNF_VCODEC_TYPE_BUTT; //40;
      break;
  }

  HI_VOID *pPara = HI_NULL;
  if (r6 == HI_UNF_VCODEC_TYPE_MVC/*35*/)
  {    
    stAvOpen.enDecType = HI_UNF_VCODEC_DEC_TYPE_BUTT; //2;
    stAvOpen.enCapLevel = HI_UNF_VCODEC_CAP_LEVEL_FULLHD; //4;
    stAvOpen.enProtocolLevel = HI_UNF_VCODEC_PRTCL_LEVEL_MVC; //2;
    pPara = &stAvOpen;
  }

  res = HI_UNF_AVPLAY_ChnOpen(m_hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID, pPara);
  if (res != HI_SUCCESS)
  {
    goto label_130c;
  }

  HI_UNF_VCODEC_ATTR_S stVdecAttr;

  res = HI_UNF_AVPLAY_GetAttr(m_hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, &stVdecAttr);
  if (res != HI_SUCCESS)
  {
    goto label_130c;
  }

  if (r6 == HI_UNF_VCODEC_TYPE_VC1/*7*/)
  {
    stVdecAttr.unExtAttr.stVC1Attr.bAdvancedProfile = (hints.profile == 3)? HI_TRUE: HI_FALSE;
    stVdecAttr.unExtAttr.stVC1Attr.u32CodecVersion = 8;
    printf("stVC1Attr bAdvancedProfile %d, version:%d\n", 
      stVdecAttr.unExtAttr.stVC1Attr.bAdvancedProfile, stVdecAttr.unExtAttr.stVC1Attr.u32CodecVersion);
  }

  if (r6 == HI_UNF_VCODEC_TYPE_VP6/*8*/)
  {
    stVdecAttr.unExtAttr.stVP6Attr.bReversed = HI_FALSE;
  }

  stVdecAttr.enType = r6;

  res = HI_UNF_AVPLAY_SetAttr(m_hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, &stVdecAttr);
  if (res != HI_SUCCESS)
  {
    goto label_130c;
  }

  res = HI_UNF_VO_AttachWindow(m_hWin, m_hAvplay);
  if (res != HI_SUCCESS)
  {
    goto label_14e0;
  }

  res = HI_UNF_VO_SetWindowEnable(m_hWin, HI_TRUE);
  if (res != HI_SUCCESS)
  {
    goto label_14e0;
  }

  res = HIADP_AVPlay_SetVdecAttr(m_hAvplay, r6, HI_UNF_VCODEC_MODE_NORMAL);
  if (res != HI_SUCCESS)
  {
    goto label_14e0;
  }

  if ((hints.fpsrate > 0) && (hints.fpsscale != 0))
  {
    HI_UNF_AVPLAY_FRMRATE_PARAM_S stFrameRate;
    stFrameRate.enFrmRateType = HI_UNF_AVPLAY_FRMRATE_TYPE_USER_PTS;
    stFrameRate.stSetFrmRate.u32fpsInteger = hints.fpsrate / hints.fpsscale;
    stFrameRate.stSetFrmRate.u32fpsDecimal = ((1000 * hints.fpsrate) / hints.fpsscale) % 1000;

    CLog::Log(LOGDEBUG, "HI_UNF_AVPLAY_SetAttr , frame rate : %d.%d\n", 
      stFrameRate.stSetFrmRate.u32fpsInteger, stFrameRate.stSetFrmRate.u32fpsDecimal);

    res = HI_UNF_AVPLAY_SetAttr(m_hAvplay, HI_UNF_AVPLAY_ATTR_ID_FRMRATE_PARAM, &stFrameRate);
    if (res != HI_SUCCESS)
    {
      goto label_14e0;
    }
  }

  res = HI_UNF_AVPLAY_Start(m_hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID, HI_NULL);
  if (res != HI_SUCCESS)
  {
    goto label_14c8;
  }

  CLog::Log(/*LOGDEBUG*/LOGINFO, "HISIAV[%s:%d]Open Video success", __MODULE_NAME__, 720);

  m_video_opend = true;
  m_video_full = false;
  m_video_eos = false;
  m_video_1st_pts = DVD_NOPTS_VALUE;
  m_video_play_started = false;
  m_video_ready = false;

  m_vptsQueue.clear();

  return true;

label_14c8:
  HI_UNF_AVPLAY_STOP_OPT_S stStopOpt;
  stStopOpt.u32TimeoutMs = 0;
  stStopOpt.enMode = HI_UNF_AVPLAY_STOP_MODE_BLACK;
  HI_UNF_AVPLAY_Stop(m_hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID, &stStopOpt);

label_14e0:
  HI_UNF_VO_SetWindowEnable(m_hWin, HI_FALSE);
  HI_UNF_VO_DetachWindow(m_hWin, m_hAvplay);

label_130c:
  HI_UNF_AVPLAY_ChnClose(m_hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID);
  return false;
}




