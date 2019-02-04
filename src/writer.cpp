/*
 * converter and decoder operate on unscaled data
 * encoder operates on scaled data
 * scaling down can be faster as time to encode video decreases
 */

#include "writer.h"

// INITIALIZE_EASYLOGGINGPP
// el::Logger* defaultLogger_writer = el::Loggers::getLogger("default");

using namespace std;

writer::writer()
{

  frameCount = 0;
  context = NULL;
  source = NULL;
  scale = false;
  status = VIDEOWRITER_STATUS_DEFAULT;

  av_register_all();
  avcodec_register_all();
  avdevice_register_all();
  ////LOG(INFO) << "writer : registered with FFMPEG libs";
}

writer::~writer()
{
  //frameCount = 0;
}

void writer::writeFrame(unsigned char *data, int len)
{

  if (status == VIDEOWRITER_STATUS_OK)
  {
    if (camformat == V4L2_PIX_FMT_YUYV)
    {
      // YUYV422 to YUV420P: Method 1
      // takes nearly 10ms per frame
      //
      // YUV422toYUV420(data, frame, width, height);

      // YUYV422 to YUV420P: Method 2
      // takes nearly 1 ms per frame

      uint8_t *inData[1] = {data};
      const int inLinesize[1] = {2 * unscaled_width};
      sws_scale(conversionContext, inData, inLinesize, 0, unscaled_height,
                frame->data, frame->linesize);
    }
    else if (camformat == V4L2_PIX_FMT_MJPEG)
    {
      // Decode JPEG data to YUV420P in frame

      // struct timeval tv1, tv2;
      // gettimeofday(&tv1, NULL);

      decodeMJPEG(data, len);

      // int sz = 1843200;
      // unsigned char *t = (unsigned char *)malloc(sz);
      // MJPEGtoYUV420P(t, data, width, height, sz);
      // frame->data[0] = t;
      // frame->data[1] = frame->data[0] + width * height;
      // frame->data[2] = frame->data[1] + (width * height) / 4;
      //

      // gettimeofday(&tv2, NULL);
      // printf("decode time: %lu\n", tv2.tv_usec - tv1.tv_usec);
    }

    if (scale)
    {
      // Scale frame
      sws_scale(scaleContext, frame->data, frame->linesize, 0, unscaled_height,
                scaled_frame->data, scaled_frame->linesize);

      // Write frame data
      encodeFrame(scaled_frame);
    }
    else
    {
      // Write frame data
      encodeFrame(frame);
    }

    // free(t);
  }
  else
  {
    if (status == VIDEOWRITER_STATUS_DEFAULT)
    {
      // LOG(ERROR) << " writeFrame: VideoWriter unintialized ";
    }
    else if (status == VIDEOWRITER_STATUS_ERR)
    {
      // LOG(ERROR) << " writeFrame: VideoWriter in error state ";
    }
    else if (status == VIDEOWRITER_STATUS_CLOSED)
    {
      // LOG(ERROR) << " writeFrame: VideoWriter in closed state ";
    }

    ////LOG(logStr, LOG_ERROR);
  }
}

void writer::writeFrame(AVPacket inputPacket)
{
  encodeFrame(inputPacket);
}

void writer::destroy()
{

  //frameCount = 0;
  if (status == VIDEOWRITER_STATUS_OK)
  {

    // LOG(INFO)<<" releasing video encoder, decoder, converter and scaler
    // resources ";
    // sprintf(logStr, "releasing video encoder, decoder, converter and scaler
    // resources");
    ////LOG(logStr, LOG_INFO);

    /* release encoder resources */
    uint8_t endcode[] = {0x00, 0x00, 0x01, 0xb7};

    avcodec_close(context);
    av_free(context);
    av_freep(&frame->data[0]);
    av_frame_free(&frame);

    /* release decoder resources */
    avcodec_close(dec_context);
    av_free(dec_context);
    av_frame_free(&dec_frame);

    /* release converter resources */
    sws_freeContext(conversionContext);

    /* release scaler resources */
    if (scale)
    {
      sws_freeContext(scaleContext);
      av_frame_free(&scaled_frame);
    }

    status = VIDEOWRITER_STATUS_CLOSED;
  }
}

void writer::close() { destroy(); }

void writer::encodeFrame(AVFrame *opt_frame)
{

  int got_output = 0;
  av_init_packet(&pkt);
  pkt.data = NULL;
  pkt.size = 0;

  opt_frame->width = scaled_width;
  opt_frame->height = scaled_height;
  opt_frame->pts = frameCount;

  opt_frame->format = AV_PIX_FMT_YUV420P;

  if (avcodec_encode_video2(context, &pkt, opt_frame, &got_output) < 0)
  {
    // LOG(ERROR) << " error encoding frame ";
    status = VIDEOWRITER_STATUS_ERR;
  }

  if (got_output)
  {

    // defaultLogger_writer->info("Write Frame  : %v size %v", frameCount+1 ,
    // pkt.size/1000 );

    if (pkt.pts != AV_NOPTS_VALUE)
      pkt.pts = av_rescale_q(pkt.pts, video_st->codec->time_base,
                             video_st->time_base);
    if (pkt.dts != AV_NOPTS_VALUE)
      pkt.dts = av_rescale_q(pkt.dts, video_st->codec->time_base,
                             video_st->time_base);

    // av_write_frame Returns < 0 on error, = 0 if OK, 1 if flushed and there is
    // no more data to flush
    if (av_write_frame(outAVFormatContext, &pkt) != 0)
    {
      // LOG(ERROR) << "av_write_frame() ";
    }

    // fwrite(pkt.data, 1, pkt.size, fp);
    av_packet_unref(&pkt);
  }

  frameCount++;
}

void writer::encodeFrame(AVPacket writePacket)
{
  pkt = writePacket;
  pkt.pts = frameCount;

  // defaultLogger_writer->info("Write Frame  : %v size %v", frameCount+1 ,
  // pkt.size/1000 );

  if (pkt.pts != AV_NOPTS_VALUE)
    pkt.pts = av_rescale_q(pkt.pts, video_st->codec->time_base,
                           video_st->time_base);
  if (pkt.dts != AV_NOPTS_VALUE)
    pkt.dts = pkt.pts;

  // av_write_frame Returns < 0 on error, = 0 if OK, 1 if flushed and there is
  // no more data to flush
  if (av_write_frame(outAVFormatContext, &pkt) != 0)
  {
    // LOG(ERROR) << "av_write_frame() ";
  }

  // fwrite(pkt.data, 1, pkt.size, fp);
  av_packet_unref(&pkt);

  frameCount++;
}

void writer::dumpTrailer()
{

  value = av_write_trailer(outAVFormatContext);
  if (value < 0)
  {
    // LOG(ERROR) << "av_Write_trailer()";
  }
  else
  {
    // LOG(INFO) << "av_Write_trailer()";
  }
}

void writer::decodeMJPEG(unsigned char *data, int len)
{

  int got_picture = 0;

  av_init_packet(&dec_pkt);
  dec_pkt.size = len;
  dec_pkt.data = data;

  dec_frame->width = unscaled_width;
  dec_frame->height = unscaled_height;
  dec_frame->format = AV_PIX_FMT_YUV422P;

  if (avcodec_decode_video2(dec_context, dec_frame, &got_picture, &dec_pkt) <
      0)
  {
    // LOG(ERROR) << " error decoding frame ";
    status = VIDEOWRITER_STATUS_ERR;
  }

  if (got_picture)
  {
    /* Convert YUVJ422P to YUV420P */
    conversionContext = sws_getContext(
        unscaled_width, unscaled_height, AV_PIX_FMT_YUV422P, unscaled_width,
        unscaled_height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    sws_scale(conversionContext, dec_frame->data, dec_frame->linesize, 0,
              unscaled_height, frame->data, frame->linesize);
  }
}

void writer::initialize(char *opt_fname, AVCodecID opt_cid, int opt_usc_wd,
                        int opt_usc_ht, int opt_sc_wd, int opt_sc_ht,
                        int opt_fps, int opt_br, int opt_gop,
                        AVPixelFormat opt_pFmt, int opt_mbf,
                        unsigned int opt_camfmt, char *opt_src)
{

  source = (char *)malloc(sizeof(opt_src));
  sprintf(source, "%s", opt_src);
  initialize(opt_fname, opt_cid, opt_usc_wd, opt_usc_ht, opt_sc_wd, opt_sc_ht,
             opt_fps, opt_br, opt_gop, opt_pFmt, opt_mbf, opt_camfmt);
}

void writer::initialize(char *opt_fname, AVCodecID opt_cid, int opt_usc_wd,
                        int opt_usc_ht, int opt_sc_wd, int opt_sc_ht,
                        int opt_fps, int opt_br, int opt_gop,
                        AVPixelFormat opt_pFmt, int opt_mbf,
                        unsigned int opt_camfmt)
{

  sprintf(fileName, "%s", opt_fname);
  codecId = opt_cid;
  fps = opt_fps;
  bitrate = opt_br;
  gopsize = opt_gop;
  pixFmt = opt_pFmt;
  maxbframes = opt_mbf;
  camformat = opt_camfmt;

  if ((opt_usc_wd != opt_sc_wd) || (opt_usc_ht != opt_sc_ht))
  {
    unscaled_width = opt_usc_wd;
    unscaled_height = opt_usc_ht;

    scaled_width = opt_sc_wd;
    scaled_height = opt_sc_ht;

    initScaler();
  }
  else
  {
    unscaled_width = opt_sc_wd;
    unscaled_height = opt_sc_ht;

    scaled_width = unscaled_width;
    scaled_height = unscaled_height;
  }

  // LOG(INFO) << " encoding video file ";

  if (source)
  {
    // sprintf(logStr, "%s from source %s", logStr, source);
    // LOG(INFO) << " from source "; // |++++++++++++++++++++
  }
  //	//LOG(logStr, LOG_INFO);

  initEncoder();
  //initDecoder(AV_CODEC_ID_MPEG4, AV_PIX_FMT_YUV422P);
  initConverter();
}

void writer::initConverter()
{
  /*
   * Initialize conversion context
   */

  conversionContext = sws_getContext(
      unscaled_width, unscaled_height, AV_PIX_FMT_YUYV422, unscaled_width,
      unscaled_height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
}

void writer::initDecoder(AVCodecID opt_decId, AVPixelFormat opt_pxfmt)
{
  /*
   * Initialize decoder
   */

  dec_codec = avcodec_find_decoder(opt_decId);
  if (!dec_codec)
  {
    // LOG(ERROR) << " decoder not found ";
    status = VIDEOWRITER_STATUS_ERR;
    return;
  }

  dec_context = avcodec_alloc_context3(dec_codec);
  if (!dec_context)
  {

    // LOG(ERROR) << " could not allocate video decoder context ";
    status = VIDEOWRITER_STATUS_ERR;
    return;
  }

  avcodec_get_context_defaults3(dec_context, dec_codec);
  /*
  avcodec_get_context_defaults3 :
  Set the fields of the given AVCodecContext to default values corresponding to
  the given codec (defaults may be codec-dependent).
  */
  dec_context->width = unscaled_width;
  dec_context->height = unscaled_height;
  dec_context->pix_fmt = opt_pxfmt;

  if (avcodec_open2(dec_context, dec_codec, NULL) < 0)
  {

    // LOG(ERROR) << " could not open decoder ";
    status = VIDEOWRITER_STATUS_ERR;
    return;
  }

  dec_frame = av_frame_alloc();

  if (!dec_frame)
  {

    // LOG(ERROR) << " could not allocate decoder video frame ";
    status = VIDEOWRITER_STATUS_ERR;
    return;
  }

  if (av_image_alloc(dec_frame->data, dec_frame->linesize, dec_context->width,
                     dec_context->height, dec_context->pix_fmt, 16) < 0)
  {

    // LOG(ERROR) << " could not allocate decoder raw picture buffer ";
    status = VIDEOWRITER_STATUS_ERR;
    return;
  }

  //	if(dec_codec->capabilities & CODEC_CAP_TRUNCATED)
  //    {
  //        dec_context->flags |= CODEC_FLAG_TRUNCATED;
  //    }
}

void writer::initContainer()
{

  outAVFormatContext = NULL;

  avformat_alloc_output_context2(&outAVFormatContext, NULL, NULL, fileName);
  if (!outAVFormatContext)
  {
    // LOG(ERROR)<<" avformat_alloc_output_context2() ";
    return;
  }
  else
  {
    // LOG(INFO)<<" avformat_alloc_output_context2() ";
  }

  output_format = av_guess_format(NULL, fileName, NULL);
  if (!output_format)
  {

    // LOG(ERROR)<<" av_guess_format() ";
    return;
  }
  else
  {
    // LOG(INFO)<<" av_guess_format() ";
  }

  outAVFormatContext->oformat = output_format;

  video_st = avformat_new_stream(outAVFormatContext, NULL);
  if (!video_st)
  {

    // LOG(ERROR)<<" avformat_new_stream() ";
    //  return -1;
  }
  else
  {
    // LOG(INFO)<<" avformat_new_stream() ";
  }
  if (fps > 40)
  {
    video_st->codec->time_base = (AVRational){1, fps};
    video_st->time_base = (AVRational){1, 90000};
  }
  else
  {
    video_st->codec->time_base = (AVRational){1, fps};
    video_st->time_base = (AVRational){1, 35000};
  }
}

void writer::makeContainer()
{

  if (!(outAVFormatContext->flags & AVFMT_NOFILE))
  {
    if (avio_open2(&outAVFormatContext->pb, fileName, AVIO_FLAG_WRITE, NULL,
                   NULL) < 0)
    {
      // LOG(ERROR)<<" avio_open2() ";
    }
    else
    {
      // LOG(INFO)<<" avio_open2() ";
    }
  }

  if (!outAVFormatContext->nb_streams)
  {
    // LOG(ERROR)<<" Output file dose not contain any stream ";
    // return -1;
  }

  value = avformat_write_header(outAVFormatContext, NULL);
  if (value < 0)
  {
    // LOG(ERROR)<<" avformat_write_header() ";
    // return -1;
  }
  else
  {
    // LOG(INFO)<<" avformat_write_header() ";
  }

  // av_dump_format(outAVFormatContext , 0 ,output_file ,1);
}

void writer::initEncoder()
{

  /* initialize container */
  initContainer();

  /*
   * Initialize encoder
   */

  codec = avcodec_find_encoder(codecId);
  if (!codec)
  {

    // LOG(ERROR)<<" codec not found ";
    status = VIDEOWRITER_STATUS_ERR;
    return;
  }
  else
  {
    // LOG(INFO)<<" avcodec_find_encoder() ";
  }

  context = avcodec_alloc_context3(codec);
  if (!context)
  {

    // LOG(ERROR)<<" could not allocate video codec context ";
    status = VIDEOWRITER_STATUS_ERR;
    return;
  }
  else
  {
    // LOG(INFO)<<" avcodec_alloc_context3() ";
  }
  context = video_st->codec; // from kabali
  context->bit_rate = bitrate;
  context->width = scaled_width;
  context->height = scaled_height;
  context->time_base = (AVRational){1, fps};
  // context->time_base.num = 1;
  // context->time_base.den = 30;
  context->gop_size = gopsize;
  context->pix_fmt = pixFmt;

  if (maxbframes != NULL)
    context->max_b_frames = maxbframes;

  if (codecId == AV_CODEC_ID_H264)
  {
    av_opt_set(context->priv_data, "preset", "slow", AV_OPT_SEARCH_CHILDREN);
    // av_opt_set(context->priv_data, "profile", 	"baseline",
    // AV_OPT_SEARCH_CHILDREN);
    // av_opt_set(context->priv_data, "level", 		"3.0",
    // AV_OPT_SEARCH_CHILDREN);
    // av_opt_set(context->priv_data, "crf",  		"18",
    // AV_OPT_SEARCH_CHILDREN);
  }

  if (outAVFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
  {
    context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  if (avcodec_open2(context, codec, NULL) < 0)
  {
    // LOG(ERROR)<<" could not open codec ";
    status = VIDEOWRITER_STATUS_ERR;
    return;
  }
  else
  {
    // LOG(INFO)<<" avcodec_open2() ";
  }

  makeContainer();

  frame = av_frame_alloc();

  if (!frame)
  {
    // LOG(ERROR)<<" could not allocate video frame ";
    status = VIDEOWRITER_STATUS_ERR;
    return;
  }

  if (av_image_alloc(frame->data, frame->linesize, unscaled_width,
                     unscaled_height, context->pix_fmt, 32) < 0)
  {

    // LOG(ERROR)<<" could not allocate raw picture buffer ";
    status = VIDEOWRITER_STATUS_ERR;
    return;
  }

  status = VIDEOWRITER_STATUS_OK;
}

void writer::initScaler()
{

  /*
   * Initialize scaler context
   */
  scaleContext =
      sws_getContext(unscaled_width, unscaled_height, pixFmt, scaled_width,
                     scaled_height, pixFmt, SWS_BICUBIC, NULL, NULL, NULL);

  /*
   * Allocate frame data
   */
  scaled_frame = av_frame_alloc();

  if (!scaled_frame)
  {
    // LOG(ERROR)<<" could not allocate scaler video frame ";
    status = VIDEOWRITER_STATUS_ERR;
    return;
  }
  // int scaled_size = avpicture_get_size(pixFmt, scaled_width, scaled_height);
  // // deprecated
  /*
-    utv->buf_size = avpicture_get_size(avctx->pix_fmt, avctx->width,
avctx->height);
+    utv->buf_size = av_image_get_buffer_size(avctx->pix_fmt, avctx->width,
avctx->height, 1);
  */

  int scaled_size =
      av_image_get_buffer_size(pixFmt, scaled_width, scaled_height, 16);

  unsigned char *temp_buffer =
      (unsigned char *)av_malloc(scaled_size * sizeof(uint8_t));
  //	avpicture_fill((AVPicture *)scaled_frame, temp_buffer, pixFmt,
  // scaled_width, scaled_height);

  uint8_t *video_outbuf = (uint8_t *)av_malloc(scaled_size);
  if (video_outbuf == NULL)
  {
    // LOG(ERROR)<<" av_malloc() ";
  }
  else
  {
    // LOG(INFO)<<"av_malloc()";
  }

  value =
      av_image_fill_arrays(scaled_frame->data, scaled_frame->linesize,
                           video_outbuf, pixFmt, scaled_height, scaled_width,
                           16); // returns : the size in bytes required for src

  // sprintf(logStr, "initialized scaler to scale from %d x %d to %d x %d",
  // unscaled_width, unscaled_height, scaled_width, scaled_height);
  // LOG(INFO)<<" initialized scaler to scale from "; //
  // ++++++++++++++++++++++++

  scale = true;
}
