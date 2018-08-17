

#include "DVDClock.h"
#include "HiAdec.h"


CHiAudio::CHiAudio() : CThread("CHiAudio")
{
  hwDec = HisiAvDecoder::GetInstance();
}


CHiAudio::~CHiAudio()
{
}


bool CHiAudio::open(CDVDStreamInfo hints)
{
  return hwDec->AudioOpen(hints);
}


bool CHiAudio::close()
{
  m_bStop = true;
  return hwDec->AudioClose();
}


bool CHiAudio::PlayStarted()
{
  return (hwDec->AudioFirstPts() != DVD_NOPTS_VALUE)? true: false;
}


bool CHiAudio::EOS()
{
  return hwDec->IsEOS();
}


bool CHiAudio::Full()
{
  return hwDec->AudioBufferFull();
}


bool CHiAudio::Ready()
{
  return hwDec->AudioBufferReady();
}


double CHiAudio::Delay()
{
  return hwDec->AudioDelay();
}


double CHiAudio::CacheTime()
{
  return hwDec->AudioCachetime();
}


double CHiAudio::CacheTotal()
{
  return hwDec->AudioCacheTotal();
}


double CHiAudio::FirstPts()
{
  return hwDec->AudioFirstPts();
}


double CHiAudio::CurrentPts()
{
  return hwDec->AudioCurrentPts();
}


void CHiAudio::reset()
{
  return hwDec->AudioReset();
}


void CHiAudio::reSync(double pts)
{
  return hwDec->AudioReSync(pts);
}


void CHiAudio::SubmitEos()
{
  return hwDec->AudioSubmitEOS();
}


bool CHiAudio::Push(uint8_t *pData, size_t iSize, unsigned int pts_ms)
{
  return hwDec->AudioWrite(pData, iSize, pts_ms);
}


bool CHiAudio::PushEx(uint8_t *pData, size_t iSize, unsigned int pts_ms, bool continues, bool last)
{
  return hwDec->AudioWriteEx(pData, iSize, pts_ms, continues, last);
}



