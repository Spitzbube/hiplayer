
#include "utils/log.h"
#include "rendering/RenderSystem.h"
#include "HiPlayerVideo.h"


/* _ZN14CHiPlayerVideoC1EP9CDVDClockP20CDVDOverlayContainerR16CDVDMessageQueueR14CRenderManagerR12CProcessInfo */
CHiPlayerVideo::CHiPlayerVideo(CDVDClock* pClock, CDVDOverlayContainer* pOverlayContainer,
                               CDVDMessageQueue& parent, CRenderManager& renderManager,
                               CProcessInfo &processInfo)
: CThread("CHiPlayerVideo"), 
  IDVDStreamPlayerVideo(processInfo), 
  m_messageQueue("video"),
  m_messageParent(parent),
  m_renderManager(renderManager)
{
#if 0
  m_pOverlayContainer = pOverlayContainer;
  m_speed = DVD_PLAYSPEED_NORMAL;
  m_bRenderSubs = false;
  m_bAllowFullscreen = false;
  m_paused = false;
  m_fFrameRate = 25;
  m_iSubtitleDelay = 0;
  m_stalled = false;
  m_syncState = IDVDStreamPlayer::SYNC_STARTING;
  m_pVideoCodec = 0;
  m_start_pts     = DVD_NOPTS_VALUE;
  m_messageQueue.SetMaxDataSize(40 * 1024 * 1024);
  m_messageQueue.SetMaxTimeSize(8.0);
#else
  m_pOverlayContainer = pOverlayContainer; //1016
  m_speed = DVD_PLAYSPEED_NORMAL; //664
  m_bRenderSubs = false; //668
  m_bAllowFullscreen = false; //669
  m_paused = false; //670
  m_stalled = false; //696
  m_syncState = IDVDStreamPlayer::SYNC_STARTING; //1024
  m_iSubtitleDelay = 0; //688
  m_pVideoCodec = 0; //1028
  m_start_pts     = DVD_NOPTS_VALUE; //1080
  m_messageQueue.SetMaxDataSize(40 * 1024 * 1024);
  m_messageQueue.SetMaxTimeSize(8.0);
  m_fFrameRate            = 25; //680
#endif
}


/* _ZN14CHiPlayerVideoD1Ev */
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


void CHiPlayerVideo::OnStartup()
{
//  printf("CHiPlayerVideo::OnStartup\n");
  CLog::Log(LOGNOTICE, "thread start: video_thread");
}


void CHiPlayerVideo::OnExit()
{
//  printf("CHiPlayerVideo::OnExit\n");
  CLog::Log(LOGNOTICE, "thread end: video_thread");
}


void CHiPlayerVideo::Process()
{
  printf("CHiPlayerVideo::Process\n");
}


void CHiPlayerVideo::OpenStream(CDVDStreamInfo &hint, CDVDVideoCodecHisi* codec)
{
  printf("CHiPlayerVideo::OpenStream(1)\n");
}


bool CHiPlayerVideo::OpenStream(CDVDStreamInfo &hint)
{
  printf("CHiPlayerVideo::OpenStream(2)\n");
}


void CHiPlayerVideo::CloseStream(bool bWaitForBuffers)
{
//  printf("CHiPlayerVideo::CloseStream\n");
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



void CHiPlayerVideo::Flush(bool sync)
{
  printf("CHiPlayerVideo::Flush\n");
}



bool CHiPlayerVideo::HasData() const
{
  printf("CHiPlayerVideo::HasData\n");
}



double CHiPlayerVideo::GetCurrentPts()
{
  printf("CHiPlayerVideo::GetCurrentPts\n");
}



double CHiPlayerVideo::GetOutputDelay()
{
  printf("CHiPlayerVideo::GetOutputDelay\n");
}



std::string CHiPlayerVideo::GetPlayerInfo()
{
  printf("CHiPlayerVideo::GetPlayerInfo\n");
}



int CHiPlayerVideo::GetVideoBitrate()
{
//  printf("CHiPlayerVideo::GetVideoBitrate\n");
  return (int)m_videoStats.GetBitrate();
}



std::string CHiPlayerVideo::GetStereoMode()
{
  printf("CHiPlayerVideo::GetStereoMode\n");
}



void CHiPlayerVideo::SetSpeed(int speed)
{
//  printf("CHiPlayerVideo::SetSpeed\n");
  if(m_messageQueue.IsInited())
    m_messageQueue.Put( new CDVDMsgInt(CDVDMsg::PLAYER_SETSPEED, speed), 1 );
  else
    m_speed = speed;
  CLog::Log(LOGNOTICE, "CHiPlayerVideo SetSpeed: %d", speed);
}






