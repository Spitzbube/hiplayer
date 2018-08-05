#pragma once

/*
 *      Copyright (C) 2005-2015 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Kodi; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <deque>
#include <sys/types.h>

#include "DVDStreamInfo.h"
#include "threads/Thread.h"
#include "IVideoPlayer.h"
#include "DVDDemuxers/DVDDemux.h"
#include "DVDMessageQueue.h"
#include "utils/BitstreamStats.h"

#include "HiCodecAudio.h"
#include "HiAdec.h"

class CHiPlayerAudio : public CThread, public IDVDStreamPlayerAudio
{
protected:
  CDVDMessageQueue      m_messageQueue;
  CDVDMessageQueue      &m_messageParent;

  CDVDStreamInfo            m_hints_current;
  CDVDStreamInfo            m_hints;
  CDVDClock                 *m_av_clock;
  CHiAudio                  *m_hisiAudio;
  std::string               m_codec_name;
  bool                      m_passthrough;
  AEAudioFormat             m_format;
  CDVDAudioCodecHisi        *m_pAudioCodec;
  int                       m_speed;
  bool                      m_silence;
  double                    m_audioClock;
  double                    m_start_pts ;

  bool                      m_stalled;
  IDVDStreamPlayer::ESyncState m_syncState;

  BitstreamStats            m_audioStats;

  bool                      m_buffer_empty;
  bool                      m_flush;
  bool                      m_DecoderOpen;

  bool                      m_bad_state;
  bool                      m_eos;
  std::atomic_bool          m_bAbortOutput;
	
  virtual void OnStartup();
  virtual void OnExit();
  virtual void Process();
  void OpenStream(CDVDStreamInfo &hints, CDVDAudioCodecHisi *codec);
private:
public:
  CHiPlayerAudio(CDVDClock *av_clock, CDVDMessageQueue& parent, CProcessInfo &processInfo);
  ~CHiPlayerAudio();
  bool OpenStream(CDVDStreamInfo &hints);
  void SendMessage(CDVDMsg* pMsg, int priority = 0) { m_messageQueue.Put(pMsg, priority); }
  void FlushMessages()                              { m_messageQueue.Flush(); }
  bool AcceptsData() const                          { return !m_messageQueue.IsFull(); }
  bool HasData() const                              ;
  bool IsInited() const                             { return m_messageQueue.IsInited(); }
  int  GetLevel() const                             { return m_messageQueue.GetLevel(); }
  bool IsStalled() const                            { return m_stalled;  }
  bool IsEOS();
  void WaitForBuffers();
  void CloseStream(bool bWaitForBuffers);
  bool CodecChange();
  bool Decode(DemuxPacket *pkt, bool bDropPacket, bool bTrickPlay);
  void Flush(bool sync);
  AEAudioFormat GetDataFormat(CDVDStreamInfo hints);
  bool IsPassthrough() const;
  bool OpenDecoder();
  void CloseDecoder();
  double GetDelay();
  double GetCacheTime();
  double GetCacheTotal();
  double GetCurrentPts() { return m_audioClock; };
  void SubmitEOS();

  void SetDynamicRangeCompression(long drc)              {   }
  float GetDynamicRangeAmplification() const             { return 0.0f; }
  void SetSpeed(int iSpeed);
  int  GetAudioBitrate();
  int GetAudioChannels();
  std::string GetPlayerInfo();

  bool BadState() { return m_bad_state; }
};

