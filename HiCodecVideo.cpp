
#include "HiCodecVideo.h"
#include "HiVdec.h"
#include "DVDClock.h"
#include "utils/BitstreamConverter.h"
#include "utils/log.h"

#define __MODULE_NAME__ "DVDVideoCodecHisi"

typedef struct frame_queue {
  double dts;
  double pts;
  double sort_time;
  struct frame_queue *nextframe;
} frame_queue;


/* complete */
bool CDVDVideoCodecHisi::ClearPicture(DVDVideoPicture* pDvdVideoPicture)
{
  return true;
}

 
/* todo */
bool CDVDVideoCodecHisi::GetPicture(DVDVideoPicture *pDvdVideoPicture)
{
  if (m_Codec)
    m_Codec->GetPicture(&m_videobuffer);
  *pDvdVideoPicture = m_videobuffer;

  // check for mpeg2 aspect ratio changes
  if (m_mpeg2_sequence && pDvdVideoPicture->pts >= m_mpeg2_sequence_pts)
    m_aspect_ratio = m_mpeg2_sequence->ratio;

  pDvdVideoPicture->iDisplayWidth  = pDvdVideoPicture->iWidth;
  pDvdVideoPicture->iDisplayHeight = pDvdVideoPicture->iHeight;
  if (m_aspect_ratio > 1.0 && !m_hints.forced_aspect)
  {
    pDvdVideoPicture->iDisplayWidth  = ((int)lrint(pDvdVideoPicture->iHeight * m_aspect_ratio)) & ~3;
    if (pDvdVideoPicture->iDisplayWidth > pDvdVideoPicture->iWidth)
    {
      pDvdVideoPicture->iDisplayWidth  = pDvdVideoPicture->iWidth;
      pDvdVideoPicture->iDisplayHeight = ((int)lrint(pDvdVideoPicture->iWidth / m_aspect_ratio)) & ~3;
    }
  }

  return true;
}

 
/* complete */
void CDVDVideoCodecHisi::SetDropState(bool bDrop)
{
  if (bDrop == m_drop)
    return;

  m_drop = bDrop;
  if (bDrop)
    m_videobuffer.iFlags |=  DVP_FLAG_DROPPED;
  else
    m_videobuffer.iFlags &= ~DVP_FLAG_DROPPED;

  if (m_Codec)
  {
    m_Codec->SetFreeRun(bDrop);
  }
}

 
/* complete */
void CDVDVideoCodecHisi::SetSpeed(int iSpeed)
{
  if (m_Codec)
    return m_Codec->SetSpeed(iSpeed);
}

 
/* complete */
int CDVDVideoCodecHisi::GetDataSize(void)
{
  if (m_Codec)
    return m_Codec->GetDataSize();
}

 
/* complete */
double CDVDVideoCodecHisi::GetTimeSize(void)
{
  if (m_Codec)
    return m_Codec->GetTimeSize();
  else 
    return 0;
}


/* todo */
bool CDVDVideoCodecHisi::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  if (hints.stills)
    return false;

  m_hints = hints;

  switch(hints.codec)
  {
    case AV_CODEC_ID_MJPEG: //8
      m_pFormatName = "hi-mjpeg";
      break;
    case AV_CODEC_ID_MPEG1VIDEO: //1
    case AV_CODEC_ID_MPEG2VIDEO: //2
    case AV_CODEC_ID_MPEG2VIDEO_XVMC: //3
      m_pFormatName = "hi-mpeg2";
      break;
    case AV_CODEC_ID_H264: //28
      m_pFormatName = "hi-h264";
      // convert h264-avcC to h264-annex-b as h264-avcC
      // under streamers can have issues when seeking.
      if (m_hints.extradata && *(uint8_t*)m_hints.extradata == 1)
      {
        m_bitstream = new CBitstreamConverter;
        m_bitstream->Open(m_hints.codec, (uint8_t*)m_hints.extradata, m_hints.extrasize, true);
        // make sure we do not leak the existing m_hints.extradata
        free(m_hints.extradata);
        m_hints.extrasize = m_bitstream->GetExtraSize();
        m_hints.extradata = malloc(m_hints.extrasize);
        memcpy(m_hints.extradata, m_bitstream->GetExtraData(), m_hints.extrasize);
      }
      //m_bitparser = new CBitstreamParser();
      //m_bitparser->Open();
      break;
    case AV_CODEC_ID_H263: //5
    case AV_CODEC_ID_H263P: //20
    case AV_CODEC_ID_H263I: //21
      m_pFormatName = "hi-h263";
      break;
    case AV_CODEC_ID_FLV1:
      m_pFormatName = "hi-flv1";
      break;
    case AV_CODEC_ID_RV10: //6
    case AV_CODEC_ID_RV20: //7
    case AV_CODEC_ID_RV30: //69
    case AV_CODEC_ID_RV40: //70
      m_pFormatName = "hi-rv";
      break;
    case AV_CODEC_ID_VC1:
      m_pFormatName = "hi-vc1";
      break;
    case AV_CODEC_ID_WMV3:
      m_pFormatName = "hi-wmv3";
      break;
    case AV_CODEC_ID_AVS:
    case AV_CODEC_ID_CAVS:
      m_pFormatName = "hi-avs";
      break;
    case AV_CODEC_ID_HEVC:
      m_pFormatName = "hi-h265";
      m_bitstream = new CBitstreamConverter();
      m_bitstream->Open(m_hints.codec, (uint8_t*)m_hints.extradata, m_hints.extrasize, true);
      // make sure we do not leak the existing m_hints.extradata
      free(m_hints.extradata);
      m_hints.extrasize = m_bitstream->GetExtraSize();
      m_hints.extradata = malloc(m_hints.extrasize);
      memcpy(m_hints.extradata, m_bitstream->GetExtraData(), m_hints.extrasize);
      break;
    case AV_CODEC_ID_MPEG4: //13
    case AV_CODEC_ID_MSMPEG4V2:
    case AV_CODEC_ID_MSMPEG4V3:
      m_pFormatName = "hi-mpeg4";
    default:
      CLog::Log(LOGDEBUG, "%s: Unknown hints.codec: %d!", __MODULE_NAME__, m_hints.codec);
//      return false;
      break;
  }

  CLog::Log(LOGDEBUG, "%s: format:%s", __MODULE_NAME__, m_pFormatName);

  m_aspect_ratio = m_hints.aspect;

  m_Codec = new CHisiVdec;

  m_opened = false;

  // allocate a dummy DVDVideoPicture buffer.
  // first make sure all properties are reset.
  memset(&m_videobuffer, 0, sizeof(DVDVideoPicture));

  m_videobuffer.dts = DVD_NOPTS_VALUE;
  m_videobuffer.pts = DVD_NOPTS_VALUE;
  m_videobuffer.color_range  = 0;
  m_videobuffer.color_matrix = 4;
  m_videobuffer.iFlags  = DVP_FLAG_ALLOCATED;
  m_videobuffer.iWidth  = m_hints.width;
  m_videobuffer.iHeight = m_hints.height;

  m_videobuffer.iDisplayWidth  = m_videobuffer.iWidth;
  m_videobuffer.iDisplayHeight = m_videobuffer.iHeight;
  if (m_hints.aspect > 0.0 && !m_hints.forced_aspect)
  {
    m_videobuffer.iDisplayWidth  = ((int)lrint(m_videobuffer.iHeight * m_hints.aspect)) & ~3;
    if (m_videobuffer.iDisplayWidth > m_videobuffer.iWidth)
    {
      m_videobuffer.iDisplayWidth  = m_videobuffer.iWidth;
      m_videobuffer.iDisplayHeight = ((int)lrint(m_videobuffer.iWidth / m_hints.aspect)) & ~3;
    }
  }

  m_processInfo.SetVideoDecoderName(m_pFormatName, true);
  m_processInfo.SetVideoDimensions(m_hints.width, m_hints.height);
  m_processInfo.SetVideoDeintMethod("hardware");

  CLog::Log(LOGINFO, "%s: Opened Hisi Codec", __MODULE_NAME__);
  return true;
}

 
/* complete */
void CDVDVideoCodecHisi::Reset(void)
{
  while (m_queue_depth)
    FrameQueuePop();

  m_Codec->Reset();
  m_mpeg2_sequence_pts = 0;
}

 
/* complete */
CDVDVideoCodecHisi::CDVDVideoCodecHisi(CProcessInfo &processInfo) 
: CDVDVideoCodec(processInfo),
  m_Codec(NULL),
  m_pFormatName("hisicodec"),
  m_last_pts(0.0),
  m_frame_queue(NULL),
  m_queue_depth(0),
  m_framerate(0.0),
  m_video_rate(0),
  m_mpeg2_sequence(NULL),
  m_bitparser(NULL),
  m_bitstream(NULL),
  m_opened(false),
  m_drop(false)
{
  pthread_mutex_init(&m_queue_mutex, NULL);
}


/* todo */
CDVDVideoCodecHisi::~CDVDVideoCodecHisi()
{
  Dispose();
  pthread_mutex_destroy(&m_queue_mutex);
}


/* todo */
int CDVDVideoCodecHisi::Decode(uint8_t *pData, int iSize, double dts, double pts)
{
  std::atomic_bool abort(false);
  return Decode(pData, iSize, dts, pts, abort);
}


/* todo */
void CDVDVideoCodecHisi::Dispose(void)
{
  if (m_Codec)
    m_Codec->CloseDecoder(), delete m_Codec, m_Codec = NULL;
  if (m_videobuffer.iFlags)
    m_videobuffer.iFlags = 0;
  if (m_mpeg2_sequence)
    delete m_mpeg2_sequence, m_mpeg2_sequence = NULL;

  if (m_bitstream)
    delete m_bitstream, m_bitstream = NULL;

  if (m_bitparser)
    delete m_bitparser, m_bitparser = NULL;

  while (m_queue_depth)
    FrameQueuePop();
}

 
/* todo */
int CDVDVideoCodecHisi::Decode(uint8_t *pData, int iSize, double dts, double pts, std::atomic_bool& abort)
{
  // Handle Input, add demuxer packet to input queue, we must accept it or
  // it will be discarded as VideoPlayerVideo has no concept of "try again".
  if (pData)
  {
    if (m_bitstream)
    {
      if (!m_bitstream->Convert(pData, iSize))
        return VC_ERROR;

      pData = m_bitstream->GetConvertBuffer();
      iSize = m_bitstream->GetConvertSize();
    }

    if (m_bitparser)
      m_bitparser->FindIdrSlice(pData, iSize);

    FrameRateTracking( pData, iSize, dts, pts);
  }

  if (!m_opened)
  {
    if (m_Codec && !m_Codec->OpenDecoder(m_hints))
      CLog::Log(LOGERROR, "%s: Failed to open Hisi Codec", __MODULE_NAME__);
    m_opened = true;
  }

  if (m_hints.ptsinvalid)
    pts = DVD_NOPTS_VALUE;

  return m_Codec->Decode(pData, iSize, dts, pts, abort);
}


/* complete */
void CDVDVideoCodecHisi::ReSync(double pts)
{
  while (m_queue_depth)
    FrameQueuePop();

  m_Codec->ReSync(pts);
  m_mpeg2_sequence_pts = 0;
}


/* complete */
void CDVDVideoCodecHisi::SubmitEOS()
{
  return m_Codec->SubmitEos();
}


/* complete */
bool CDVDVideoCodecHisi::IsEOS()
{
  return m_Codec->EOS();
}


/* complete */
bool CDVDVideoCodecHisi::PlayStarted()
{
  return m_Codec->PlayStarted();
}


/* complete */
bool CDVDVideoCodecHisi::BufferFull()
{
  return m_Codec->BufferFull();
}


/* complete */
bool CDVDVideoCodecHisi::BufferReady()
{
  return m_Codec->BufferReady();
}


/* complete */
double CDVDVideoCodecHisi::FirstPts()
{
  return m_Codec->FirstPts();
}


/* complete */
double CDVDVideoCodecHisi::GetCurrentPts()
{
  return m_Codec->GetCurrentPts();
}


/* complete */
void CDVDVideoCodecHisi::FrameQueuePop(void)
{
  if (!m_frame_queue || m_queue_depth == 0)
    return;

  pthread_mutex_lock(&m_queue_mutex);
  // pop the top frame off the queue
  frame_queue *top = m_frame_queue;
  m_frame_queue = top->nextframe;
  m_queue_depth--;
  pthread_mutex_unlock(&m_queue_mutex);

  // and release it
  free(top);
}


/* nearly complete */
void CDVDVideoCodecHisi::FrameQueuePush(double dts, double pts)
{
  frame_queue *newframe = (frame_queue*)calloc(sizeof(frame_queue), 1);
  newframe->dts = dts;
  newframe->pts = pts;
  // if both dts or pts are good we use those, else use decoder insert time for frame sort
  if ((newframe->pts != DVD_NOPTS_VALUE) || (newframe->dts != DVD_NOPTS_VALUE))
  {
    // if pts is borked (stupid avi's), use dts for frame sort
    if (newframe->pts == DVD_NOPTS_VALUE)
      newframe->sort_time = newframe->dts;
    else
      newframe->sort_time = newframe->pts;
  }

  pthread_mutex_lock(&m_queue_mutex);
  frame_queue *queueWalker = m_frame_queue;
  if (!queueWalker || (newframe->sort_time < queueWalker->sort_time))
  {
    // we have an empty queue, or this frame earlier than the current queue head.
    newframe->nextframe = queueWalker;
    m_frame_queue = newframe;
  }
  else
  {
    // walk the queue and insert this frame where it belongs in display order.
    bool ptrInserted = false;
    frame_queue *nextframe = NULL;
    //
    while (!ptrInserted)
    {
      nextframe = queueWalker->nextframe;
      if (!nextframe || (newframe->sort_time < nextframe->sort_time))
      {
        // if the next frame is the tail of the queue, or our new frame is earlier.
        newframe->nextframe = nextframe;
        queueWalker->nextframe = newframe;
        ptrInserted = true;
      }
      queueWalker = nextframe;
    }
  }
  m_queue_depth++;
  pthread_mutex_unlock(&m_queue_mutex);	
}


/* complete */
void CDVDVideoCodecHisi::FrameRateTracking(uint8_t *pData, int iSize, double dts, double pts)
{
  // mpeg2 handling
  if (m_mpeg2_sequence)
  {
    // probe demux for sequence_header_code NAL and
    // decode aspect ratio and frame rate.
    if (CBitstreamConverter::mpeg2_sequence_header(pData, iSize, m_mpeg2_sequence))
    {
      m_mpeg2_sequence_pts = pts;
      if (m_mpeg2_sequence_pts == DVD_NOPTS_VALUE)
        m_mpeg2_sequence_pts = dts;

      m_framerate = m_mpeg2_sequence->rate;
      m_video_rate = (int)(0.5 + (96000.0 / m_framerate));

      CLog::Log(LOGDEBUG, "%s: detected mpeg2 aspect ratio(%f), framerate(%f), video_rate(%d)",
        __MODULE_NAME__, m_mpeg2_sequence->ratio, m_framerate, m_video_rate);

      // update m_hints for 1st frame fixup.
      switch(m_mpeg2_sequence->rate_info)
      {
        default:
        case 0x01:
          m_hints.fpsrate = 24000.0;
          m_hints.fpsscale = 1001.0;
          break;
        case 0x02:
          m_hints.fpsrate = 24000.0;
          m_hints.fpsscale = 1000.0;
          break;
        case 0x03:
          m_hints.fpsrate = 25000.0;
          m_hints.fpsscale = 1000.0;
          break;
        case 0x04:
          m_hints.fpsrate = 30000.0;
          m_hints.fpsscale = 1001.0;
          break;
        case 0x05:
          m_hints.fpsrate = 30000.0;
          m_hints.fpsscale = 1000.0;
          break;
        case 0x06:
          m_hints.fpsrate = 50000.0;
          m_hints.fpsscale = 1000.0;
          break;
        case 0x07:
          m_hints.fpsrate = 60000.0;
          m_hints.fpsscale = 1001.0;
          break;
        case 0x08:
          m_hints.fpsrate = 60000.0;
          m_hints.fpsscale = 1000.0;
          break;
      }
      m_hints.width    = m_mpeg2_sequence->width;
      m_hints.height   = m_mpeg2_sequence->height;
      m_hints.aspect   = m_mpeg2_sequence->ratio;
    }
    return;
  }

  // everything else
  FrameQueuePush(dts, pts);

  // we might have out-of-order pts,
  // so make sure we wait for at least 8 values in sorted queue.
  if (m_queue_depth > 16)
  {
    pthread_mutex_lock(&m_queue_mutex);

    float cur_pts = m_frame_queue->pts;
    if (cur_pts == DVD_NOPTS_VALUE)
      cur_pts = m_frame_queue->dts;

    pthread_mutex_unlock(&m_queue_mutex);	

    float duration = cur_pts - m_last_pts;
    m_last_pts = cur_pts;

    // clamp duration to sensible range,
    // 66 fsp to 20 fsp
    if (duration >= 15000.0 && duration <= 50000.0)
    {
      double framerate;
      switch((int)(0.5 + duration))
      {
        // 59.940 (16683.333333)
        case 16000 ... 17000:
          framerate = 60000.0 / 1001.0;
          break;

        // 50.000 (20000.000000)
        case 20000:
          framerate = 50000.0 / 1000.0;
          break;

        // 49.950 (20020.000000)
        case 20020:
          framerate = 50000.0 / 1001.0;
          break;

        // 29.970 (33366.666656)
        case 32000 ... 35000:
          framerate = 30000.0 / 1001.0;
          break;

        // 25.000 (40000.000000)
        case 40000:
          framerate = 25000.0 / 1000.0;
          break;

        // 24.975 (40040.000000)
        case 40040:
          framerate = 25000.0 / 1001.0;
          break;

        /*
        // 24.000 (41666.666666)
        case 41667:
          framerate = 24000.0 / 1000.0;
          break;
        */

        // 23.976 (41708.33333)
        case 40200 ... 43200:
          // 23.976 seems to have the crappiest encodings :)
          framerate = 24000.0 / 1001.0;
          break;

        default:
          framerate = 0.0;
          //CLog::Log(LOGDEBUG, "%s: unknown duration(%f), cur_pts(%f)",
          //  __MODULE_NAME__, duration, cur_pts);
          break;
      }

      if (framerate > 0.0 && (int)m_framerate != (int)framerate)
      {
        m_framerate = framerate;
        m_video_rate = (int)(0.5 + (96000.0 / framerate));
        CLog::Log(LOGDEBUG, "%s: detected new framerate(%f), video_rate(%d)",
          __MODULE_NAME__, m_framerate, m_video_rate);
      }
    }

    FrameQueuePop();
  }
}



