

#include "libavcodec/avcodec.h"
#include "utils/log.h"
#include "HiDecoder.h"
#include "DVDClock.h"



#if 0
extern void _Z42_107f1b933a62cea021c088004790e758008a3e49Av();
extern bool _Z42_d2050ca4b20719cc67b22956b468894d54294ff8Cv();
#endif


/* complete */
HisiAvDecoder* HisiAvDecoder::GetInstance()
{
  static HisiAvDecoder sHisiAvDecoder;
  return &sHisiAvDecoder;
}


/* complete */
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
  HisiAvDecoder* pThis = HisiAvDecoder::GetInstance();
  return pThis->EventReport(hAvplay, enEvent, pPara);
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

#if 0
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
#endif

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
#if 0
      goto label_2fc;
#else
      CLog::Log(LOGERROR, "HI_UNF_AVPLAY_RegisterEvent64 failed (0x%x) for enEvent %d", res, enEvent);
#endif
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

  CLog::Log(/*LOGDEBUG*/LOGINFO, "HISIAV[%s:%d]HisiAvDecoder::Init success", __FUNCTION__, 231);

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


/* complete */
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


bool HisiAvDecoder::AudioOpen(CDVDStreamInfo &hints)
{
  Init();

  CSingleLock lock(m_section);

  HA_CODEC_ID_E adec = codec2adec(hints.codec);
  HI_S32 res = HI_UNF_AVPLAY_ChnOpen(m_hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, NULL);
  if (res != HI_SUCCESS)
  {
    goto label_674;
  }

  HI_UNF_AUDIOTRACK_ATTR_S stAttr;

  res = HI_UNF_SND_GetDefaultTrackAttr(HI_UNF_SND_TRACK_TYPE_MASTER, &stAttr);
  if (res != HI_SUCCESS)
  {
    goto label_674;
  }

  res = HI_UNF_SND_CreateTrack(HI_UNF_SND_0, &stAttr, &m_hTrack);
  if (res != HI_SUCCESS)
  {
    goto label_824;
  }

  res = HI_UNF_SND_Attach(m_hTrack, m_hAvplay);
  if (res != HI_SUCCESS)
  {
    goto label_818;
  }

  if (hints.software)
  {
    WAV_FORMAT_S sp48;
 
    sp48.nChannels = 2;
    sp48.wBitsPerSample = 16;
    sp48.nSamplesPerSec = hints.samplerate;

    m_audio_bytes_pes_sec = sp48.nChannels * sp48.wBitsPerSample * sp48.nSamplesPerSec / 8;

    HI_UNF_ACODEC_ATTR_S stAudAttr;

    res = HI_UNF_AVPLAY_GetAttr(m_hAvplay, HI_UNF_AVPLAY_ATTR_ID_ADEC, &stAudAttr);
    if (res != HI_SUCCESS)
    {
      goto label_818;
    }

    stAudAttr.enType = HA_AUDIO_ID_PCM;

#if 0
#define HA_PCM_DecGetDefaultOpenParam(pOpenParam, pstPrivateParams, samplerate) \
do{ HI_S32 i; \
    ((HI_HADECODE_OPENPARAM_S *)(pOpenParam))->enDecMode = HD_DEC_MODE_RAWPCM; \
    ((HI_HADECODE_OPENPARAM_S *)(pOpenParam))->sPcmformat.u32DesiredOutChannels = 2; \
    ((HI_HADECODE_OPENPARAM_S *)(pOpenParam))->sPcmformat.bInterleaved  = HI_TRUE; \
    ((HI_HADECODE_OPENPARAM_S *)(pOpenParam))->sPcmformat.u32BitPerSample = 16; \
    ((HI_HADECODE_OPENPARAM_S *)(pOpenParam))->sPcmformat.u32DesiredSampleRate = samplerate; \
    for (i = 0; i < HA_AUDIO_MAXCHANNELS; i++) \
    { \
        ((HI_HADECODE_OPENPARAM_S *)pOpenParam)->sPcmformat.enChannelMapping[i] = HA_AUDIO_ChannelNone; \
    } \
    ((HI_HADECODE_OPENPARAM_S *)pOpenParam)->pCodecPrivateData = (HI_VOID*)pstPrivateParams; \
    ((HI_HADECODE_OPENPARAM_S *)pOpenParam)->u32CodecPrivateDataSize = sizeof(WAV_FORMAT_S); \
}while(0)

    HA_PCM_DecGetDefaultOpenParam(&stAudAttr.stDecodeParam, &sp48, hints.samplerate);
#else
    HA_PCM_DecGetDefalutOpenParam(&stAudAttr.stDecodeParam, &sp48);
    stAudAttr.stDecodeParam.sPcmformat.u32DesiredSampleRate = hints.samplerate;
#endif

    res = HI_UNF_AVPLAY_SetAttr(m_hAvplay, HI_UNF_AVPLAY_ATTR_ID_ADEC, &stAudAttr);
    if (res != HI_SUCCESS)
    {
      goto label_818;
    }
  }
  else
  {
    HIADP_AVPlay_SetAdecAttr(m_hAvplay, adec, HD_DEC_MODE_SIMUL, 0/*isCoreOnly*/);
  }

  res = HI_UNF_AVPLAY_Start(m_hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, HI_NULL);
  if (res != HI_SUCCESS)
  {
    goto label_800;
  }

  HI_UNF_SYNC_ATTR_S stSyncAttr;

  HI_UNF_AVPLAY_GetAttr(m_hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &stSyncAttr);

  stSyncAttr.enSyncRef = HI_UNF_SYNC_REF_AUDIO;
  stSyncAttr.stSyncStartRegion.s32VidPlusTime = 60;
  stSyncAttr.stSyncStartRegion.s32VidNegativeTime = -4;
  stSyncAttr.u32PreSyncTimeoutMs = 1000;
  stSyncAttr.bQuickOutput = HI_TRUE;

  HI_UNF_AVPLAY_SetAttr(m_hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &stSyncAttr);

  m_audio_opend = true;
  m_audio_full = false;
  m_audio_eos = false;
  m_audio_1st_pts = DVD_NOPTS_VALUE;
  m_audio_play_started = false;
  m_audio_ready = false;

  m_aptsQueue.clear();

  return true;

label_800:
  HI_UNF_AVPLAY_STOP_OPT_S stStopOpt;
  stStopOpt.u32TimeoutMs = 0;
  stStopOpt.enMode = HI_UNF_AVPLAY_STOP_MODE_BLACK;
  HI_UNF_AVPLAY_Stop(m_hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, &stStopOpt);

label_818:
  HI_UNF_SND_Detach(m_hTrack, m_hAvplay);

label_824:
  HI_UNF_SND_DestroyTrack(m_hTrack);
  m_hTrack = 0;

label_674:
  HI_UNF_AVPLAY_ChnClose(m_hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD);
  return false;
}


bool HisiAvDecoder::AudioClose()
{
  CSingleLock lock(m_section);

  if (m_hTrack == 0)
  {
    return false;
  }

  HI_UNF_AVPLAY_STOP_OPT_S stStopOpt;
  stStopOpt.u32TimeoutMs = 0;
  stStopOpt.enMode = HI_UNF_AVPLAY_STOP_MODE_BLACK;

  HI_S32 res = HI_UNF_AVPLAY_Stop(m_hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, &stStopOpt);
  if (res != HI_SUCCESS)
  {
    return false;
  }

  res = HI_UNF_SND_Detach(m_hTrack, m_hAvplay);
  if (res != HI_SUCCESS)
  {
    return false;
  }

  res = HI_UNF_SND_DestroyTrack(m_hTrack);
  if (res != HI_SUCCESS)
  {
    return false;
  }

  m_hTrack = 0;

  res = HI_UNF_AVPLAY_ChnClose(m_hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD);
  if (res != HI_SUCCESS)
  {
    return false;
  }

  HI_UNF_SYNC_ATTR_S stSyncAttr;

  HI_UNF_AVPLAY_GetAttr(m_hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &stSyncAttr);

  stSyncAttr.enSyncRef = HI_UNF_SYNC_REF_NONE;

  HI_UNF_AVPLAY_SetAttr(m_hAvplay, HI_UNF_AVPLAY_ATTR_ID_SYNC, &stSyncAttr);

  AudioReset();

  m_audio_opend = false;

  return true;
}


/* complete */
void HisiAvDecoder::AudioReset()
{
  m_audio_full = false;
  m_audio_eos = false;
  m_audio_play_started = false;
  m_audio_ready = false;
  m_audio_1st_pts = DVD_NOPTS_VALUE;

  m_aptsQueue.clear();

  HI_UNF_AVPLAY_Reset(m_hAvplay, NULL);
}


/* complete */
void HisiAvDecoder::AudioReSync(unsigned int pts)
{
  //empty
}

	
/* todo */
double HisiAvDecoder::AudioFirstPts()
{
  CSingleLock lock(m_section);

  if (!m_audio_opend)
    return 0;

  if (!m_aptsQueue.size())
    return DVD_NOPTS_VALUE;

  return m_audio_1st_pts;
}


/* complete */
double HisiAvDecoder::AudioCurrentPts()
{
  if (!m_audio_opend)
    return DVD_NOPTS_VALUE;

  HI_UNF_AVPLAY_STATUS_INFO_S stStatusInfo;
  HI_S32 res = HI_UNF_AVPLAY_GetStatusInfo(m_hAvplay, &stStatusInfo);
  if (res != HI_SUCCESS)
  {
    return DVD_NOPTS_VALUE;
  }
  
  return stStatusInfo.stSyncStatus.u32LastAudPts * 1000.0f;
}


/* complete */
bool HisiAvDecoder::AudioPlayStarted()
{
  return m_audio_play_started;
}


/* complete */
bool HisiAvDecoder::AudioBufferReady()
{
  return m_audio_ready;
}


/* complete */
bool HisiAvDecoder::AudioBufferFull()
{
  return m_audio_full;
}


/* complete */
bool HisiAvDecoder::AudioWrite(uint8_t *pData, size_t iSize, unsigned int pts_ms)
{
  CSingleLock lock(m_section);

  HI_S32 res;

  if (!m_audio_opend)
    return false;

  m_audio_last_package_pts = pts_ms;

  HI_UNF_STREAM_BUF_S  stData;
  res = HI_UNF_AVPLAY_GetBuf(m_hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_AUD, iSize, &stData, 0/*u32TimeOutMs*/);
  if (res != HI_SUCCESS)
  {
    return false;
  }

  memcpy(stData.pu8Data, pData, iSize);

  res = HI_UNF_AVPLAY_PutBuf(m_hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_AUD, iSize, pts_ms);
  if (res != HI_SUCCESS)
  {
    return false;
  }

  return true;
}


/* complete */
bool HisiAvDecoder::AudioWriteEx(uint8_t *pData, size_t iSize, unsigned int pts_ms, bool continues, bool last)
{
  CSingleLock lock(m_section);

  HI_S32 res;

  if (!m_audio_opend)
    return false;

  m_audio_last_package_pts = pts_ms;

  HI_UNF_STREAM_BUF_S  stData;
  res = HI_UNF_AVPLAY_GetBuf(m_hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_AUD, iSize, &stData, 0/*u32TimeOutMs*/);
  if (res != HI_SUCCESS)
  {
    return false;
  }

  memcpy(stData.pu8Data, pData, iSize);

  HI_UNF_AVPLAY_PUTBUFEX_OPT_S stPutOpt;
  stPutOpt.bContinue = continues? HI_FALSE: HI_TRUE;
  stPutOpt.bEndOfFrm = last? HI_FALSE: HI_TRUE;

  res = HI_UNF_AVPLAY_PutBufEx(m_hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_AUD, iSize, pts_ms, &stPutOpt);
  if (res != HI_SUCCESS)
  {
    return false;
  }

  return true;
}


double HisiAvDecoder::AudioCacheTotal()
{
  return m_audio_cache_total * 0.001;
}


/* complete */
double HisiAvDecoder::AudioCachetime()
{
  return m_audio_cache_time * 0.001;
}


/* complete */
double HisiAvDecoder::AudioDelay()
{
  if (m_aptsQueue.size())
  {
    return (m_audio_last_package_pts - m_aptsQueue.front())/1000000 * 1000.0;
  }
  else
  {
    return AudioCachetime();
  }
}

	
/* complete */
void HisiAvDecoder::AudioSubmitEOS()
{
  m_audio_eos = true;
  m_eos = false;

  if (m_video_opend && !m_video_eos)
    return;

  HI_UNF_AVPLAY_FLUSH_STREAM_OPT_S stFlushOpt;
  HI_UNF_AVPLAY_FlushStream(m_hAvplay, &stFlushOpt);
}


/* complete */
void HisiAvDecoder::VideoReset()
{
  m_video_full = false;
  m_video_eos = false;
  m_video_1st_pts = DVD_NOPTS_VALUE;
  m_video_play_started = false;
  m_video_ready = false;

  m_vptsQueue.clear();

  HI_UNF_AVPLAY_Reset(m_hAvplay, NULL);
   
  HI_BOOL bIsEmpty = HI_FALSE;
  HI_UNF_AVPLAY_IsBuffEmpty(m_hAvplay, &bIsEmpty);

#if 0
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
  printf("HisiAvDecoder::VideoReset()\n");
#endif
}


bool HisiAvDecoder::VideoClose()
{
  CSingleLock lock(m_section);

  HI_S32 res;

  if (!m_video_opend)
  {
    goto error;
  }

  HI_UNF_AVPLAY_STATUS_INFO_S stStatusInfo;
  res = HI_UNF_AVPLAY_GetStatusInfo(m_hAvplay, &stStatusInfo);
  if (res != HI_SUCCESS)
  {
    goto error;
  }

  if (stStatusInfo.enRunStatus == HI_UNF_AVPLAY_STATUS_PAUSE)
  {
    res = HI_UNF_AVPLAY_Resume(m_hAvplay, NULL);
    if (res != HI_SUCCESS)
    {
      goto error;
    }
  }

  HI_UNF_AVPLAY_STOP_OPT_S stStopOpt;
  stStopOpt.u32TimeoutMs = 0;
  stStopOpt.enMode = HI_UNF_AVPLAY_STOP_MODE_BLACK;

  res = HI_UNF_AVPLAY_Stop(m_hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID, &stStopOpt);
  if (res != HI_SUCCESS)
  {
    goto error;
  }

  res = HI_UNF_VO_SetWindowEnable(m_hWin, HI_FALSE);
  if (res != HI_SUCCESS)
  {
    goto error;
  }

  res = HI_UNF_VO_DetachWindow(m_hWin, m_hAvplay);
  if (res != HI_SUCCESS)
  {
    goto error;
  }

  res = HI_UNF_AVPLAY_ChnClose(m_hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID);
  if (res != HI_SUCCESS)
  {
    goto error;
  }

  VideoReset();

  m_video_eos = true;
  m_video_opend = false;

  m_vptsQueue.clear();

  return true;

error:
//  printf("error");
  return false;
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

  HI_UNF_VCODEC_TYPE_E vdec = codec2vdec(hints.codec);

  HI_VOID *pPara = HI_NULL;
  if (vdec == HI_UNF_VCODEC_TYPE_MVC/*35*/)
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

  if (vdec == HI_UNF_VCODEC_TYPE_VC1/*7*/)
  {
    stVdecAttr.unExtAttr.stVC1Attr.bAdvancedProfile = (hints.profile == 3)? HI_TRUE: HI_FALSE;
    stVdecAttr.unExtAttr.stVC1Attr.u32CodecVersion = 8;
    printf("stVC1Attr bAdvancedProfile %d, version:%d\n", 
      stVdecAttr.unExtAttr.stVC1Attr.bAdvancedProfile, stVdecAttr.unExtAttr.stVC1Attr.u32CodecVersion);
  }

  if (vdec == HI_UNF_VCODEC_TYPE_VP6/*8*/)
  {
    stVdecAttr.unExtAttr.stVP6Attr.bReversed = HI_FALSE;
  }

  stVdecAttr.enType = vdec;

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

  res = HIADP_AVPlay_SetVdecAttr(m_hAvplay, vdec, HI_UNF_VCODEC_MODE_NORMAL);
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

  CLog::Log(/*LOGDEBUG*/LOGINFO, "HISIAV[%s:%d]Open Video success", __FUNCTION__, 720);

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


void HisiAvDecoder::VideoReSync(unsigned int pts)
{
  /*empty*/
}

	
/* complete */
void HisiAvDecoder::VideoSetSpeed(int speed)
{
  CLog::Log(LOGDEBUG, "HisiAvDecoder::SetSpeed (%d)", speed);

  if (!m_opened)
    return;

  if (speed == m_speed)
    return;

  switch (speed)
  {
    case 0:
      if ((m_mode == VIDEO_MODE_FORWARD) || (m_mode == VIDEO_MODE_BACKWARD))
      {
        HI_UNF_AVPLAY_Reset(m_hAvplay, NULL);
      }

      m_mode = VIDEO_MODE_PAUSE;
      HI_UNF_AVPLAY_Pause(m_hAvplay, NULL);
      HI_UNF_AVPLAY_SetDecodeMode(m_hAvplay, HI_UNF_VCODEC_MODE_NORMAL);
      break;

    case 1000:
      if ((m_mode == VIDEO_MODE_FORWARD) || (m_mode == VIDEO_MODE_BACKWARD))
      {
        HI_UNF_AVPLAY_Reset(m_hAvplay, NULL);
      }

      m_mode = VIDEO_MODE_NORMAL;
      HI_UNF_AVPLAY_SetDecodeMode(m_hAvplay, HI_UNF_VCODEC_MODE_NORMAL);
      HI_UNF_AVPLAY_Resume(m_hAvplay, NULL);
      break;

    default:
    {
      HI_UNF_AVPLAY_TPLAY_OPT_S stTplayOpt;
      if (speed > 0)
      {
        CLog::Log(LOGDEBUG, "CHisiCodec::SetSpeed goes forward, speed:%d", speed);

        stTplayOpt.enTplayDirect = HI_UNF_AVPLAY_TPLAY_DIRECT_FORWARD;

        if (m_mode != VIDEO_MODE_FORWARD)
        {
          HI_UNF_AVPLAY_Reset(m_hAvplay, NULL);
        }

        m_mode = VIDEO_MODE_FORWARD;
      }
      else
      {
        CLog::Log(LOGDEBUG, "CHisiCodec::SetSpeed goes backward, speed:%d", speed);
        speed = -speed;

        stTplayOpt.enTplayDirect = HI_UNF_AVPLAY_TPLAY_DIRECT_BACKWARD;

        if (m_mode != VIDEO_MODE_BACKWARD)
        {
          HI_UNF_AVPLAY_Reset(m_hAvplay, NULL);
        }

        m_mode = VIDEO_MODE_BACKWARD;
      }

      stTplayOpt.u32SpeedInteger = speed;
      stTplayOpt.u32SpeedDecimal = 0;

      HI_UNF_AVPLAY_SetDecodeMode(m_hAvplay, HI_UNF_VCODEC_MODE_I);
      HI_UNF_AVPLAY_Tplay(m_hAvplay, &stTplayOpt);
      break;
    }
  }

  m_speed = speed;
}


/* complete */
double HisiAvDecoder::VideoCurrentPts()
{
//  if (!m_video_opend)
  if (!m_opened)
    return DVD_NOPTS_VALUE;

  HI_UNF_AVPLAY_STATUS_INFO_S stStatusInfo;
  HI_S32 res = HI_UNF_AVPLAY_GetStatusInfo(m_hAvplay, &stStatusInfo);
  if (res != HI_SUCCESS)
  {
    return DVD_NOPTS_VALUE;
  }
  
  return stStatusInfo.stSyncStatus.u32LastVidPts * 1000.0f;
}


/* complete */
bool HisiAvDecoder::VideoPlayStarted()
{
  return m_video_play_started;
}


/* complete */
bool HisiAvDecoder::VideoFramePts(int &pts)
{
  CSingleLock lock(m_EventMutex);

  if (!m_vptsQueue.size())
    return false;

  pts = m_vptsQueue.front();
  m_vptsQueue.pop_front();

  return true;
}

	
/* todo */
double HisiAvDecoder::VideoFirstPts()
{
  if (!m_video_opend)
    return 0;

  if (m_vptsQueue.size())
    return m_video_1st_pts;

  return DVD_NOPTS_VALUE;
}


/* complete */
bool HisiAvDecoder::VideoBufferFull()
{
  return m_video_full;
}


/* complete */
bool HisiAvDecoder::VideoBufferReady()
{
  return m_video_ready;
}


/* complete */
bool HisiAvDecoder::VideoWrite(uint8_t *pData, size_t iSize, unsigned int pts_ms)
{
  HI_S32 res;

  if (!m_video_opend)
    return false;

  HI_UNF_STREAM_BUF_S  stData;
  res = HI_UNF_AVPLAY_GetBuf(m_hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_VID, iSize, &stData, 0/*u32TimeOutMs*/);
  if (res != HI_SUCCESS)
  {
    return false;
  }

  memcpy(stData.pu8Data, pData, iSize);

  res = HI_UNF_AVPLAY_PutBuf(m_hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_VID, iSize, pts_ms);
  if (res != HI_SUCCESS)
  {
    return false;
  }

  return true;
}


/* complete */
bool HisiAvDecoder::VideoWriteEx(uint8_t *pData, size_t iSize, unsigned int pts_ms, bool continues, bool last)
{
  HI_S32 res;

  if (!m_video_opend)
    return false;

  HI_UNF_STREAM_BUF_S  stData;
  res = HI_UNF_AVPLAY_GetBuf(m_hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_VID, iSize, &stData, 0/*u32TimeOutMs*/);
  if (res != HI_SUCCESS)
  {
    return false;
  }

  memcpy(stData.pu8Data, pData, iSize);

  HI_UNF_AVPLAY_PUTBUFEX_OPT_S stPutOpt;
  stPutOpt.bContinue = continues? HI_FALSE: HI_TRUE;
  stPutOpt.bEndOfFrm = last? HI_FALSE: HI_TRUE;

  res = HI_UNF_AVPLAY_PutBufEx(m_hAvplay, HI_UNF_AVPLAY_BUF_ID_ES_VID, iSize, pts_ms, &stPutOpt);
  if (res != HI_SUCCESS)
  {
    return false;
  }

  return true;
}

	
/* complete */
void HisiAvDecoder::VideoSubmitEOS()
{
  m_video_eos = true;
  m_eos = false;

  if (m_audio_opend && !m_audio_eos)
    return;

  HI_UNF_AVPLAY_FLUSH_STREAM_OPT_S stFlushOpt;
  HI_UNF_AVPLAY_FlushStream(m_hAvplay, &stFlushOpt);
}


/* complete */
void HisiAvDecoder::VideoSetRect(int x, int y, int width, int height)
{
  HI_UNF_WINDOW_ATTR_S winAttr;
  HI_S32 res = HI_UNF_VO_GetWindowAttr(m_hWin, &winAttr);
  if (res == HI_SUCCESS)
  { 
    winAttr.stOutputRect.s32X = x;
    winAttr.stOutputRect.s32Y = y;
    winAttr.stOutputRect.s32Width = width;
    winAttr.stOutputRect.s32Height = height;

    res = HI_UNF_VO_SetWindowAttr(m_hWin, &winAttr);
    if (res == HI_SUCCESS)
    {
      return;
    }
  }

  CLog::Log(LOGDEBUG, "HI_UNF_VO_GetWindowAttr error");
}

	
/* complete */
bool HisiAvDecoder::IsEOS()
{ 
  bool ret = false;

  if (m_audio_opend && !m_audio_eos)
    ;
  else if (m_video_opend && !m_video_eos)
    ;
  else
  {
    HI_BOOL bIsEmpty = HI_FALSE;
    HI_UNF_AVPLAY_IsBuffEmpty(m_hAvplay, &bIsEmpty);

    if (bIsEmpty)
      ret = m_eos;
  }
  return ret;
}


/* complete */
void HisiAvDecoder::SubmitEOS()
{
  m_eos = false;

  if (m_audio_opend && !m_audio_eos)
    return;

  if (m_video_opend && !m_video_eos)
    return;

  HI_UNF_AVPLAY_FLUSH_STREAM_OPT_S stFlushOpt;
  HI_UNF_AVPLAY_FlushStream(m_hAvplay, &stFlushOpt);
}


HA_CODEC_ID_E HisiAvDecoder::codec2adec(int enc)
{
  switch ((AVCodecID) enc)
  {
    case AV_CODEC_ID_MP2: //0x15000:
      return HA_AUDIO_ID_MP2; //0x20000002

    case AV_CODEC_ID_AAC: //0x15002:
      return HA_AUDIO_ID_AAC; //0x80020001

    case AV_CODEC_ID_AC3: //0x15003:
      return HA_AUDIO_ID_CUSTOM_0; //0x81f00400;

    case AV_CODEC_ID_DTS: //0x15004:
      return HA_AUDIO_ID_DTSHD; //0x20041020

    case AV_CODEC_ID_VORBIS: //0x15005:
      return HA_AUDIO_ID_VORBIS; //0x80051007

    case AV_CODEC_ID_WMAV1: //0x15007:
    case AV_CODEC_ID_WMAV2: //0x15008:
      return HA_AUDIO_ID_WMA9STD; //0x21f00006

    case AV_CODEC_ID_MP3ADU: //0x1500d
    case AV_CODEC_ID_MP3ON4: //0x1500e
      return HA_AUDIO_ID_MP3; //0x21f00003

    case AV_CODEC_ID_COOK: //0x15014
      return HA_AUDIO_ID_COOK; //0x80160009

    case AV_CODEC_ID_PCM_BLURAY: //0x10018
      return HA_AUDIO_ID_BLYRAYLPCM; //0x81210021;

    case AV_CODEC_ID_PCM_S16LE: // = 0x10000
    case AV_CODEC_ID_PCM_S16BE:
    case AV_CODEC_ID_PCM_U16LE:
    case AV_CODEC_ID_PCM_U16BE:
    case AV_CODEC_ID_PCM_S8:
    case AV_CODEC_ID_PCM_U8:
    case AV_CODEC_ID_PCM_MULAW:
    case AV_CODEC_ID_PCM_ALAW:
    case AV_CODEC_ID_PCM_S32LE:
    case AV_CODEC_ID_PCM_S32BE:
    case AV_CODEC_ID_PCM_U32LE:
    case AV_CODEC_ID_PCM_U32BE:
    case AV_CODEC_ID_PCM_S24LE:
    case AV_CODEC_ID_PCM_S24BE:
    case AV_CODEC_ID_PCM_U24LE:
    case AV_CODEC_ID_PCM_U24BE:
    case AV_CODEC_ID_PCM_S24DAUD: //0x10010
    case AV_CODEC_ID_PCM_ZORK:
    case AV_CODEC_ID_PCM_S16LE_PLANAR:
    case AV_CODEC_ID_PCM_DVD:
    case AV_CODEC_ID_PCM_F32BE:
    case AV_CODEC_ID_PCM_F32LE:
    case AV_CODEC_ID_PCM_F64BE: //0x10016
    case AV_CODEC_ID_PCM_F64LE: //0x10017
    case AV_CODEC_ID_PCM_LXF: //0x10019
    case AV_CODEC_ID_S302M:
    case AV_CODEC_ID_PCM_S8_PLANAR:
    case AV_CODEC_ID_PCM_S24LE_PLANAR:
    case AV_CODEC_ID_PCM_S32LE_PLANAR:
    case AV_CODEC_ID_PCM_S16BE_PLANAR: //0x1001E
      //84c
      return HA_AUDIO_ID_PCM; //0x81000000

    default:
      break;
  }

  if ((enc >= AV_CODEC_ID_ADPCM_IMA_QT/*0x11000*/) && (enc <= AV_CODEC_ID_ADPCM_G726LE/*0x11804*/))
    return HA_AUDIO_ID_ADPCM; //0x21300107

  if ((enc >= AV_CODEC_ID_AMR_NB/*0x12000*/) && (enc <= AV_CODEC_ID_SOL_DPCM/*0x14003*/))
    return HA_AUDIO_ID_AMRNB; //0x81600100

  return HA_AUDIO_ID_MP3; //0x21f00003
}


HI_UNF_VCODEC_TYPE_E HisiAvDecoder::codec2vdec(int enc)
{
  switch (enc)
  {
    case AV_CODEC_ID_MPEG1VIDEO: //1:
    case AV_CODEC_ID_MPEG2VIDEO: //2:
    case AV_CODEC_ID_MPEG2VIDEO_XVMC: 
      return HI_UNF_VCODEC_TYPE_MPEG2; //0;
    case AV_CODEC_ID_H261: //3
      return HI_UNF_VCODEC_TYPE_H261; //26;
    case AV_CODEC_ID_H263: //4
      return HI_UNF_VCODEC_TYPE_H263; //3;
    case AV_CODEC_ID_RV10: //5
      return HI_UNF_VCODEC_TYPE_RV10; //22;
    case AV_CODEC_ID_RV20: //6
      return HI_UNF_VCODEC_TYPE_RV20; //23;
    case AV_CODEC_ID_MJPEG: //7
      return HI_UNF_VCODEC_TYPE_MJPEG; //11;
    case AV_CODEC_ID_MJPEGB: //8
      return HI_UNF_VCODEC_TYPE_MJPEGB; //34;
    case AV_CODEC_ID_LJPEG: //9
      return HI_UNF_VCODEC_TYPE_JPEG; //15;
    case AV_CODEC_ID_MPEG4: //12
      return HI_UNF_VCODEC_TYPE_MPEG4; //1;
    case AV_CODEC_ID_RAWVIDEO: //13
      return HI_UNF_VCODEC_TYPE_RAW; //14;
    case AV_CODEC_ID_MSMPEG4V1: //14
      return HI_UNF_VCODEC_TYPE_MSMPEG4V1; //17;
    case AV_CODEC_ID_MSMPEG4V2: //15
      return HI_UNF_VCODEC_TYPE_MSMPEG4V2; //18;
    case AV_CODEC_ID_MSMPEG4V3: //16
      return HI_UNF_VCODEC_TYPE_DIVX3; //13;
    case AV_CODEC_ID_WMV1: //17
      return HI_UNF_VCODEC_TYPE_WMV1; //20;
    case AV_CODEC_ID_WMV2: //18
      return HI_UNF_VCODEC_TYPE_WMV2; //21
    case AV_CODEC_ID_FLV1: //21
      return HI_UNF_VCODEC_TYPE_SORENSON; //12
    case AV_CODEC_ID_SVQ1: //22
      return HI_UNF_VCODEC_TYPE_SVQ1; //24
    case AV_CODEC_ID_SVQ3: //23
      return HI_UNF_VCODEC_TYPE_SVQ3; //25
    case AV_CODEC_ID_DVVIDEO: //24
      return HI_UNF_VCODEC_TYPE_DV; //37
    case AV_CODEC_ID_H264: //27
      return HI_UNF_VCODEC_TYPE_H264; //4
    case AV_CODEC_ID_INDEO3: //28
      return HI_UNF_VCODEC_TYPE_INDEO3; //31
    case AV_CODEC_ID_VP3: //29
      return HI_UNF_VCODEC_TYPE_VP3; //27
    case AV_CODEC_ID_CINEPAK: //43
      return HI_UNF_VCODEC_TYPE_CINEPAK; //29
    case AV_CODEC_ID_MSVIDEO1: //46
      return HI_UNF_VCODEC_TYPE_MSVIDEO1; //19
    case AV_CODEC_ID_RV30: //68
      return HI_UNF_VCODEC_TYPE_REAL8; //5
    case AV_CODEC_ID_RV40: //69
      return HI_UNF_VCODEC_TYPE_REAL9; //6
    case AV_CODEC_ID_VC1: //70
      return HI_UNF_VCODEC_TYPE_VC1; //7
    case AV_CODEC_ID_INDEO2: //75
      return HI_UNF_VCODEC_TYPE_INDEO2; //30
    case AV_CODEC_ID_AVS: //82
      return HI_UNF_VCODEC_TYPE_AVS; //2
    case AV_CODEC_ID_KMVC: //85?
      return HI_UNF_VCODEC_TYPE_MVC; //35;
    case AV_CODEC_ID_CAVS: //87?
      return HI_UNF_VCODEC_TYPE_AVS2; //39;
    case AV_CODEC_ID_JPEG2000: //88?
      return HI_UNF_VCODEC_TYPE_JPEG; //15;
    case AV_CODEC_ID_VP5: //90
      return HI_UNF_VCODEC_TYPE_VP5; //28
    case AV_CODEC_ID_VP6: //91
      return HI_UNF_VCODEC_TYPE_VP6; //8
    case AV_CODEC_ID_VP6F: //92
      return HI_UNF_VCODEC_TYPE_VP6F; //9
    case AV_CODEC_ID_VP6A: //106
      return HI_UNF_VCODEC_TYPE_VP6A; //10
    case AV_CODEC_ID_INDEO4: //111
      return HI_UNF_VCODEC_TYPE_INDEO4; //32
    case AV_CODEC_ID_INDEO5: //112
      return HI_UNF_VCODEC_TYPE_INDEO5; //33
    case AV_CODEC_ID_VP8: //139
      return HI_UNF_VCODEC_TYPE_VP8; //16
    case AV_CODEC_ID_VP9: //167
      return HI_UNF_VCODEC_TYPE_VP9; //38
    case AV_CODEC_ID_HEVC: //173
      return HI_UNF_VCODEC_TYPE_HEVC; //36
    default:
      return HI_UNF_VCODEC_TYPE_BUTT; //40;
  }
}


HI_S32 HisiAvDecoder::EventReport(HI_HANDLE hAvplay, HI_UNF_AVPLAY_EVENT_E enEvent, HI_VOID* pPara)
{
  CSingleLock lock(m_EventMutex);

  switch (enEvent)
  {
    case HI_UNF_AVPLAY_EVENT_EOS: 
      m_eos = true;
      break;

    case HI_UNF_AVPLAY_EVENT_NEW_VID_FRAME:
    {
      typedef struct
      { 
        int fill[16];
        HI_U32 u32PtsMs;
      } HI_UNF_VO_FRAMEINFO_S;

      HI_UNF_VO_FRAMEINFO_S* pFrameInfo = (HI_UNF_VO_FRAMEINFO_S*)pPara;

      if (m_vptsQueue.begin() == m_vptsQueue.end()) //!m_vptsQueue.size())
      {
        m_video_1st_pts = pFrameInfo->u32PtsMs * 1000.0f;
      }

      m_vptsQueue.push_back(pFrameInfo->u32PtsMs);
      break;
    }

    case HI_UNF_AVPLAY_EVENT_NEW_AUD_FRAME:
    {
      HI_UNF_AO_FRAMEINFO_S* pFrameInfo = (HI_UNF_AO_FRAMEINFO_S*)pPara;

      if (m_aptsQueue.begin() == m_aptsQueue.end()) //!m_aptsQueue.size())
      {
        m_audio_1st_pts = pFrameInfo->u32PtsMs * 1000.0f;
      }

      m_aptsQueue.push_back(pFrameInfo->u32PtsMs);
      m_audio_play_started = true;
      break;
    }

    case HI_UNF_AVPLAY_EVENT_SYNC_STAT_CHANGE:
    {
      HI_UNF_SYNC_STAT_PARAM_S* pSynInfo = (HI_UNF_SYNC_STAT_PARAM_S*)pPara;

      printf("sync info, vid&aud:%d, vid&pcr:%d, aud&pcr:%d, vid@local:%u, aud@local:%u, pcr@local:%u\n",
        pSynInfo->s32VidAudDiff, pSynInfo->s32VidPcrDiff, pSynInfo->s32AudPcrDiff, 
        pSynInfo->u32VidLocalTime, pSynInfo->u32AudLocalTime, pSynInfo->u32PcrLocalTime);
      break;
    }

    case HI_UNF_AVPLAY_EVENT_VID_BUF_STATE:
    {
      HI_UNF_AVPLAY_BUF_STATE_E state = *((HI_UNF_AVPLAY_BUF_STATE_E*)pPara);
      m_video_full = (state <= HI_UNF_AVPLAY_BUF_STATE_NORMAL)? false: true;

      if (!m_video_ready && (state >= HI_UNF_AVPLAY_BUF_STATE_LOW))
      {
        m_audio_ready = true;
        m_video_ready = true;
      }
      break;
    }

    case HI_UNF_AVPLAY_EVENT_AUD_BUF_STATE:
    {
      HI_UNF_AVPLAY_BUF_STATE_E state = *((HI_UNF_AVPLAY_BUF_STATE_E*)pPara);
      m_audio_full = (state <= HI_UNF_AVPLAY_BUF_STATE_NORMAL)? false: true;

      if (!m_audio_ready && (state >= HI_UNF_AVPLAY_BUF_STATE_LOW))
      {
        m_audio_ready = true;
        m_video_ready = true;
      }
      break;
    }

    case HI_UNF_AVPLAY_EVENT_FIRST_FRAME_DISPLAYED:
    {
      m_video_play_started = true;
      break;
    }

    case HI_UNF_AVPLAY_EVENT_STATUS_REPORT:
    {
      hiUNF_AVPLAY_STATUS_INFO_S* pStatusInfo = (hiUNF_AVPLAY_STATUS_INFO_S*)pPara;

      m_audio_cache_time = pStatusInfo->stBufStatus[HI_UNF_AVPLAY_BUF_ID_ES_AUD].u32FrameBufTime;
      m_audio_cache_total = m_audio_cache_time + pStatusInfo->stBufStatus[HI_UNF_AVPLAY_BUF_ID_ES_AUD].u32UsedSize / (m_audio_bytes_pes_sec * 0.001);
      break;
    }

    default:
      break;
  };
}





