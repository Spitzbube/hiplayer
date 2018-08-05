#pragma once

/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "threads/Thread.h"
#include "IVideoPlayer.h"
#include "DVDMessageQueue.h"
#include "DVDStreamInfo.h"
#include "DVDCodecs/Video/DVDVideoCodec.h"
#include "DVDClock.h"
#include "DVDOverlayContainer.h"
#include "DVDTSCorrection.h"
#include "cores/VideoPlayer/VideoRenderers/RenderManager.h"
#include "utils/BitstreamStats.h"
#include "utils/BitstreamConverter.h"
#include "guilib/Geometry.h"
#include "HiCodecVideo.h"
#include <atomic>

class CHiPlayerVideo : public CThread, public IDVDStreamPlayerVideo
{
public:
  CHiPlayerVideo(CDVDClock* pClock
                 ,CDVDOverlayContainer* pOverlayContainer
                 ,CDVDMessageQueue& parent
                 ,CRenderManager& renderManager,
                 CProcessInfo &processInfo);
  
  virtual ~CHiPlayerVideo();

  bool OpenStream(CDVDStreamInfo &hint);
  void CloseStream(bool bWaitForBuffers);

  void Flush(bool sync);
  bool AcceptsData() const { return !m_messageQueue.IsFull(); }
  bool HasData() const ;
  int  GetLevel() const { return m_messageQueue.GetLevel(); }
  bool IsInited() const { return m_messageQueue.IsInited(); }
  void SendMessage(CDVDMsg* pMsg, int priority = 0) { m_messageQueue.Put(pMsg, priority); }
  void FlushMessages() { m_messageQueue.Flush(); }

  void EnableSubtitle(bool bEnable) { m_bRenderSubs = bEnable; }
  bool IsSubtitleEnabled() { return m_bRenderSubs; }
  void EnableFullscreen(bool bEnable) { m_bAllowFullscreen = bEnable; }
  double GetSubtitleDelay() { return m_iSubtitleDelay; }
  void SetSubtitleDelay(double delay) { m_iSubtitleDelay = delay; }
  bool IsStalled() const override { return m_stalled; }
  bool IsRewindStalled() const override { return m_rewindStalled; }
  double GetCurrentPts();
  double GetOutputDelay(); /* returns the expected delay, from that a packet is put in queue */
  int GetDecoderFreeSpace() { return 0; }
  std::string GetPlayerInfo();
  int GetVideoBitrate();
  std::string GetStereoMode();
  void SetSpeed(int iSpeed);
	void	SubmitEOS();

protected:
  virtual void OnStartup();
  virtual void OnExit();
  virtual void Process();

	
	void OpenStream(CDVDStreamInfo &hint, CDVDVideoCodecHisi* codec);
  void ProcessOverlays(double pts);
	void ResolutionUpdateCallBack();
	void Output(double pts, bool bDropPacket);
	void SetVideoRect(const CRect &InSrcRect, const CRect &InDestRect);

  CDVDStreamInfo m_hints;

  int m_speed;

  bool m_bRenderSubs;
  bool m_bAllowFullscreen;
  bool m_paused;
  bool m_bFpsInvalid;        // needed to ignore fps (e.g. dvd stills)

  float m_fForcedAspectRatio;
	
  double m_fFrameRate;       //framerate of the video currently playing
  double m_iSubtitleDelay;

  bool m_stalled;
  std::atomic_bool m_rewindStalled;
  std::atomic_bool m_bAbortOutput;
	
  BitstreamStats m_videoStats;
	
  CDVDMessageQueue  m_messageQueue;
  CDVDMessageQueue& m_messageParent;
  
  CRenderManager& m_renderManager;

  CDVDOverlayContainer  *m_pOverlayContainer;
	
	CBitstreamConverter *m_bitstream;

  IDVDStreamPlayer::ESyncState m_syncState;

	CDVDVideoCodecHisi * m_pVideoCodec;
	
  CRect                     m_src_rect;
  CRect                     m_dst_rect;
  RENDER_STEREO_MODE        m_video_stereo_mode;
  RENDER_STEREO_MODE        m_display_stereo_mode;
  bool                      m_StereoInvert;
  double                    m_start_pts;

	DVDVideoPicture m_picture;
};

