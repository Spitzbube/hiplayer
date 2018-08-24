

#include <stdio.h>
#include <unistd.h>
#include <iomanip>
#include <algorithm>
#include <iostream>
#include <sstream>

#include "utils/MathUtils.h"
#include "utils/log.h"
#include "DVDDemuxers/DVDDemuxUtils.h"
#include "cores/AudioEngine/AEFactory.h"
#include "cores/DataCacheCore.h"
#include "ServiceBroker.h"
#include "HiPlayerAudio.h"


class COMXMsgAudioCodecChange : public CDVDMsg
{
public:
  COMXMsgAudioCodecChange(const CDVDStreamInfo &hints, CDVDAudioCodecHisi* codec)
    : CDVDMsg(GENERAL_STREAMCHANGE)
    , m_codec(codec)
    , m_hints(hints)
  {}
 ~COMXMsgAudioCodecChange()
  {
    delete m_codec;
  }
  CDVDAudioCodecHisi   *m_codec;
  CDVDStreamInfo      m_hints;
};


/* complete */
void CHiPlayerAudio::OnStartup()
{
  /* empty */
}


/* complete */
bool CHiPlayerAudio::IsPassthrough() const
{
  return m_passthrough;
}


/* complete */
int  CHiPlayerAudio::GetAudioBitrate()
{
  return m_audioStats.GetBitrate();
}


/* complete */
int CHiPlayerAudio::GetAudioChannels()
{
  return m_hints.channels;
}


/* complete */
void CHiPlayerAudio::OnExit()
{
  CLog::Log(LOGNOTICE, "thread end: CHiPlayerAudio::OnExit()");
}


/* complete */
void CHiPlayerAudio::SetSpeed(int iSpeed)
{
  if(m_messageQueue.IsInited())
  {
    m_messageQueue.Put( new CDVDMsgInt(CDVDMsg::PLAYER_SETSPEED, iSpeed), 1 );
  }
  else
  {
    m_speed = iSpeed;
  }

  CLog::Log(LOGINFO, "CHiPlayerAudio SetSpeed: %d", iSpeed);
}


/* complete */
void CHiPlayerAudio::Flush(bool sync)
{
  CLog::Log(LOGINFO, "CHiPlayerAudio - Flush, sync: %d", sync);

  m_flush = sync; //true;
  m_messageQueue.Flush();
//  m_messageQueue.Flush(CDVDMsg::GENERAL_EOF);
  m_messageQueue.Put( new CDVDMsgBool(CDVDMsg::GENERAL_FLUSH, sync), 1);

  m_bAbortOutput = true;
}


/* complete */
void CHiPlayerAudio::CloseStream(bool bWaitForBuffers)
{
  // wait until buffers are empty
  if (bWaitForBuffers && m_speed > 0) m_messageQueue.WaitUntilEmpty();

  m_messageQueue.Abort();

  m_speed = DVD_PLAYSPEED_NORMAL;
  m_bStop = true;
  m_bAbortOutput = true;

  if(IsRunning())
    StopThread();

  m_messageQueue.End();

  if (m_pAudioCodec)
  {
    m_pAudioCodec->Dispose();
    delete m_pAudioCodec;
    m_pAudioCodec = NULL;
  }

  m_hisiAudio->close();

  m_DecoderOpen = false;
}


/* complete */
bool CHiPlayerAudio::IsEOS()
{
  if (m_bad_state)
    return true;

  return m_hisiAudio->EOS();
}


/* complete */
bool CHiPlayerAudio::HasData() const
{
  if (m_messageQueue.GetDataSize() > 0)
  {
    return true;
  }
  else
  {
    return !m_hisiAudio->EOS();
  }
}


/* todo */
std::string CHiPlayerAudio::GetPlayerInfo()
{
  std::ostringstream s;
  s << "aq:"     << std::setw(2) << std::min(99,m_messageQueue.GetLevel() /*+ MathUtils::round_int(100.0/8.0*GetCacheTime())*/) << "%";
  s << ", Kb/s:" << std::fixed << std::setprecision(2) << (double)GetAudioBitrate() / 1024.0;

  return s.str();
}


/* complete */
bool CHiPlayerAudio::CodecChange()
{
  unsigned int old_bitrate = m_hints.bitrate;
  unsigned int new_bitrate = m_hints_current.bitrate;

  if(m_pAudioCodec)
  {
    m_hints.channels = m_pAudioCodec->GetChannels();
    m_hints.samplerate = m_pAudioCodec->GetSampleRate();
    m_hints.bitspersample = m_pAudioCodec->GetBitsPerSample();
  }

  /* only check bitrate changes on AV_CODEC_ID_DTS, AV_CODEC_ID_AC3, AV_CODEC_ID_EAC3 */
  if(m_hints.codec != AV_CODEC_ID_DTS && m_hints.codec != AV_CODEC_ID_AC3 && m_hints.codec != AV_CODEC_ID_EAC3)
    new_bitrate = old_bitrate = 0;

  // for passthrough we only care about the codec and the samplerate
  bool minor_change = m_hints_current.channels       != m_hints.channels ||
                      m_hints_current.bitspersample  != m_hints.bitspersample ||
                      old_bitrate                    != new_bitrate;

  if(m_hints_current.codec          != m_hints.codec ||
     m_hints_current.samplerate     != m_hints.samplerate ||
     (!m_passthrough && minor_change) || !m_DecoderOpen)
  {
    CLog::Log(LOGNOTICE, "CHiPlayerAudio, CodecChanged");

    m_hints_current = m_hints;

    m_processInfo.SetAudioSampleRate(m_hints.samplerate);
    m_processInfo.SetAudioBitsPerSample(m_hints.bitspersample);

    CServiceBroker::GetDataCacheCore().SignalAudioInfoChange();
    return true;
  }

  return false;
}


/* todo */
void CHiPlayerAudio::WaitForBuffers()
{
  // make sure there are no more packets available
  m_messageQueue.WaitUntilEmpty();

  // make sure almost all has been rendered
  // leave 500ms to avound buffer underruns
  double delay = m_hisiAudio->CacheTime();
  if(delay > 0.5)
    Sleep((int)(1000 * (delay - 0.5)));
}


/* complete */
AEAudioFormat CHiPlayerAudio::GetDataFormat(CDVDStreamInfo hints)
{
  AEAudioFormat format;
  format.m_dataFormat = AE_FMT_RAW;
  format.m_sampleRate = hints.samplerate;
  switch (hints.codec)
  {
    case AV_CODEC_ID_AC3:
      format.m_streamInfo.m_type = CAEStreamInfo::STREAM_TYPE_AC3;
      format.m_streamInfo.m_sampleRate = hints.samplerate;
      break;

    case AV_CODEC_ID_EAC3:
      format.m_streamInfo.m_type = CAEStreamInfo::STREAM_TYPE_EAC3;
      format.m_streamInfo.m_sampleRate = hints.samplerate * 4;
      break;

    case AV_CODEC_ID_DTS:
      format.m_streamInfo.m_type = CAEStreamInfo::STREAM_TYPE_DTSHD;
      format.m_streamInfo.m_sampleRate = hints.samplerate;
      break;

    case AV_CODEC_ID_TRUEHD:
      format.m_streamInfo.m_type = CAEStreamInfo::STREAM_TYPE_TRUEHD;
      format.m_streamInfo.m_sampleRate = hints.samplerate;
      break;

    default:
      format.m_streamInfo.m_type = CAEStreamInfo::STREAM_TYPE_NULL;
  }

  m_passthrough = CAEFactory::SupportsRaw(format);

  if (!m_passthrough && hints.codec == AV_CODEC_ID_DTS)
  {
    format.m_streamInfo.m_type = CAEStreamInfo::STREAM_TYPE_DTSHD_CORE;
    m_passthrough = CAEFactory::SupportsRaw(format);
  }

  if(!m_passthrough)
  {
    if (m_pAudioCodec && m_pAudioCodec->GetBitsPerSample() == 16)
      format.m_dataFormat = AE_FMT_S16NE;
    else
      format.m_dataFormat = AE_FMT_FLOAT;
  }

  return format;
}


/* todo */
void CHiPlayerAudio::OpenStream(CDVDStreamInfo &hints, CDVDAudioCodecHisi *codec)
{
  SAFE_DELETE(m_pAudioCodec);

  if (m_hints.bitspersample == 0)
    m_hints.bitspersample = 16;

  m_pAudioCodec = codec;
  m_hints = hints;

  CThread::m_bStop = false;
  m_speed = DVD_PLAYSPEED_NORMAL;
  m_audioClock      = DVD_NOPTS_VALUE;
  m_start_pts = DVD_NOPTS_VALUE;
  m_syncState = IDVDStreamPlayer::SYNC_STARTING;
  m_flush           = false;
  m_bAbortOutput    = false;
  m_stalled         = m_messageQueue.GetPacketCount(CDVDMsg::DEMUXER_PACKET) == 0;
  m_format = GetDataFormat(m_hints);
  m_format.m_sampleRate    = 0;
  m_format.m_channelLayout = 0;

  CServiceBroker::GetDataCacheCore().SignalAudioInfoChange();

  m_messageParent.Put(new CDVDMsg(CDVDMsg::PLAYER_AVCHANGE));
}


/* todo */
bool CHiPlayerAudio::OpenDecoder()
{
  m_passthrough = false;

  if(m_DecoderOpen)
  {
    m_hisiAudio->close();
    m_DecoderOpen = false;
  }

  /* setup audi format for audio render */
  m_format = GetDataFormat(m_hints);

  CAEChannelInfo channelMap;
  if (m_pAudioCodec && !m_passthrough)
  {
    channelMap = m_pAudioCodec->GetChannelMap();
  }
  else if (m_passthrough)
  {
    // we just want to get the channel count right to stop OMXAudio.cpp rejecting stream
    // the actual layout is not used
    channelMap = AE_CH_LAYOUT_5_1;

    if (m_hints.codec == AV_CODEC_ID_AC3)
      m_processInfo.SetAudioDecoderName("PT_AC3");
    else if (m_hints.codec == AV_CODEC_ID_EAC3)
      m_processInfo.SetAudioDecoderName("PT_EAC3");
    else
      m_processInfo.SetAudioDecoderName("PT_DTS");
  }
  m_processInfo.SetAudioChannels(channelMap);

  m_hints.software = true;
  bool bAudioRenderOpen = m_hisiAudio->open(m_hints);

  m_codec_name = "";
  m_bad_state  = !bAudioRenderOpen;
  
  if(!bAudioRenderOpen)
  {
    CLog::Log(LOGERROR, "OMXPlayerAudio : Error open audio output");
    m_hisiAudio->close();
  }
  else
  {
    CLog::Log(LOGINFO, "Audio codec %s channels %d samplerate %d bitspersample %d\n",
      m_codec_name.c_str(), m_hints.channels, m_hints.samplerate, m_hints.bitspersample);
  }

  return bAudioRenderOpen;
}


/* todo */
bool CHiPlayerAudio::Decode(DemuxPacket *pkt, bool bDropPacket, bool bTrickPlay)
{
  if(!pkt || m_bad_state || !m_pAudioCodec)
    return false;

  bool settings_changed = false;
  const uint8_t *data_dec = pkt->pData; //r9
  int            data_len = pkt->iSize; //r7
  double dts = pkt->dts; //sp8
  double pts = pkt->pts; //sp16

  if(CodecChange())
  {
    m_DecoderOpen = OpenDecoder();
    if(!m_DecoderOpen)
      return false;
  }

  m_bAbortOutput = false;

  if (bTrickPlay)
  {
    settings_changed = true;
    //->13c8
  }
  else if(m_format.m_dataFormat != AE_FMT_RAW && !bDropPacket)
  {
    //122c
    while(!m_bStop && data_len > 0 && !m_bAbortOutput)
    {
      //1274
      int len = m_pAudioCodec->Decode((BYTE *)data_dec, data_len, dts, pts);
      if( (len < 0) || (len >  data_len) )
      {
        m_pAudioCodec->Reset();
        break;
        //->13c8
      }
      //12a0
      data_dec += len;
      data_len -= len;
      
      uint8_t *decoded;
      int decoded_size = m_pAudioCodec->GetData(&decoded, dts, pts);

      if(decoded_size <=0)
        continue;

      //int ret = 0;

      m_audioStats.AddSampleBytes(decoded_size);
      //12d4
      if (pts != DVD_NOPTS_VALUE)
      {
        if ((pts < m_audioClock) && (m_audioClock != DVD_NOPTS_VALUE))
        {
          //14c4?
          CLog::Log(LOGDEBUG, "CHiPlayerAudio package need drop, seek pts:%f, package pts:%f", m_audioClock, pts);
        
          if ((m_start_pts == DVD_NOPTS_VALUE) && (pts != DVD_NOPTS_VALUE))
          {
            //1328
            m_start_pts = pts;
          }          
        }
        else
        {
          //1314
          if (m_start_pts == DVD_NOPTS_VALUE)
          {
            //1328
            m_start_pts = pts;
          }
        }
      }
      //132c
      while(!m_bStop && !m_bAbortOutput)
      {
        if (!m_hisiAudio->Full())
        {
          if (m_hisiAudio->Push(decoded, decoded_size, pts / 1000.0))
          {
            //->14b4
            m_audioClock = pts;
            //->1238
            break;
          }
        }
        //138c
        Sleep(10);
        //->132c
      }
      //->1238
    }
    //->13c8
  }
  else if(!bDropPacket)
  {
    //13a4
    while(!m_bStop)
    {
      //1510
      if (m_bAbortOutput || m_flush)
      {
        //->13b8
        break;
      }
      //1530
      uint8_t *r9/*pData*/ = pkt->pData; //r9
      int      r7/*iSize*/ = pkt->iSize; //r7

      if (r9)
      {
        //1540
        AVPacket avpkt;
        av_init_packet(&avpkt);
        avpkt.data = r9;
        avpkt.size = r7;
        int didSplit = av_packet_split_side_data(&avpkt);
        if (didSplit)
        {
          //167c
          r9 = avpkt.data;
          r7 = avpkt.size;
          av_packet_free_side_data(&avpkt);
        }
      }
      //1564
#if 1
      uint8_t bla[] = {0xff, 0xf1, 0x50, 0x80, 0x00, 0x1f, 0xfc};
      uint8_t* r6 = (uint8_t*) malloc(7); 
      memcpy(r6, bla, 7);
#else
      struct
      {
        char bData_0;
        char bData_1;
        char bData_2;
        char bData_3_bit8: 1;
        unsigned bData_3_rest: 18;
        unsigned rest: 12;
      } bla = {0xff, 0xf1, 0x50, 1, r7 + 7, 0x1ffc};
      
//      bla.bData_3_rest = r7 + 7;

      //uint8_t* r6 = (char*) malloc(7);
      //r6 = bla; 
#endif

      //->1664
      while (!m_bAbortOutput)
      {
        //15e8
        if (!m_hisiAudio->Full())
        {
          //1600
          if ((m_hisiAudio->PushEx((uint8_t*)r6, 7, pkt->pts / 1000.0, /*continues*/false, /*last*/false)) &&
            (m_hisiAudio->PushEx(r9, r7, pkt->pts / 1000.0, /*continues*/true, /*last*/true)))
          {
            //->13b8
            break;
          }
        }
        //1658
        Sleep(10);
      }
      //->13b8
      break;
    }
    //13b8
    m_audioStats.AddSampleBytes(pkt->iSize);
  }
  //13c8
  if (m_syncState == IDVDStreamPlayer::SYNC_STARTING && !bDropPacket && (m_start_pts != DVD_NOPTS_VALUE))
  {
    //->1418
    SStartMsg msg;
    msg.player = VideoPlayer_AUDIO;
    msg.cachetotal = DVD_SEC_TO_TIME(m_hisiAudio->CacheTotal());
    msg.cachetime = DVD_SEC_TO_TIME(m_hisiAudio->CacheTime());
    msg.timestamp = m_start_pts; //m_audioClock;
    m_messageParent.Put(new CDVDMsgType<SStartMsg>(CDVDMsg::PLAYER_STARTED, msg));

    CLog::Log(LOGINFO, "CHiPlayerAudio PLAYER_STARTED, pts : %f\n", m_start_pts);

    m_syncState = IDVDStreamPlayer::SYNC_WAITSYNC;
  }
  //13f4
  return true;
}



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


void CHiPlayerAudio::Process()
{
  m_audioStats.Start();

  while(!m_bStop)
  {
    CDVDMsg* pMsg;
    int timeout = 1000;

    // read next packet and return -1 on error
    int priority = 1;
    //Do we want a new audio frame?
    if (m_syncState == IDVDStreamPlayer::SYNC_STARTING ||              /* when not started */
        m_speed == DVD_PLAYSPEED_NORMAL || /* when playing normally */
        m_speed <  DVD_PLAYSPEED_PAUSE  || /* when rewinding */
       (m_speed >  DVD_PLAYSPEED_NORMAL && m_audioClock < m_av_clock->GetClock())) /* when behind clock in ff */
      priority = 0;

    if (m_syncState == IDVDStreamPlayer::SYNC_WAITSYNC)
      priority = 1;

    // consider stream stalled if queue is empty
    // we can't sync audio to clock with an empty queue
    if (m_speed == DVD_PLAYSPEED_NORMAL)
    {
      timeout = 0;
    }

    MsgQueueReturnCode ret = m_messageQueue.Get(&pMsg, timeout, priority);

    if (ret == MSGQ_TIMEOUT)
    {
      Sleep(10);
      continue;
    }

    if (MSGQ_IS_ERROR(ret) || ret == MSGQ_ABORT)
    {
      Sleep(10);
      continue;
    }

    if (pMsg->IsType(CDVDMsg::DEMUXER_PACKET)) //1026
    {
      DemuxPacket* pPacket = ((CDVDMsgDemuxerPacket*)pMsg)->GetPacket();
      bool bPacketDrop     = ((CDVDMsgDemuxerPacket*)pMsg)->GetPacketDrop();

      #ifdef _DEBUG
      CLog::Log(LOGINFO, "Audio: dts:%.0f pts:%.0f size:%d (s:%d f:%d d:%d l:%d) s:%d %d/%d late:%d,%d", pPacket->dts, pPacket->pts,
           (int)pPacket->iSize, m_syncState, m_flush, bPacketDrop, m_stalled, m_speed, 0, 0, (int)m_omxAudio.GetAudioRenderingLatency(), (int)m_hints_current.samplerate);
      #endif
      if(Decode(pPacket, bPacketDrop, m_speed > DVD_PLAYSPEED_NORMAL || m_speed < 0))
      {
        // we are not running until something is cached in output device
        if(m_stalled && m_hisiAudio->CacheTime() > 0.0)
        {
          CLog::Log(LOGINFO, "COMXPlayerAudio - Switching to normal playback");
          m_stalled = false;
        }
      }
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_SYNCHRONIZE)) //1006
    {
      if(((CDVDMsgGeneralSynchronize*)pMsg)->Wait( 100, SYNCSOURCE_AUDIO ))
        CLog::Log(LOGDEBUG, "COMXPlayerAudio - CDVDMsg::GENERAL_SYNCHRONIZE");
      else
        m_messageQueue.Put(pMsg->Acquire(), 1); /* push back as prio message, to process other prio messages */
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_RESYNC)) //1001
    { //player asked us to set internal clock
      double pts = static_cast<CDVDMsgDouble*>(pMsg)->m_value;
      CLog::Log(LOGDEBUG, "COMXPlayerAudio - CDVDMsg::GENERAL_RESYNC(%f)", pts);

      m_start_pts = DVD_NOPTS_VALUE; //0;
      m_audioClock = pts;
      m_syncState = IDVDStreamPlayer::SYNC_INSYNC;

      m_hisiAudio->reSync(pts / 1000.0);
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_RESET)) //1003
    {
      CLog::Log(LOGDEBUG, "COMXPlayerAudio - CDVDMsg::GENERAL_RESET");
      m_syncState = IDVDStreamPlayer::SYNC_STARTING;
      m_audioClock = 0; //DVD_NOPTS_VALUE;
      m_pAudioCodec->Reset();
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_FLUSH)) //1002
    {
      bool sync = static_cast<CDVDMsgBool*>(pMsg)->m_value;
      CLog::Log(LOGDEBUG, "COMXPlayerAudio - CDVDMsg::GENERAL_FLUSH(%d, %f)", sync, m_start_pts);

      m_stalled   = true;
      m_syncState = IDVDStreamPlayer::SYNC_STARTING;
      m_flush = false;
      m_start_pts = DVD_NOPTS_VALUE;
      m_audioClock = 0; //DVD_NOPTS_VALUE

      m_hisiAudio->reset();
      m_pAudioCodec->Reset();
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_EOF)) //1008
    {
      CLog::Log(LOGDEBUG, "COMXPlayerAudio - CDVDMsg::GENERAL_EOF");
      m_hisiAudio->SubmitEos();
    }
    else if (pMsg->IsType(CDVDMsg::PLAYER_SETSPEED)) //1017
    {
      if (m_speed != static_cast<CDVDMsgInt*>(pMsg)->m_value)
      {
        m_speed = static_cast<CDVDMsgInt*>(pMsg)->m_value;
        CLog::Log(LOGDEBUG, "COMXPlayerAudio - CDVDMsg::PLAYER_SETSPEED %d", m_speed);
      }
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_STREAMCHANGE)) //1005
    {
      COMXMsgAudioCodecChange* msg(static_cast<COMXMsgAudioCodecChange*>(pMsg));
      CLog::Log(LOGDEBUG, "COMXPlayerAudio - CDVDMsg::GENERAL_STREAMCHANGE");
      OpenStream(msg->m_hints, msg->m_codec);
      msg->m_codec = NULL;
    }

    pMsg->Release();
  }
}


/* complete */
void CHiPlayerAudio::CloseDecoder()
{
  m_hisiAudio->close();
  m_DecoderOpen = false;
}


/* complete */
double CHiPlayerAudio::GetDelay()
{
  return m_hisiAudio->Delay();
}


/* complete */
double CHiPlayerAudio::GetCacheTime()
{
  return m_hisiAudio->CacheTime();
}


/* complete */
double CHiPlayerAudio::GetCacheTotal()
{
  return m_hisiAudio->CacheTotal();
}


/* complete */
void CHiPlayerAudio::SubmitEOS()
{
  return m_hisiAudio->SubmitEos();
}



bool CHiPlayerAudio::OpenStream(CDVDStreamInfo &hints)
{
  m_bad_state = false;

  m_processInfo.ResetAudioCodecInfo();

  if (m_processInfo.AllowDTSHDDecode())
  {
    printf("todo");
  }


#if 0
  COMXAudioCodecOMX *codec = new COMXAudioCodecOMX(m_processInfo);

  if(!codec || !codec->Open(hints))
  {
    CLog::Log(LOGERROR, "Unsupported audio codec");
    delete codec; codec = NULL;
    return false;
  }

  if(m_messageQueue.IsInited())
    m_messageQueue.Put(new COMXMsgAudioCodecChange(hints, codec), 0);
  else
  {
    OpenStream(hints, codec);
    m_messageQueue.Init();
    CLog::Log(LOGNOTICE, "Creating audio thread");
    Create();
  }
#endif

  return true;
}




