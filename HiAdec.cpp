
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


