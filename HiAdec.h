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

#include "cores/VideoPlayer/DVDStreamInfo.h"
#include "threads/Thread.h"
#include "HiDecoder.h"

class CHiAudio : public CThread
{
public:
    CHiAudio();
    virtual ~CHiAudio();

    bool open(CDVDStreamInfo hints);
    bool close();
    
    bool PlayStarted();
    bool EOS();
		bool Ready();
    bool Full();

    double Delay();
    double CacheTime();
    double CacheTotal();
    double FirstPts();
    double CurrentPts();

    void SubmitEos();
    void reset();
    void reSync(double pts);
    
    bool Push(uint8_t *pData, size_t iSize, unsigned int pts_ms);
    bool PushEx(uint8_t *pData, size_t iSize, unsigned int pts_ms, bool continues, bool last);

private:
    HisiAvDecoder * hwDec;
		bool m_bStop;
};
