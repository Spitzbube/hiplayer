
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "utils/log.h"
#include "rendering/RenderSystem.h"
#include "cores/VideoPlayer/VideoRenderers/RenderFlags.h"
#include "cores/VideoPlayer/VideoRenderers/RenderManager.h"
#include "settings/MediaSettings.h"
#include "HiPlayerVideo.h"


/* complete */
int CHiPlayerVideo::GetVideoBitrate()
{
  return (int)m_videoStats.GetBitrate();
}


/* complete */
void CHiPlayerVideo::OnStartup()
{
  CLog::Log(LOGNOTICE, "thread start: video_thread");
}


/* complete */
void CHiPlayerVideo::OnExit()
{
  CLog::Log(LOGNOTICE, "thread end: video_thread");
}


/* complete */
void CHiPlayerVideo::SetSpeed(int speed)
{
  if(m_messageQueue.IsInited())
    m_messageQueue.Put( new CDVDMsgInt(CDVDMsg::PLAYER_SETSPEED, speed), 1 );
  else
    m_speed = speed;
  CLog::Log(LOGNOTICE, "CHiPlayerVideo SetSpeed: %d", speed);
}


/* complete */
void CHiPlayerVideo::CloseStream(bool bWaitForBuffers)
{
  CLog::Log(LOGNOTICE, "CHiPlayerVideo::CloseStream");

  // wait until buffers are empty
  if (bWaitForBuffers && m_speed > 0)
  {
    m_messageQueue.Put(new CDVDMsg(CDVDMsg::VIDEO_DRAIN), 0);
    m_messageQueue.WaitUntilEmpty();
  }

  m_messageQueue.Abort();

  // wait for decode_video thread to end
  CLog::Log(LOGNOTICE, "waiting for video thread to exit");

  m_bAbortOutput = true;
  StopThread();

  m_messageQueue.End();

  if (m_pVideoCodec)
  {
    m_pVideoCodec->ClearPicture(&m_picture);
    delete m_pVideoCodec;
    m_pVideoCodec = NULL;
  }
  CLog::Log(LOGNOTICE, "CHiPlayerVideo::CloseStream  success");
}


/* complete */
void CHiPlayerVideo::Flush(bool sync)
{
  CLog::Log(LOGNOTICE, "CHiPlayerVideo Flush");

  m_messageQueue.Flush();
  m_messageQueue.Put(new CDVDMsgBool(CDVDMsg::GENERAL_FLUSH, sync), 1);
  m_bAbortOutput = true;
}


/* complete */
double CHiPlayerVideo::GetOutputDelay()
{
  double time = m_messageQueue.GetPacketCount(CDVDMsg::DEMUXER_PACKET);
  if( m_fFrameRate )
    time = (time * DVD_TIME_BASE) / m_fFrameRate;
  else
    time = 0.0;

  if( m_speed != 0 )
    time = time * DVD_PLAYSPEED_NORMAL / abs(m_speed);

  printf("CHiPlayerVideo GetOutputDelay %f\n", time);

  return time;
}


/* complete */
double CHiPlayerVideo::GetCurrentPts()
{
  CLog::Log(LOGNOTICE, "GetCurrentPts");

  if (m_stalled)
  {
    return DVD_NOPTS_VALUE;
  }
  
  return m_pVideoCodec->GetCurrentPts();
}


/* complete */
bool CHiPlayerVideo::HasData() const
{
  if (m_messageQueue.GetDataSize() > 0)
  {
    return true;
  }
  else
  {
    return !m_pVideoCodec->IsEOS();
  }
}


/* complete */
std::string CHiPlayerVideo::GetStereoMode()
{
  std::string  stereo_mode;

  switch(CMediaSettings::GetInstance().GetCurrentVideoSettings().m_StereoMode)
  {
    case RENDER_STEREO_MODE_SPLIT_VERTICAL:   stereo_mode = "left_right"; break;
    case RENDER_STEREO_MODE_SPLIT_HORIZONTAL: stereo_mode = "top_bottom"; break;
    default:                                  stereo_mode = m_hints.stereo_mode; break;
  }

  if(CMediaSettings::GetInstance().GetCurrentVideoSettings().m_StereoInvert)
    stereo_mode = RenderManager::GetStereoModeInvert(stereo_mode);
  return stereo_mode;
}


/* todo */
std::string CHiPlayerVideo::GetPlayerInfo()
{
  std::ostringstream s;
  s << ", vq:"   << std::setw(2) << std::min(99,GetLevel()) << "%";
  s << ", Mb/s:" << std::fixed << std::setprecision(2) << (double)GetVideoBitrate() / (1024.0*1024.0);
  s << ", fr:" << std::fixed << std::setprecision(3) << m_fFrameRate;
  return s.str();
}


/* todo */
void CHiPlayerVideo::OpenStream(CDVDStreamInfo &hint, CDVDVideoCodecHisi* codec)
{
  printf("CHiPlayerVideo::OpenStream(1)\n");
}


void CHiPlayerVideo::SubmitEOS()
{
  m_pVideoCodec->SubmitEOS();
}


/* todo */
void CHiPlayerVideo::ResolutionUpdateCallBack()
{
}


/* todo */
void CHiPlayerVideo::Process()
{
  CLog::Log(LOGNOTICE, "running thread: video_thread");

  m_rewindStalled = false;

  double frametime = (double)DVD_TIME_BASE / m_fFrameRate;
  bool bRequestDrop = false;
  bool settings_changed = false;

  m_videoStats.Start();

  while(!m_bStop)
  {
    int iQueueTimeOut = (int)(m_stalled ? frametime / 4 : frametime * 10) / 1000;
    int iPriority = (m_speed == DVD_PLAYSPEED_PAUSE && m_syncState == IDVDStreamPlayer::SYNC_INSYNC) ? 1 : 0;

    if (m_syncState == IDVDStreamPlayer::SYNC_WAITSYNC)
      iPriority = 1;

    CDVDMsg* pMsg;
    MsgQueueReturnCode ret = m_messageQueue.Get(&pMsg, iQueueTimeOut, iPriority);

    if (MSGQ_IS_ERROR(ret) || ret == MSGQ_ABORT)
    {
      CLog::Log(LOGERROR, "Got MSGQ_ABORT or MSGQ_IS_ERROR return true");
      break;
    }
    else if (ret == MSGQ_TIMEOUT)
    {
      continue;
    }

    if (pMsg->IsType(CDVDMsg::GENERAL_SYNCHRONIZE))
    {
      if(((CDVDMsgGeneralSynchronize*)pMsg)->Wait(100, SYNCSOURCE_VIDEO))
      {
        CLog::Log(LOGDEBUG, "COMXPlayerVideo - CDVDMsg::GENERAL_SYNCHRONIZE");

      }
      else
        m_messageQueue.Put(pMsg->Acquire(), 1); /* push back as prio message, to process other prio messages */
    }
#if 0
    else if (pMsg->IsType(CDVDMsg::GENERAL_RESYNC))
    {
      double pts = static_cast<CDVDMsgDouble*>(pMsg)->m_value;

      m_nextOverlay = DVD_NOPTS_VALUE;
      m_iCurrentPts = DVD_NOPTS_VALUE;
      m_syncState = IDVDStreamPlayer::SYNC_INSYNC;

      CLog::Log(LOGDEBUG, "CVideoPlayerVideo - CDVDMsg::GENERAL_RESYNC(%f)", pts);
    }
    else if (pMsg->IsType(CDVDMsg::VIDEO_SET_ASPECT))
    {
      m_fForcedAspectRatio = *((CDVDMsgDouble*)pMsg);
      CLog::Log(LOGDEBUG, "COMXPlayerVideo - CDVDMsg::VIDEO_SET_ASPECT %.2f", m_fForcedAspectRatio);
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_RESET))
    {
      CLog::Log(LOGDEBUG, "COMXPlayerVideo - CDVDMsg::GENERAL_RESET");
      m_syncState = IDVDStreamPlayer::SYNC_STARTING;
      m_nextOverlay = DVD_NOPTS_VALUE;
      m_iCurrentPts = DVD_NOPTS_VALUE;
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_FLUSH)) // private message sent by (COMXPlayerVideo::Flush())
    {
      bool sync = static_cast<CDVDMsgBool*>(pMsg)->m_value;
      CLog::Log(LOGDEBUG, "COMXPlayerVideo - CDVDMsg::GENERAL_FLUSH(%d)", sync);
      m_stalled = true;
      m_syncState = IDVDStreamPlayer::SYNC_STARTING;
      m_nextOverlay = DVD_NOPTS_VALUE;
      m_iCurrentPts = DVD_NOPTS_VALUE;
      m_omxVideo.Reset();
      m_flush = false;
    }
    else if (pMsg->IsType(CDVDMsg::PLAYER_SETSPEED))
    {
      if (m_speed != static_cast<CDVDMsgInt*>(pMsg)->m_value)
      {
        m_speed = static_cast<CDVDMsgInt*>(pMsg)->m_value;
        CLog::Log(LOGDEBUG, "COMXPlayerVideo - CDVDMsg::PLAYER_SETSPEED %d", m_speed);
      }
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_STREAMCHANGE))
    {
      COMXMsgVideoCodecChange* msg(static_cast<COMXMsgVideoCodecChange*>(pMsg));
      OpenStream(msg->m_hints, msg->m_codec);
      msg->m_codec = NULL;
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_EOF))
    {
      CLog::Log(LOGDEBUG, "COMXPlayerVideo - CDVDMsg::GENERAL_EOF");
      SubmitEOS();
    }
    else if (pMsg->IsType(CDVDMsg::DEMUXER_PACKET))
    {
      DemuxPacket* pPacket = ((CDVDMsgDemuxerPacket*)pMsg)->GetPacket();
      bool bPacketDrop     = ((CDVDMsgDemuxerPacket*)pMsg)->GetPacketDrop();

      #ifdef _DEBUG
      CLog::Log(LOGINFO, "Video: dts:%.0f pts:%.0f size:%d (s:%d f:%d d:%d l:%d) s:%d %d/%d late:%d\n", pPacket->dts, pPacket->pts, 
          (int)pPacket->iSize, m_syncState, m_flush, bPacketDrop, m_stalled, m_speed, 0, 0, 0);
      #endif
      if (m_messageQueue.GetDataSize() == 0
      ||  m_speed < 0)
      {
        bRequestDrop = false;
      }

      // if player want's us to drop this packet, do so nomatter what
      if(bPacketDrop)
        bRequestDrop = true;

      m_omxVideo.SetDropState(bRequestDrop);

      while (!m_bStop)
      {
        // discard if flushing as clocks may be stopped and we'll never submit it
        if (m_flush)
           break;

        if((int)m_omxVideo.GetFreeSpace() < pPacket->iSize)
        {
          Sleep(10);
          continue;
        }
  
        if (m_stalled)
        {
          if(m_syncState == IDVDStreamPlayer::SYNC_INSYNC)
            CLog::Log(LOGINFO, "COMXPlayerVideo - Stillframe left, switching to normal playback");
          m_stalled = false;
        }

        double dts = pPacket->dts;
        double pts = pPacket->pts;
        double iVideoDelay = m_renderManager.GetDelay() * (DVD_TIME_BASE / 1000.0);

        if (dts != DVD_NOPTS_VALUE)
          dts += iVideoDelay;

        if (pts != DVD_NOPTS_VALUE)
          pts += iVideoDelay;

        m_omxVideo.Decode(pPacket->pData, pPacket->iSize, dts, m_hints.ptsinvalid ? DVD_NOPTS_VALUE : pts, settings_changed);

        if (pts == DVD_NOPTS_VALUE)
          pts = dts;

        Output(pts, bRequestDrop);
        if(pts != DVD_NOPTS_VALUE)
          m_iCurrentPts = pts;

        if (m_syncState == IDVDStreamPlayer::SYNC_STARTING && !bRequestDrop && settings_changed)
        {
          m_processInfo.SetVideoDecoderName(m_omxVideo.GetDecoderName(), true);
          m_syncState = IDVDStreamPlayer::SYNC_WAITSYNC;
          SStartMsg msg;
          msg.player = VideoPlayer_VIDEO;
          msg.cachetime = DVD_MSEC_TO_TIME(50); //! @todo implement
          msg.cachetotal = DVD_MSEC_TO_TIME(100); //! @todo implement
          msg.timestamp = pts;
          m_messageParent.Put(new CDVDMsgType<SStartMsg>(CDVDMsg::PLAYER_STARTED, msg));
        }

        break;
      }

      bRequestDrop = false;

      m_videoStats.AddSampleBytes(pPacket->iSize);
    }
#endif
    pMsg->Release();

  }
  //1244
  m_pVideoCodec->ClearPicture(&m_picture);
}


/* todo */
void CHiPlayerVideo::SetVideoRect(const CRect &InSrcRect, const CRect &InDestRect)
{
}


/* todo */
void CHiPlayerVideo::ProcessOverlays(double pts)
{
}


/* todo */
void CHiPlayerVideo::Output(double pts, bool bDropPacket)
{
}



/* complete */
CHiPlayerVideo::CHiPlayerVideo(CDVDClock* pClock, CDVDOverlayContainer* pOverlayContainer,
                               CDVDMessageQueue& parent, CRenderManager& renderManager,
                               CProcessInfo &processInfo)
: CThread("CHiPlayerVideo"), 
  IDVDStreamPlayerVideo(processInfo), 
  m_messageQueue("video"),
  m_messageParent(parent),
  m_renderManager(renderManager)
{
  m_pOverlayContainer = pOverlayContainer;
  m_speed = DVD_PLAYSPEED_NORMAL;
  m_bRenderSubs = false;
  m_bAllowFullscreen = false;
  m_paused = false;
  m_fFrameRate = 25;
  m_iSubtitleDelay = 0;
  m_stalled = false;
  m_syncState = IDVDStreamPlayer::SYNC_STARTING;
  m_pVideoCodec = NULL;
  m_start_pts     = DVD_NOPTS_VALUE;
  m_messageQueue.SetMaxDataSize(40 * 1024 * 1024);
  m_messageQueue.SetMaxTimeSize(8.0);
}


/* complete */
CHiPlayerVideo::~CHiPlayerVideo()
{
  m_bAbortOutput = true;
  StopThread();
  if (m_pVideoCodec)
  {
    delete m_pVideoCodec;
  }
  m_pVideoCodec = NULL;
}


bool CHiPlayerVideo::OpenStream(CDVDStreamInfo &hint)
{
  printf("CHiPlayerVideo::OpenStream(2)\n");
}






