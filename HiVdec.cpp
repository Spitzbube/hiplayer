
#include "utils/log.h"
#include "settings/DisplaySettings.h"
#include "DVDClock.h"
#include "HiVdec.h"


/* complete */
void CHisiVdec::Process()
{
  CLog::Log(LOGDEBUG, "CHisiVdec::Process Started");

  while (!m_bStop)
  {
    Sleep(10);

    {
      CSingleLock lock(m_ptsQueueMutex);
      int pts = 0; 

      if (hwDec->VideoFramePts(pts))
      {
        m_ptsQueue.push_back(pts);
        m_ready_event.Set();
      }
    }
  }

  CLog::Log(LOGDEBUG, "CHisiVdec::Process Stopped");
}


/* complete */
CHisiVdec::CHisiVdec() : CThread("CHisiVdec")
{
  m_opened = false;
  m_extradata = NULL;
  m_extrasize = 0;
  m_header = NULL;
  m_headersize = 0;
  hwDec = HisiAvDecoder::GetInstance();
}


/* complete */
CHisiVdec::~CHisiVdec()
{
  StopThread();
}


/* todo */
bool CHisiVdec::OpenDecoder(CDVDStreamInfo &hints)
{
  m_mode = 0;
  m_speed = 1000;
  m_zoom = -1;
  m_contrast = -1; 
  m_brightness = -1;

  m_hints = hints;

  m_extraneed = true;

//  memset(&m_dst_rect, 0, sizeof(m_dst_rect));
//  memset(&m_display_rect, 0, sizeof(m_display_rect));
  m_dst_rect.SetRect(0, 0, 0, 0);
//  m_display_rect.SetRect(0, 0, 0, 0);

//  m_view_mode = 0;
	
//  m_stereo_mode = RENDER_STEREO_MODE_OFF;
//  m_stereo_view = RENDER_STEREO_VIEW_OFF;

  m_extradata = (unsigned char*) malloc(hints.extrasize);
  m_extrasize = hints.extrasize;
  memcpy(m_extradata, hints.extradata, hints.extrasize);

  SetHeader();

  unsigned i = 0;

  CLog::Log(LOGDEBUG, "CHisiVdec::OpenDecoder extra size %d", m_extrasize);

  while (i < m_extrasize)
  {
    char c = m_extradata[i];
    i++;
    printf("0x%x ", c);
    if (!(i%10))
      printf("\n");
  }
  printf("\n");

  if ((hints.fpsrate > 0) && (hints.fpsscale != 0))
  {
    m_video_rate = 0.5 + (float)UNIT_FREQ * hints.fpsscale / hints.fpsrate;
  }

  if ((hints.width == 1920) && (m_video_rate == 1920))
  {
    CLog::Log(LOGDEBUG, "CHisiVdec::OpenDecoder video_rate exception");
    m_video_rate = 0.5 + (float)UNIT_FREQ * 1001 / 25000;
  }

  if (hints.codec == AV_CODEC_ID_H264 && hints.width <= 720 && m_video_rate == 1602)
  {
    CLog::Log(LOGDEBUG, "CHisiVdec::OpenDecoder video_rate exception");
    m_video_rate = 0.5 + (float)UNIT_FREQ * 1001 / 24000;
  }

  if (hints.codec == AV_CODEC_ID_H264 && hints.width <= 720)
  {
    if (m_video_rate >= 3200 && m_video_rate <= 3210)
    {
      CLog::Log(LOGDEBUG, "CAMLCodec::OpenDecoder video_rate exception");
      m_video_rate = 0.5 + (float)UNIT_FREQ * 1001 / 24000;
    }
  }

  CLog::Log(LOGDEBUG, "%s: detected new framerate(%f), video_rate(%d)", "DVDVideoCodecHisi", 
    (double)hints.fpsrate / (double)hints.fpsscale, m_video_rate);

  printf("Update Current resolution %d,%d, %s,%s,%s\n", 
    CDisplaySettings::GetInstance().GetCurrentResolutionInfo().iWidth,
    CDisplaySettings::GetInstance().GetCurrentResolutionInfo().iHeight,
    CDisplaySettings::GetInstance().GetCurrentResolutionInfo().strMode.c_str(),
    CDisplaySettings::GetInstance().GetCurrentResolutionInfo().strId.c_str(),
    CDisplaySettings::GetInstance().GetCurrentResolutionInfo().strOutput.c_str());

  bool res = hwDec->VideoOpen(hints);
  if (!res)
  {
    CLog::Log(LOGINFO, "[%s:%d]OpenVideo return falied", __FUNCTION__, 191);
  }
  else
  {
    CLog::Log(LOGINFO, "[%s:%d]OpenDecoder success", __FUNCTION__, 195);
    
    Create();
    
    m_opened = true;
  }

  return res;
}


/* todo */
void CHisiVdec::CloseDecoder()
{
  m_opened = false;

  StopThread();

  hwDec->VideoClose();

  delete m_extradata;
  m_extradata = NULL;
  delete m_header;
  m_header = NULL;

  CLog::Log(LOGDEBUG, "CHisiVdec::CloseDecoder");
}


/* complete */
void CHisiVdec::Reset()
{
  CLog::Log(LOGDEBUG, "CHisiVdec::Reset!");

  if (m_opened)
  {
    return;
  }

  m_extraneed = true;
  m_ptsQueue.clear();

  return hwDec->VideoReset();
}


/* todo */
void CHisiVdec::ReSync(double pts)
{
  CLog::Log(LOGDEBUG, "CHisiVdec::ReSync!");

  if (!m_opened)
  {
    return;
  }

  double new_pts = (pts < 0.0)? 0.0: pts;

  m_extraneed = true;

  SetHeader();

  m_ptsQueue.clear();

  return hwDec->VideoReSync(new_pts / 1000);
}


/* todo */
int CHisiVdec::Decode(uint8_t *pData, size_t size, double dts, double pts, std::atomic_bool& abort)
{
  int rtn = 0;  
  unsigned int pts_ms = 0;
  bool r9 = 1;

  if (!m_opened)
  {
    return VC_ERROR;
  }

  if (m_extraneed)
  {
    hwDec->VideoWrite((uint8_t*)m_hints.extradata, m_hints.extrasize, 0/*pts_ms*/);
    m_extraneed = false;
  }

  if (pData)
  {
    if (pts != DVD_NOPTS_VALUE)
    {
      pts_ms = pts / 1000;
    }

    while (m_opened && !abort)
    {
      if (!hwDec->VideoBufferFull())
      {
        if (m_headersize)
        {
          bool res = hwDec->VideoWriteEx(m_header, m_headersize, pts_ms, false/*continues*/, false/*last*/);
          res |= hwDec->VideoWriteEx(pData, size, pts_ms, r9/*continues*/, r9/*last*/);
          if (res)
          {
            break;
          }
        }
        else
        {
          if (hwDec->VideoWrite(pData, size, pts_ms))
          {
            break;
          }
        }
      }

      Sleep(10);
    }
  }

  if (!hwDec->VideoBufferFull())
  {
    rtn |= VC_BUFFER;
  }

  if (!m_ptsQueue.size())
  {
    m_ready_event.WaitMSec(25);
  }

  if (m_ptsQueue.size() > 0)
  {
    CSingleLock lock(m_ptsQueueMutex);
    //m_cur_pts = m_ptsQueue.front();
    m_ptsQueue.pop_front();
    rtn |= VC_PICTURE;
  }

  return rtn;
}


/* todo */
bool CHisiVdec::GetFramePts(int& pts)
{
  return hwDec->VideoFramePts(pts);
}


/* todo */
bool CHisiVdec::PlayStarted()
{
  return hwDec->VideoPlayStarted();
}


/* todo */
bool CHisiVdec::BufferFull()
{
  return hwDec->VideoBufferFull();
}



/* todo */
bool CHisiVdec::BufferReady()
{
  return hwDec->VideoBufferReady();
}


/* todo */
double CHisiVdec::FirstPts()
{
  return hwDec->VideoFirstPts();
}



/* todo */
double CHisiVdec::GetCurrentPts()
{
  return hwDec->VideoCurrentPts();
}


/* todo */
void CHisiVdec::SetHeader()
{
  unsigned char header[] = {0x00, 0x00, 0x01, 0x0d};

  delete m_header; 
  m_header = NULL;

  if (m_hints.codec == AV_CODEC_ID_VC1)
  {
    m_headersize = 4;
    m_header = (unsigned char*) malloc(m_headersize); //new unsigned char[4];
    memcpy(m_header, header, sizeof(header));
  }
  else
  {
    m_headersize = 0;
  }
}


/* todo */
bool CHisiVdec::GetPicture(DVDVideoPicture* pDvdVideoPicture)
{
  if (!m_opened)
  {
    return false;
  }

  pDvdVideoPicture->dts = DVD_NOPTS_VALUE;
  pDvdVideoPicture->iFlags = DVP_FLAG_ALLOCATED;
  pDvdVideoPicture->iDuration = m_video_rate * 1000000 / 96000.0;
  pDvdVideoPicture->pts = hwDec->VideoCurrentPts();

  return true;
}


/* complete */
void CHisiVdec::SetSpeed(int speed)
{
  if (!m_opened)
  {
    return;
  }

  m_speed = speed;
  hwDec->VideoSetSpeed(m_speed);
  CLog::Log(LOGDEBUG, "CHisiVdec::SetSpeed (%d)", speed);
}


/* complete */
int CHisiVdec::GetDataSize()
{
  CLog::Log(LOGDEBUG, "CHisiVdec::GetDataSize");

  return 0;
}


/* complete */
double CHisiVdec::GetTimeSize()
{
  return 0.0;
}


/* complete */
void CHisiVdec::SetFreeRun(const bool freerun)
{
  /* empty */
}


/* complete */
void CHisiVdec::SubmitEos()
{
  return hwDec->VideoSubmitEOS();
}


/* complete */
bool CHisiVdec::EOS()
{
  return hwDec->IsEOS();
}


/* complete */
void CHisiVdec::SetVideoRect(const CRect &SrcRect, const CRect &DestRect)
{
  /* empty */
}














