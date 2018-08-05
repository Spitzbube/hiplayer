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
#include "cores/VideoPlayer/DVDStreamInfo.h"
#include <deque>

extern "C" {
#include <hisi/hi_type.h>
#include <hisi/hi_unf_video.h>
#include <hisi/hi_unf_audio.h>
#include <hisi/hi_unf_common.h>
#include <hisi/hi_unf_vo.h>
#include <hisi/hi_unf_disp.h>
#include <hisi/hi_unf_advca.h>
#include <hisi/hi_unf_otp.h> 
#include <hisi/hi_adp_mpi.h> 
#include <hisi/hi_audio_codec.h> 
#include "hisi/HA.AUDIO.PCM.decode.h"

}  // extern "C"

#define PTS_FREQ        90000
#define UNIT_FREQ       96000
#define AV_SYNC_THRESH  PTS_FREQ*30
#define PTS_US_TO_MS(PTS_US) ((PTS_US) / 1000.0f)
#define PTS_MS_TO_US(PTS_MS) ((PTS_MS) * 1000.0f)
#define PTS_90KHZ_TO_MS(PTS_90MS) ((PTS_90MS) / 90.0f)
#define PTS_MS_TO_90KHZ(PTS_MS) ((PTS_MS) * 90.0f)

enum
{
	VIDEO_MODE_NORMAL,
	VIDEO_MODE_PAUSE,
	VIDEO_MODE_FORWARD,
	VIDEO_MODE_BACKWARD
};

class HisiAvDecoder
{
public:
  static HisiAvDecoder* GetInstance();
  
  HisiAvDecoder();
  virtual ~HisiAvDecoder();

  bool		IsEOS();

  bool		VideoOpen(CDVDStreamInfo &hints);
  bool		VideoClose();
  void		VideoReset();
  void		VideoReSync(unsigned int pts);
	
  bool		VideoWrite(uint8_t *pData, size_t iSize, unsigned int pts_ms);
  bool		VideoWriteEx(uint8_t *pData, size_t iSize, unsigned int pts_ms, bool continues, bool last);
	
  double	VideoFirstPts();
  double	VideoCurrentPts();
  bool		VideoFramePts(int &pts);
	
  bool		VideoBufferReady();
  bool		VideoBufferFull();
  bool		VideoPlayStarted();
  void		VideoSetSpeed(int speed);
  void		VideoSubmitEOS();
  void		VideoSetRect(int x, int y, int width, int height);	
	
  bool		AudioOpen(CDVDStreamInfo &hints);
  bool		AudioClose(); 
  void		AudioReset();
  void		AudioReSync(unsigned int pts);

	
  bool		AudioBufferReady();
  bool		AudioBufferFull();
  bool		AudioWrite(uint8_t *pData, size_t iSize, unsigned int pts_ms);
  bool		AudioWriteEx(uint8_t *pData, size_t iSize, unsigned int pts_ms, bool continues, bool last);

  bool		AudioPlayStarted();
  double	AudioCurrentPts();
  double	AudioFirstPts();
  double	AudioCacheTotal();
  double	AudioCachetime();
  double	AudioDelay();
	
  void		AudioSubmitEOS();

  HI_S32	EventReport(HI_HANDLE hAvplay, HI_UNF_AVPLAY_EVENT_E enEvent, HI_VOID* pPara);
	
protected:
  bool		Init();
  bool		Deinit();
  void		SubmitEOS();
  
  HI_UNF_VCODEC_TYPE_E codec2vdec(int enc);
  HA_CODEC_ID_E codec2adec(int enc);

private:
  bool						m_opened;
  bool						m_video_opend;
  bool						m_audio_opend;
  bool						m_dump;
  
  unsigned int      		m_hAvplay;
  unsigned int      		m_hWin;
  unsigned int 				m_hTrack;

  //video play
  int						m_mode;
  int						m_speed;
  
  HI_UNF_DISP_E				m_disp;
  bool						m_freerun;

  CCriticalSection  m_section;
  CCriticalSection	m_EventMutex;
    
  std::deque<int>  m_aptsQueue;
  std::deque<int>  m_vptsQueue;


	bool			m_audio_full;
	bool			m_video_full;
	bool			m_video_eos;
	bool			m_audio_eos;
	bool			m_eos;

	int				m_audio_last_package_pts;
	int 			m_audio_bytes_pes_sec;
	double			m_audio_cache_total;
	double			m_audio_cache_time;
	double			m_audio_delay;
	
	double			m_video_1st_pts;
	double			m_audio_1st_pts;

	bool			m_audio_play_started;
	bool   			m_video_play_started;
	
	bool			m_audio_ready;
	bool			m_video_ready;
};
