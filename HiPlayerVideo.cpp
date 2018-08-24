
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "utils/log.h"
#include "rendering/RenderSystem.h"
#include "DVDDemuxers/DVDDemuxUtils.h"
#include "DVDCodecs/DVDCodecUtils.h"
#include "cores/VideoPlayer/VideoRenderers/RenderFlags.h"
#include "cores/VideoPlayer/VideoRenderers/RenderManager.h"
#include "DVDCodecs/DVDCodecs.h"
#include "settings/MediaSettings.h"
#include "settings/DisplaySettings.h"
#include "guilib/GraphicContext.h"
#include "HiDecoder.h"
#include "HiPlayerVideo.h"


class CDVDMsgVideoCodecChangeHi : public CDVDMsg
{
public:
  CDVDMsgVideoCodecChangeHi(const CDVDStreamInfo &hints, CDVDVideoCodecHisi *codec)
    : CDVDMsg(GENERAL_STREAMCHANGE)
    , m_codec(codec)
    , m_hints(hints)
  {}
 ~CDVDMsgVideoCodecChangeHi()
  {
    delete m_codec;
  }
  CDVDVideoCodecHisi *m_codec;
  CDVDStreamInfo      m_hints;
};


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
  s << "vq:"   << std::setw(2) << std::min(99,GetLevel()) << "%";
  s << ", Mb/s:" << std::fixed << std::setprecision(2) << (double)GetVideoBitrate() / (1024.0*1024.0);
  s << ", fr:" << std::fixed << std::setprecision(3) << m_fFrameRate;
  return s.str();
}


/* complete */
void CHiPlayerVideo::OpenStream(CDVDStreamInfo &hint, CDVDVideoCodecHisi* codec)
{
  CLog::Log(LOGDEBUG, "CHiPlayerVideo::OpenStream, code changed");

  m_hints = hint;

  if (hint.fpsrate && hint.fpsscale)
  {
    m_fFrameRate = DVD_TIME_BASE / CDVDCodecUtils::NormalizeFrameduration((double)DVD_TIME_BASE * hint.fpsscale / hint.fpsrate);
    m_bFpsInvalid = false;
    m_processInfo.SetVideoFps(m_fFrameRate);
  }
  else
  {
    m_fFrameRate = 25;
    m_bFpsInvalid = true;
    m_processInfo.SetVideoFps(0); //m_fFrameRate);
  }

  if( m_fFrameRate > 120 || m_fFrameRate < 5 )
  {
    CLog::Log(LOGERROR, "CHiPlayerVideo::OpenStream - Invalid framerate %d, using forced 25fps and just trust timestamps", (int)m_fFrameRate);
    m_fFrameRate = 25;
  }

  // use aspect in stream if available
  if (hint.forced_aspect)
    m_fForcedAspectRatio = hint.aspect;
  else
    m_fForcedAspectRatio = 0.0;

  if (m_pVideoCodec)
  {
    CLog::Log(LOGNOTICE, "CHiPlayerVideo::OpenStream  deleting video codec");
    m_pVideoCodec->ClearPicture(&m_picture);
    if (m_pVideoCodec)
    {
      delete m_pVideoCodec;
    }
    m_pVideoCodec = NULL;
  }

  CLog::Log(LOGDEBUG, "CHiPlayerVideo::OpenStream, assign new codec");

  m_pVideoCodec = codec;

  m_hints = hint;

  m_stalled = m_messageQueue.GetPacketCount(CDVDMsg::DEMUXER_PACKET)? false: true;
  m_rewindStalled = false;
  m_syncState = IDVDStreamPlayer::SYNC_STARTING;
  m_start_pts = DVD_NOPTS_VALUE;
  m_src_rect.SetRect(0, 0, 0, 0);
  m_dst_rect.SetRect(0, 0, 0, 0);
}


void CHiPlayerVideo::SubmitEOS()
{
  m_pVideoCodec->SubmitEOS();
}


/* complete */
void CHiPlayerVideo::ResolutionUpdateCallBack()
{
  uint32_t width = m_hints.width;
  uint32_t height = m_hints.height;

  ERenderFormat format = RENDER_FMT_BYPASS;

  /* figure out steremode expected based on user settings and hints */
  unsigned flags = RenderManager::GetStereoModeFlags(GetStereoMode());

  if(m_bAllowFullscreen)
  {
    flags |= CONF_FLAGS_FULLSCREEN;
    m_bAllowFullscreen = false; // only allow on first configure
  }

  unsigned int iDisplayWidth  = width;
  unsigned int iDisplayHeight = height;

  /* use forced aspect if any */
  if( m_fForcedAspectRatio != 0.0f )
    iDisplayWidth = (int) (iDisplayHeight * m_fForcedAspectRatio);

  m_processInfo.SetVideoFps(m_fFrameRate);
  m_processInfo.SetVideoDimensions(width, height);
  m_processInfo.SetVideoDAR((float)iDisplayWidth / (float)iDisplayHeight);

  DVDVideoPicture picture;
  memset(&picture, 0, sizeof(DVDVideoPicture));

  picture.iWidth = width;
  picture.iHeight = height;
  picture.iDisplayWidth = iDisplayWidth;
  picture.iDisplayHeight = iDisplayHeight;
  picture.format = format;

  if(!m_renderManager.Configure(picture, m_fFrameRate, flags, m_hints.orientation, 3))
  {
    CLog::Log(LOGERROR, "%s - failed to configure renderer", __FUNCTION__);
    return;
  }
}


/* complete */
void CHiPlayerVideo::Process()
{
  CLog::Log(LOGNOTICE, "running thread: video_thread");

  double frametime = (double)DVD_TIME_BASE / m_fFrameRate;
  bool bRequestDrop = false;
  bool settings_changed = false;

  m_rewindStalled = false;

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
      CLog::Log(LOGERROR, "Got MSGQ_ABORT or MSGO_IS_ERROR return true");
      break;
    }
    else if (ret == MSGQ_TIMEOUT)
    {
      continue;
    }

    if (pMsg->IsType(CDVDMsg::GENERAL_SYNCHRONIZE)) //1006
    {
      if(((CDVDMsgGeneralSynchronize*)pMsg)->Wait(100, SYNCSOURCE_VIDEO))
      {
        CLog::Log(LOGDEBUG, "CHiPlayerVideo - CDVDMsg::GENERAL_SYNCHRONIZE");

      }
      else
        m_messageQueue.Put(pMsg->Acquire(), 1); /* push back as prio message, to process other prio messages */
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_RESYNC)) //1001
    {
      double pts = static_cast<CDVDMsgDouble*>(pMsg)->m_value;

      m_syncState = IDVDStreamPlayer::SYNC_INSYNC;

      m_pVideoCodec->ReSync(pts);

      CLog::Log(LOGDEBUG, "CHiPlayerVideo - CDVDMsg::GENERAL_RESYNC(%f)", pts);
    }
    else if (pMsg->IsType(CDVDMsg::VIDEO_SET_ASPECT)) //1028
    {
      CLog::Log(LOGDEBUG, "CHiPlayerVideo - CDVDMsg::VIDEO_SET_ASPECT");
      m_fForcedAspectRatio = *((CDVDMsgDouble*)pMsg);
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_RESET)) //1003
    {
      m_stalled = true;
      m_start_pts = DVD_NOPTS_VALUE;
      m_syncState = IDVDStreamPlayer::SYNC_STARTING;
     
      m_pVideoCodec->Reset();

      CLog::Log(LOGDEBUG, "CHiPlayerVideo - CDVDMsg::GENERAL_RESET");
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_FLUSH)) // private message sent by (COMXPlayerVideo::Flush()) //1002
    {
      bool sync = static_cast<CDVDMsgBool*>(pMsg)->m_value;
      CLog::Log(LOGDEBUG, "CHiPlayerVideo - CDVDMsg::GENERAL_FLUSH(%d)", sync);
      m_stalled = true;
      m_start_pts = DVD_NOPTS_VALUE;
      m_syncState = IDVDStreamPlayer::SYNC_STARTING;
     
      m_pVideoCodec->Reset();
    }
    else if (pMsg->IsType(CDVDMsg::PLAYER_SETSPEED)) //1017
    {
      if (m_speed != static_cast<CDVDMsgInt*>(pMsg)->m_value)
      {
        m_speed = static_cast<CDVDMsgInt*>(pMsg)->m_value;

        m_pVideoCodec->SetSpeed(m_speed);

        CLog::Log(LOGDEBUG, "CHiPlayerVideo - CDVDMsg::PLAYER_SETSPEED %d", m_speed);
      }
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_STREAMCHANGE)) //1005
    {
      CLog::Log(LOGDEBUG, "CHiPlayerVideo - CDVDMsg::GENERAL_STREAMCHANGE");

      CDVDMsgVideoCodecChangeHi* msg(static_cast<CDVDMsgVideoCodecChangeHi*>(pMsg));
      OpenStream(msg->m_hints, msg->m_codec);
      msg->m_codec = NULL;
    }
    else if (pMsg->IsType(CDVDMsg::VIDEO_DRAIN)) //1029
    {
      CLog::Log(LOGDEBUG, "CHiPlayerVideo - CDVDMsg::VIDEO_DRAIN");
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_PAUSE)) //1004
    {
      m_paused = static_cast<CDVDMsgBool*>(pMsg)->m_value;
      CLog::Log(LOGDEBUG, "CHiPlayerVideo - CDVDMsg::GENERAL_PAUSE: %d", m_paused);
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_EOF)) //1008
    {
      CLog::Log(LOGDEBUG, "CHiPlayerVideo - CDVDMsg::GENERAL_EOF");

      m_pVideoCodec->SubmitEOS();
    }
    else if (pMsg->IsType(CDVDMsg::DEMUXER_PACKET)) //1026
    {
      DemuxPacket* pPacket = ((CDVDMsgDemuxerPacket*)pMsg)->GetPacket();
      bool bPacketDrop     = ((CDVDMsgDemuxerPacket*)pMsg)->GetPacketDrop();
   
      if (!bPacketDrop)
      {
        m_bAbortOutput = false;
        int r0 = m_pVideoCodec->Decode(pPacket->pData, pPacket->iSize, pPacket->dts, pPacket->pts, m_bAbortOutput);

        double d16;
        double d8 = pPacket->pts;
        if (d8 == DVD_NOPTS_VALUE)
        {
          d8 = pPacket->dts;
        }

        if (r0 & VC_PICTURE)
        {
          m_pVideoCodec->ClearPicture(&m_picture);
          if (m_pVideoCodec->GetPicture(&m_picture))
          {
            d16 = m_picture.dts;
            if (d16 == DVD_NOPTS_VALUE)
            { 
              d16 = m_picture.pts;
              if (d16 == DVD_NOPTS_VALUE)
              {
                m_picture.pts = d8;
              }
              else
              {
                d8 = d16;
              }
            }
            else
            {
              d8 = m_picture.pts;
              if (d8 == DVD_NOPTS_VALUE)
              {
                d8 = d16;
                m_picture.pts = d16;
              }
            }
          }
        }

        d16 = m_start_pts;
        if ((d16 == DVD_NOPTS_VALUE) && (d8 != DVD_NOPTS_VALUE))
        {
          m_start_pts = d8;
        }

        if (m_stalled)
        {
          if (m_pVideoCodec->PlayStarted())
          {          
            CLog::Log(LOGINFO, "CHiPlayerVideo - Stillframe left, switching to normal playback");
            m_stalled = false;
          }
        }

        ResolutionUpdateCallBack();
      }

      if ((m_syncState == IDVDStreamPlayer::SYNC_STARTING) && (m_start_pts != DVD_NOPTS_VALUE))
      {
        m_processInfo.SetVideoDecoderName(/*m_omxVideo.GetDecoderName()*/"hi_video", true);
        m_syncState = IDVDStreamPlayer::SYNC_WAITSYNC;
        SStartMsg msg;
        msg.player = VideoPlayer_VIDEO;
        msg.cachetime = DVD_MSEC_TO_TIME(50); //! @todo implement
        msg.cachetotal = DVD_MSEC_TO_TIME(100); //! @todo implement
        msg.timestamp = m_start_pts;
        m_messageParent.Put(new CDVDMsgType<SStartMsg>(CDVDMsg::PLAYER_STARTED, msg));

        CLog::Log(LOGINFO, "CCHiPlayerVideo PLAYER_STARTED, pts : %f\n", msg.timestamp);
      }
    }
    pMsg->Release();

  }

  m_pVideoCodec->ClearPicture(&m_picture);
}


/* todo */
void CHiPlayerVideo::SetVideoRect(const CRect &InSrcRect, const CRect &InDestRect)
{
  CRect SrcRect = InSrcRect, DestRect = InDestRect;
  unsigned flags = RenderManager::GetStereoModeFlags(GetStereoMode());
  RENDER_STEREO_MODE video_stereo_mode = (flags & CONF_FLAGS_STEREO_MODE_SBS) ? RENDER_STEREO_MODE_SPLIT_VERTICAL :
                                         (flags & CONF_FLAGS_STEREO_MODE_TAB) ? RENDER_STEREO_MODE_SPLIT_HORIZONTAL : RENDER_STEREO_MODE_OFF;
  bool stereo_invert                   = (flags & CONF_FLAGS_STEREO_CADANCE_RIGHT_LEFT) ? true : false;
  RENDER_STEREO_MODE display_stereo_mode = g_graphicsContext.GetStereoMode();

  // ignore video stereo mode when 3D display mode is disabled
  if (display_stereo_mode == RENDER_STEREO_MODE_OFF)
    video_stereo_mode = RENDER_STEREO_MODE_OFF;

  // fix up transposed video
  if (m_hints.orientation == 90 || m_hints.orientation == 270)
  {
    float newWidth, newHeight;
    float aspectRatio = m_renderManager.GetAspectRatio();
    // clamp width if too wide
    if (DestRect.Height() > DestRect.Width())
    {
      newWidth = DestRect.Width(); // clamp to the width of the old dest rect
      newHeight = newWidth * aspectRatio;
    }
    else // else clamp to height
    {
      newHeight = DestRect.Height(); // clamp to the height of the old dest rect
      newWidth = newHeight / aspectRatio;
    }

    // calculate the center point of the view and offsets
    float centerX = DestRect.x1 + DestRect.Width() * 0.5f;
    float centerY = DestRect.y1 + DestRect.Height() * 0.5f;
    float diffX = newWidth * 0.5f;
    float diffY = newHeight * 0.5f;

    DestRect.x1 = centerX - diffX;
    DestRect.x2 = centerX + diffX;
    DestRect.y1 = centerY - diffY;
    DestRect.y2 = centerY + diffY;
  }

  // check if destination rect or video view mode has changed
  if (!(m_dst_rect != DestRect) && !(m_src_rect != SrcRect) && m_video_stereo_mode == video_stereo_mode && m_display_stereo_mode == display_stereo_mode && m_StereoInvert == stereo_invert)
  {
    return;
  }

  if (m_src_rect != SrcRect)
  {
    CLog::Log(LOGDEBUG, "m_src_rect (%d,%d,%d,%d) SrcRect(%d,%d,%d,%d)",
      (int)m_src_rect.x1, (int)m_src_rect.y1, (int)m_src_rect.x2, (int)m_src_rect.y2,
      (int)SrcRect.x1, (int)SrcRect.y1, (int)SrcRect.x2, (int)SrcRect.y2); 
  }

  if (m_dst_rect != DestRect)
  {
    CLog::Log(LOGDEBUG, "m_dst_rect (%d,%d,%d,%d) DestRect(%d,%d,%d,%d)",
      (int)m_dst_rect.x1, (int)m_dst_rect.y1, (int)m_dst_rect.x2, (int)m_dst_rect.y2,
      (int)DestRect.x1, (int)DestRect.y1, (int)DestRect.x2, (int)DestRect.y2); 
  }

  if (m_video_stereo_mode != video_stereo_mode)
  {
    CLog::Log(LOGDEBUG, "m_video_stereo_mode (%d ) video_stereo_mode(%d )", m_video_stereo_mode, video_stereo_mode);
  }

  if (m_display_stereo_mode != display_stereo_mode)
  {
    CLog::Log(LOGDEBUG, "m_display_stereo_mode (%d ) display_stereo_mode(%d )", m_display_stereo_mode, display_stereo_mode);
  }

  if (m_StereoInvert != stereo_invert)
  {
    CLog::Log(LOGDEBUG, "m_StereoInvert (%d ) stereo_invert(%d )", m_StereoInvert, stereo_invert);
  }

  CLog::Log(LOGDEBUG, "HiPlayerVideo::%s %d,%d,%d,%d -> %d,%d,%d,%d (%d,%d,%d,%d)", __func__,
      (int)SrcRect.x1, (int)SrcRect.y1, (int)SrcRect.x2, (int)SrcRect.y2,
      (int)DestRect.x1, (int)DestRect.y1, (int)DestRect.x2, (int)DestRect.y2,
      video_stereo_mode, display_stereo_mode, CMediaSettings::GetInstance().GetCurrentVideoSettings().m_StereoInvert, g_graphicsContext.GetStereoView());

  m_src_rect = SrcRect;
  m_dst_rect = DestRect;
  m_video_stereo_mode = video_stereo_mode;
  m_display_stereo_mode = display_stereo_mode;
  m_StereoInvert = stereo_invert;

  // might need to scale up m_dst_rect to display size as video decodes
  // to separate video plane that is at display size.
  RESOLUTION res = g_graphicsContext.GetVideoResolution();
  CRect gui(0, 0, CDisplaySettings::GetInstance().GetResolutionInfo(res).iWidth, CDisplaySettings::GetInstance().GetResolutionInfo(res).iHeight);
  CRect display(0, 0, CDisplaySettings::GetInstance().GetResolutionInfo(res).iScreenWidth, CDisplaySettings::GetInstance().GetResolutionInfo(res).iScreenHeight);

  if (display_stereo_mode == RENDER_STEREO_MODE_SPLIT_VERTICAL)
  {
    float width = DestRect.x2 - DestRect.x1;
    DestRect.x1 *= 2.0f;
    DestRect.x2 = DestRect.x1 + 2.0f * width;
  }
  else if (display_stereo_mode == RENDER_STEREO_MODE_SPLIT_HORIZONTAL)
  {
    float height = DestRect.y2 - DestRect.y1;
    DestRect.y1 *= 2.0f;
    DestRect.y2 = DestRect.y1 + 2.0f * height;
  }

  if (gui != display)
  {
    float xscale = display.Width()  / gui.Width();
    float yscale = display.Height() / gui.Height();
    DestRect.x1 *= xscale;
    DestRect.x2 *= xscale;
    DestRect.y1 *= yscale;
    DestRect.y2 *= yscale;
  }
  HisiAvDecoder::GetInstance()->VideoSetRect(DestRect.x1, DestRect.y1, DestRect.x2 - DestRect.x1, DestRect.y2 - DestRect.y1);
}


/* complete */
void CHiPlayerVideo::ProcessOverlays(double pts)
{
  // remove any overlays that are out of time
  if (m_syncState == IDVDStreamPlayer::SYNC_INSYNC)
    m_pOverlayContainer->CleanUp(pts - m_iSubtitleDelay);

  VecOverlays overlays;

  CSingleLock lock(*m_pOverlayContainer);

  VecOverlays* pVecOverlays = m_pOverlayContainer->GetOverlays();
  VecOverlaysIter it = pVecOverlays->begin();

  //Check all overlays and render those that should be rendered, based on time and forced
  //Both forced and subs should check timing
  while (it != pVecOverlays->end())
  {
    CDVDOverlay* pOverlay = *it++;
    if(!pOverlay->bForced && !m_bRenderSubs)
      continue;

    double pts2 = pOverlay->bForced ? pts : pts - m_iSubtitleDelay;

    if((pOverlay->iPTSStartTime <= pts2 && (pOverlay->iPTSStopTime > pts2 || pOverlay->iPTSStopTime == 0LL)))
    {
      if(pOverlay->IsOverlayType(DVDOVERLAY_TYPE_GROUP))
        overlays.insert(overlays.end(), static_cast<CDVDOverlayGroup*>(pOverlay)->m_overlays.begin()
                                      , static_cast<CDVDOverlayGroup*>(pOverlay)->m_overlays.end());
      else
        overlays.push_back(pOverlay);
    }
  }

  for(it = overlays.begin(); it != overlays.end(); ++it)
  {
    double pts2 = (*it)->bForced ? pts : pts - m_iSubtitleDelay;
    m_renderManager.AddOverlay(*it, pts2);
  }
}


/* complete */
void CHiPlayerVideo::Output(double pts, bool bDropPacket)
{
  if (!m_renderManager.IsConfigured()) {
    CLog::Log(LOGINFO, "%s - renderer not configured", __FUNCTION__);
    return;
  }

  if (CThread::m_bStop)
    return;

  CRect SrcRect, DestRect, viewRect;
  m_renderManager.GetVideoRect(SrcRect, DestRect, viewRect);
  SetVideoRect(SrcRect, DestRect);

  m_bAbortOutput = false;
  int buffer = m_renderManager.WaitForBuffer(m_bAbortOutput);
  if (buffer < 0)
    return;

  ProcessOverlays(pts);

  m_renderManager.FlipPage(m_bAbortOutput, pts, EINTERLACEMETHOD::VS_INTERLACEMETHOD_NONE, FS_NONE, m_syncState == IDVDStreamPlayer::SYNC_STARTING);
}


/* complete */
CHiPlayerVideo::CHiPlayerVideo(CDVDClock* pClock, CDVDOverlayContainer* pOverlayContainer,
                               CDVDMessageQueue& parent, CRenderManager& renderManager,
                               CProcessInfo &processInfo)
: CThread("VideoPlayerVideo"), 
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


/* todo */
bool CHiPlayerVideo::OpenStream(CDVDStreamInfo &hint)
{
  CLog::Log(LOGNOTICE, "CHiPlayerVideo::OpenStream");

  m_processInfo.ResetVideoCodecInfo();

  CRenderInfo renderinfo = m_renderManager.GetRenderInfo();

  CDVDVideoCodecHisi * pVideoCodec = new CDVDVideoCodecHisi(m_processInfo);

  CDVDCodecOptions options;
  bool ret = pVideoCodec->Open(hint, options);
  if (ret)
  {
    if (m_messageQueue.IsInited())
    {
      m_messageQueue.Put(new CDVDMsgVideoCodecChangeHi(hint, pVideoCodec), 0);
    }
    else
    {
      OpenStream(hint, pVideoCodec);

      CLog::Log(LOGNOTICE, "Creating video thread");

      m_messageQueue.Init();
      Create();
    }
  }
  else
  {
    CLog::Log(LOGERROR, "Unsupported video codec");
  }

  return ret;
}






