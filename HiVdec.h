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

#include "cores/VideoPlayer/DVDCodecs/Video/DVDVideoCodec.h"
#include "cores/VideoPlayer/DVDStreamInfo.h"
#include "cores/IPlayer.h"
#include "guilib/Geometry.h"
#include "rendering/RenderSystem.h"
#include "threads/Thread.h"
#include <deque>

#include "HiDecoder.h"

class PosixFile;
typedef std::shared_ptr<PosixFile> PosixFilePtr;


class CHisiVdec : public CThread
{
public:
  CHisiVdec();
  virtual ~CHisiVdec();

  bool          OpenDecoder(CDVDStreamInfo &hints);
  void          CloseDecoder();
  void          Reset();
	void 					ReSync(double pts);

  int           Decode(uint8_t *pData, size_t size, double dts, double pts, std::atomic_bool& abort);

  bool          GetPicture(DVDVideoPicture* pDvdVideoPicture);
	bool 					GetFramePts(int& pts);
  void          SetSpeed(int speed);
  int           GetDataSize();
  double        GetTimeSize();
  void          SetVideoRect(const CRect &SrcRect, const CRect &DestRect);
  void          SetFreeRun(const bool freerun);

	void					SubmitEos();
	bool					EOS();
	bool					PlayStarted();
	
	bool 					BufferReady();
	bool					BufferFull();
	double				FirstPts();
	double 				GetCurrentPts();
	
protected:
  virtual void  Process();

private:
#if 0
  void          ShowMainVideo(const bool show);
  void          SetVideoZoom(const float zoom);
  void          SetVideoContrast(const int contrast);
  void          SetVideoBrightness(const int brightness);
  void          SetVideoSaturation(const int saturation);
  bool          SetVideo3dMode(const int mode3d);
  std::string   GetStereoMode();
#endif	
  void					SetHeader();

private:
  bool             m_opened;
  CDVDStreamInfo   m_hints;
  volatile int     m_speed;
  volatile int     m_mode;
	
  CEvent           m_ready_event;
	
  CRect            m_dst_rect;
  CRect            m_display_rect;
  int              m_view_mode;
	
  RENDER_STEREO_MODE m_stereo_mode;
  RENDER_STEREO_VIEW m_stereo_view;
	
  float            m_zoom;
  int              m_contrast;
  int              m_brightness;

  std::deque<int>  m_ptsQueue;
  CCriticalSection m_ptsQueueMutex;

  unsigned int		m_video_rate;
  unsigned char* 	m_extradata;
  unsigned int		m_extrasize;
	bool						m_extraneed;
	
  unsigned char*	m_header;
  unsigned int		m_headersize;
  
  CRect						m_VideoRect;
    
  HisiAvDecoder * hwDec;
};
