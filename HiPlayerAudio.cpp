
#include "HiPlayerAudio.h"


/* _ZN14CHiPlayerAudioC1EP9CDVDClockR16CDVDMessageQueueR12CProcessInfo */
CHiPlayerAudio::CHiPlayerAudio(CDVDClock *av_clock, CDVDMessageQueue& parent, CProcessInfo &processInfo) 
: CThread("CHiPlayerAudio"), 
  IDVDStreamPlayerAudio(processInfo), 
  m_messageQueue("audio"), 
  m_messageParent(parent),
  m_hisiAudio(new CHiAudio())
{
  m_av_clock      = av_clock;
  m_pAudioCodec   = NULL;
  m_speed         = DVD_PLAYSPEED_NORMAL;
  m_syncState = IDVDStreamPlayer::SYNC_STARTING;
  m_stalled       = false;
  m_audioClock    = DVD_NOPTS_VALUE;
  m_buffer_empty  = false;
  m_DecoderOpen   = false;
  m_bad_state     = false;
  CThread::m_bStop = false;
  m_start_pts     = DVD_NOPTS_VALUE;
  m_bAbortOutput  = false;
  m_hints_current.Clear();
  m_messageQueue.SetMaxTimeSize(8.0);
  m_messageQueue.SetMaxDataSize(6 * 1024 * 1024);
  m_passthrough   = false;
  m_flush         = false;

}


CHiPlayerAudio::~CHiPlayerAudio()
{
}


void CHiPlayerAudio::OnStartup()
{
  printf("CHiPlayerAudio::OnStartup\n");
}


void CHiPlayerAudio::OnExit()
{
  printf("CHiPlayerAudio::OnExit\n");
}


void CHiPlayerAudio::Process()
{
  printf("CHiPlayerAudio::Process\n");
}


bool CHiPlayerAudio::OpenStream(CDVDStreamInfo &hints)
{
  printf("CHiPlayerAudio::OpenStream\n");
}



bool CHiPlayerAudio::HasData() const
{
  printf("CHiPlayerAudio::HasData\n");
}


bool CHiPlayerAudio::IsEOS()
{
  printf("CHiPlayerAudio::IsEOS\n");
}


void CHiPlayerAudio::CloseStream(bool bWaitForBuffers)
{
  printf("CHiPlayerAudio::CloseStream\n");
}


void CHiPlayerAudio::Flush(bool sync)
{
  printf("CHiPlayerAudio::Flush\n");
}


bool CHiPlayerAudio::IsPassthrough() const
{
  printf("CHiPlayerAudio::IsPassthrough\n");
}


void CHiPlayerAudio::SetSpeed(int iSpeed)
{
  printf("CHiPlayerAudio::SetSpeed\n");
}


int  CHiPlayerAudio::GetAudioBitrate()
{
  printf("CHiPlayerAudio::GetAudioBitrate\n");
}


int CHiPlayerAudio::GetAudioChannels()
{
  printf("CHiPlayerAudio::GetAudioChannels\n");
}


std::string CHiPlayerAudio::GetPlayerInfo()
{
  printf("CHiPlayerAudio::GetPlayerInfo\n");
}



