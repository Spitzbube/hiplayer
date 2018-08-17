
#include "HiCodecAudio.h"
#include "utils/log.h"

// the size of the audio_render output port buffers
#define AUDIO_DECODE_OUTPUT_BUFFER (32*1024)
static const char rounded_up_channels_shift[] = {0,0,1,2,2,3,3,3,3};


/* complete */
CDVDAudioCodecHisi::CDVDAudioCodecHisi(CProcessInfo &processInfo)
: m_processInfo(processInfo)
{
  m_bGotFrame = false;
  m_pCodecContext = NULL;
  m_pConvert = NULL;
  m_iSampleFormat = AV_SAMPLE_FMT_NONE;
  m_desiredSampleFormat = AV_SAMPLE_FMT_NONE;
  m_pFrame1 = NULL;
  m_pBufferOutput = NULL;
  m_iBufferOutputUsed = 0;
  m_iBufferOutputAlloced = 0;
  m_channels = 0;
  m_desiredchannels = 2;
  m_bNoConcatenate = false;
  m_frameSize = 0;
}


/* todo */
CDVDAudioCodecHisi::~CDVDAudioCodecHisi()
{
  if (m_pBufferOutput)
  {
    av_free(m_pBufferOutput);
  }
  m_pBufferOutput = NULL;
  m_iBufferOutputUsed = 0;
  m_iBufferOutputAlloced = 0;

  Dispose();
}


/* todo */
bool CDVDAudioCodecHisi::Open(CDVDStreamInfo &hints)
{
  AVCodec* pCodec = NULL;

  if (hints.codec == AV_CODEC_ID_DTS/*0x15004*/)
  {
    pCodec = avcodec_find_decoder_by_name("dcadec");
    printf("CDVDAudioCodecHisi::Open, dcadec %p\n", pCodec);
  }

  if (pCodec == NULL)
  {
    pCodec = avcodec_find_decoder(hints.codec);
  }

  if (pCodec == NULL)
  {
    CLog::Log(LOGDEBUG, "CDVDAudioCodecHisi::Open() Unable to find codec %d", hints.codec);
    return false;
  }

  m_bFirstFrame = true;
  m_pCodecContext = avcodec_alloc_context3(pCodec);
  if (m_pCodecContext == NULL)
  {
    goto label_478;
  }

  m_pCodecContext->workaround_bugs = FF_BUG_AUTODETECT;
  m_pCodecContext->debug = 0;
  m_pCodecContext->debug_mv = 0;

  if (pCodec->capabilities & CODEC_CAP_TRUNCATED)
  {
    m_pCodecContext->flags |= CODEC_FLAG_TRUNCATED;
  }

  m_channels = 0;
  m_pCodecContext->request_channel_layout = 3;
  m_pCodecContext->bit_rate = hints.bitrate;
  m_pCodecContext->sample_rate = hints.samplerate;
  m_pCodecContext->channels = hints.channels;
  m_pCodecContext->block_align = hints.blockalign;
  m_pCodecContext->bits_per_coded_sample = hints.bitspersample;

  CLog::Log(LOGNOTICE, "CDVDAudioCodecHisi::Open() Requesting channel layout of %x", 3); //m_pCodecContext->request_channel_layout);

  if (m_pCodecContext->bits_per_coded_sample == 0)
  {
    m_pCodecContext->bits_per_coded_sample = 16;
  }

  if (hints.extradata && hints.extrasize)
  {
    m_pCodecContext->extradata = (uint8_t*) av_mallocz(hints.extrasize + 32);
    if (m_pCodecContext->extradata)
    {
      m_pCodecContext->extradata_size = hints.extrasize;
      memcpy(m_pCodecContext->extradata, hints.extradata, hints.extrasize);
    }
  }

  if (avcodec_open2(m_pCodecContext, pCodec, /*AVDictionary ** */NULL) < 0)
  {
    goto label_430;
  }

  m_pFrame1 = av_frame_alloc();
  if (m_pFrame1 == NULL)
  {
    goto label_47c;
  }

  m_iSampleFormat = AV_SAMPLE_FMT_NONE;
  m_desiredSampleFormat = AV_SAMPLE_FMT_S16;

  m_processInfo.SetAudioDecoderName(m_pCodecContext->codec->name);

  return true;

label_478:
  return false;

label_430:
  CLog::Log(LOGDEBUG, "CDVDAudioCodecHisi::Open() Unable to open codec");
  Dispose();
  return false;

label_47c:
  Dispose();
  return false;
}


/* complete */
void CDVDAudioCodecHisi::Dispose()
{
  av_frame_free(&m_pFrame1);
  swr_free(&m_pConvert);
  avcodec_free_context(&m_pCodecContext);
  m_bGotFrame = false;
}


/* complete */
int CDVDAudioCodecHisi::Decode(BYTE* pData, int iSize, double dts, double pts)
{
  int iBytesUsed, got_frame;
  if (!m_pCodecContext) return -1;

  AVPacket avpkt;
  if (!m_iBufferOutputUsed)
  {
    m_dts = dts;
    m_pts = pts;
  }
  if (m_bGotFrame)
    return 0;
  av_init_packet(&avpkt);
  avpkt.data = pData;
  avpkt.size = iSize;
  iBytesUsed = avcodec_decode_audio4( m_pCodecContext
                                                 , m_pFrame1
                                                 , &got_frame
                                                 , &avpkt);
  if (iBytesUsed < 0 || !got_frame)
  {
    return iBytesUsed;
  }
  /* some codecs will attempt to consume more data than what we gave */
  if (iBytesUsed > iSize)
  {
    CLog::Log(LOGWARNING, "CDVDAudioCodecHisi::Decode - decoder attempted to consume more data than given");
    iBytesUsed = iSize;
  }

  if (m_bFirstFrame)
  {
    CLog::Log(LOGDEBUG, "CDVDAudioCodecHisi::Decode(%p,%d) format=%d(%d) chan=%d samples=%d size=%d data=%p,%p,%p,%p,%p,%p,%p,%p",
             pData, iSize, m_pCodecContext->sample_fmt, m_desiredSampleFormat, m_pCodecContext->channels, m_pFrame1->nb_samples,
             m_pFrame1->linesize[0],
             m_pFrame1->data[0], m_pFrame1->data[1], m_pFrame1->data[2], m_pFrame1->data[3], m_pFrame1->data[4], m_pFrame1->data[5], m_pFrame1->data[6], m_pFrame1->data[7]
             );
  }

  m_bGotFrame = true;
  return iBytesUsed;
}


/* complete */
int CDVDAudioCodecHisi::GetData(BYTE** dst, double &dts, double &pts)
{
  if (!m_bGotFrame)
    return 0;
  int inLineSize, outLineSize;
  /* input audio is aligned */
  int inputSize = av_samples_get_buffer_size(&inLineSize, m_pCodecContext->channels, m_pFrame1->nb_samples, m_pCodecContext->sample_fmt, 0);
  /* output audio will be packed */
  int outputSize = av_samples_get_buffer_size(&outLineSize, m_desiredchannels, m_pFrame1->nb_samples, m_desiredSampleFormat, 1);

  if (!m_bNoConcatenate && m_iBufferOutputUsed && (int)m_frameSize != outputSize)
  {
    CLog::Log(LOGERROR, "CDVDAudioCodecHisi::GetData Unexpected change of size (%d->%d)", m_frameSize, outputSize);
    m_bNoConcatenate = true;
  }
  // if this buffer won't fit then flush out what we have
  int desired_size = AUDIO_DECODE_OUTPUT_BUFFER * (m_desiredchannels * GetBitsPerSample()) >> (rounded_up_channels_shift[m_desiredchannels] + 4);
  if (m_iBufferOutputUsed && (m_iBufferOutputUsed + outputSize > desired_size || m_bNoConcatenate))
  {
     int ret = m_iBufferOutputUsed;
     m_iBufferOutputUsed = 0;
     m_bNoConcatenate = false;
     dts = m_dts;
     pts = m_pts;
     *dst = m_pBufferOutput;
     return ret;
  }
  m_frameSize = outputSize;

  if (m_iBufferOutputAlloced < m_iBufferOutputUsed + outputSize)
  {
     m_pBufferOutput = (BYTE*)av_realloc(m_pBufferOutput, m_iBufferOutputUsed + outputSize + FF_INPUT_BUFFER_PADDING_SIZE);
     m_iBufferOutputAlloced = m_iBufferOutputUsed + outputSize;
  }
  /* need to convert format */
  if(m_pCodecContext->sample_fmt != m_desiredSampleFormat || m_pCodecContext->channels != m_desiredchannels)
  {
    if(m_pConvert && (m_pCodecContext->sample_fmt != m_iSampleFormat || m_channels != m_pCodecContext->channels))
    {
      swr_free(&m_pConvert);
      m_channels = m_pCodecContext->channels;
    }

    if(!m_pConvert)
    {
      m_iSampleFormat = m_pCodecContext->sample_fmt;
      m_pConvert = swr_alloc_set_opts(NULL,
                      av_get_default_channel_layout(/*m_pCodecContext->channels*/m_desiredchannels), 
                      m_desiredSampleFormat, m_pCodecContext->sample_rate,
                      av_get_default_channel_layout(m_pCodecContext->channels), 
                      m_pCodecContext->sample_fmt, m_pCodecContext->sample_rate,
                      0, NULL);

      if(!m_pConvert || swr_init(m_pConvert) < 0)
      {
        CLog::Log(LOGERROR, "CDVDAudioCodecHisi::Decode - Unable to initialise convert format %d to %d", m_pCodecContext->sample_fmt, m_desiredSampleFormat);
        return 0;
      }
    }

    /* use unaligned flag to keep output packed */
    uint8_t *out_planes[m_pCodecContext->channels];
    if(av_samples_fill_arrays(out_planes, NULL, m_pBufferOutput + m_iBufferOutputUsed, /*m_pCodecContext->channels*/m_desiredchannels, m_pFrame1->nb_samples, m_desiredSampleFormat, 1) < 0 ||
       swr_convert(m_pConvert, out_planes, m_pFrame1->nb_samples, (const uint8_t **)m_pFrame1->data, m_pFrame1->nb_samples) < 0)
    {
      CLog::Log(LOGERROR, "CDVDAudioCodecHisi::Decode - Unable to convert format %d to %d", (int)m_pCodecContext->sample_fmt, m_desiredSampleFormat);
      outputSize = 0;
    }
  }
  else
  {
    /* copy to a contiguous buffer */
    uint8_t *out_planes[m_pCodecContext->channels];
    if (av_samples_fill_arrays(out_planes, NULL, m_pBufferOutput + m_iBufferOutputUsed, m_pCodecContext->channels, m_pFrame1->nb_samples, m_desiredSampleFormat, 1) < 0 ||
      av_samples_copy(out_planes, m_pFrame1->data, 0, 0, m_pFrame1->nb_samples, m_pCodecContext->channels, m_desiredSampleFormat) < 0 )
    {
      outputSize = 0;
    }
  }
  m_bGotFrame = false;

  if (m_bFirstFrame)
  {
    CLog::Log(LOGDEBUG, "CDVDAudioCodecHisi::GetData size=%d/%d line=%d/%d buf=%p, desired=%d", inputSize, outputSize, inLineSize, outLineSize, m_pBufferOutput, desired_size);
    m_bFirstFrame = false;
  }
  m_iBufferOutputUsed += outputSize;
  return 0;
}


/* complete */
void CDVDAudioCodecHisi::Reset()
{
  if (m_pCodecContext)
  {
    avcodec_flush_buffers(m_pCodecContext);
  }

  m_iBufferOutputUsed = 0;
  m_bGotFrame = false;
}


/* complete */
int CDVDAudioCodecHisi::GetChannels()
{
  if (!m_pCodecContext)
  {
    return 0;
  }

  return m_pCodecContext->channels;
}


/* complete */
int CDVDAudioCodecHisi::GetSampleRate()
{
  if (!m_pCodecContext)
  {
    return 0;
  }

  return m_pCodecContext->sample_rate;
}


/* complete */
int CDVDAudioCodecHisi::GetBitsPerSample()
{
  if (!m_pCodecContext)
  {
    return 0;
  }

  return (m_desiredSampleFormat == AV_SAMPLE_FMT_S16)? 16: 32;
}


/* complete */
int CDVDAudioCodecHisi::GetBitRate()
{
  if (!m_pCodecContext)
  {
    return 0;
  }

  return m_pCodecContext->bit_rate;
}


static unsigned count_bits(int64_t value)
{
  unsigned bits = 0;
  for(;value;++bits)
    value &= value - 1;
  return bits;
}


/* complete */
void CDVDAudioCodecHisi::BuildChannelMap()
{
  int64_t layout;

  int bits = count_bits(m_pCodecContext->channel_layout);
  if (bits == m_pCodecContext->channels)
    layout = m_pCodecContext->channel_layout;
  else
  {
    CLog::Log(LOGINFO, "CDVDAudioCodecHisi::GetChannelMap - FFmpeg reported %d channels, but the layout contains %d ignoring", m_pCodecContext->channels, bits);
    layout = av_get_default_channel_layout(m_pCodecContext->channels);
  }

  m_channelLayout.Reset();

  if (layout & AV_CH_FRONT_LEFT           ) m_channelLayout += AE_CH_FL  ;
  if (layout & AV_CH_FRONT_RIGHT          ) m_channelLayout += AE_CH_FR  ;
  if (layout & AV_CH_FRONT_CENTER         ) m_channelLayout += AE_CH_FC  ;
  if (layout & AV_CH_LOW_FREQUENCY        ) m_channelLayout += AE_CH_LFE ;
  if (layout & AV_CH_BACK_LEFT            ) m_channelLayout += AE_CH_BL  ;
  if (layout & AV_CH_BACK_RIGHT           ) m_channelLayout += AE_CH_BR  ;
  if (layout & AV_CH_FRONT_LEFT_OF_CENTER ) m_channelLayout += AE_CH_FLOC;
  if (layout & AV_CH_FRONT_RIGHT_OF_CENTER) m_channelLayout += AE_CH_FROC;
  if (layout & AV_CH_BACK_CENTER          ) m_channelLayout += AE_CH_BC  ;
  if (layout & AV_CH_SIDE_LEFT            ) m_channelLayout += AE_CH_SL  ;
  if (layout & AV_CH_SIDE_RIGHT           ) m_channelLayout += AE_CH_SR  ;
  if (layout & AV_CH_TOP_CENTER           ) m_channelLayout += AE_CH_TC  ;
  if (layout & AV_CH_TOP_FRONT_LEFT       ) m_channelLayout += AE_CH_TFL ;
  if (layout & AV_CH_TOP_FRONT_CENTER     ) m_channelLayout += AE_CH_TFC ;
  if (layout & AV_CH_TOP_FRONT_RIGHT      ) m_channelLayout += AE_CH_TFR ;
  if (layout & AV_CH_TOP_BACK_LEFT        ) m_channelLayout += AE_CH_BL  ;
  if (layout & AV_CH_TOP_BACK_CENTER      ) m_channelLayout += AE_CH_BC  ;
  if (layout & AV_CH_TOP_BACK_RIGHT       ) m_channelLayout += AE_CH_BR  ;
}


/* complete */
CAEChannelInfo CDVDAudioCodecHisi::GetChannelMap()
{
  BuildChannelMap();
  return m_channelLayout;
}







