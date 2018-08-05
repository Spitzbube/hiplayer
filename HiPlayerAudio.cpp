
#include "HiPlayerAudio.h"


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



