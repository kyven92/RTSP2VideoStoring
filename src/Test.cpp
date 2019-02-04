#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <iostream>

extern "C"
{
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavdevice/avdevice.h>
#include <libavutil/timestamp.h>
}

std::string filename = "./output.avi";
long long frameCount = 0;
AVPacket pkt;
AVCodecContext *pCodecCtx;
int status;
AVStream *video_st = NULL;
AVFormatContext *outAVFormatContext;

void encodeFrame(AVFrame *opt_frame)
{

    int got_output = 0;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    opt_frame->width = 1280;
    opt_frame->height = 720;
    opt_frame->pts = frameCount;

    opt_frame->format = AV_PIX_FMT_YUV420P;

    if (avcodec_encode_video2(pCodecCtx, &pkt, opt_frame, &got_output) < 0)
    {
        // LOG(ERROR) << " error encoding frame ";
        status = 2;
    }
    int ret = avformat_write_header(outAVFormatContext, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
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
int main(int argc, char *argv[])
{

    av_register_all();
    avdevice_register_all();
    avcodec_register_all();
    avformat_network_init();

    avformat_alloc_output_context2(&outAVFormatContext, NULL, NULL, filename.c_str());

    const char *filenameSrc = "rtsp://admin:planysch4pn@192.168.0.125:554";

    // AVCodecContext *pCodecCtx;
    AVFormatContext *pFormatCtx = avformat_alloc_context();

    AVCodec *pCodec;
    AVFrame *pFrame, *pFrameRGB;

    if (avformat_open_input(&pFormatCtx, filenameSrc, NULL, NULL) != 0)
        return -12;

    av_dump_format(pFormatCtx, 0, filenameSrc, 0);
    int videoStream = 1;
    for (int i = 0; i < pFormatCtx->nb_streams; i++)

    {
        if (pFormatCtx->streams[i]->codec->coder_type == AVMEDIA_TYPE_VIDEO)

        {
            videoStream = i;
            std::cout << "Video Stream Found: " << videoStream << std::endl;
            break;
        }
    }

    if (videoStream == -1)
        return -14;
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;

    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL)
        return -15; //codec not found

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
        return -16;

    pFrame = avcodec_alloc_frame();
    pFrameRGB = avcodec_alloc_frame();

    uint8_t *buffer;
    int numBytes;

    AVPixelFormat pFormat = AV_PIX_FMT_YUV420P;
    // numBytes = avpicture_get_size(pFormat, pCodecCtx->width, pCodecCtx->height); //AV_PIX_FMT_RGB24
    // std::cout << "No Of Bytes: " << numBytes << std::endl;
    // buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    // avpicture_fill((AVPicture *)pFrameRGB, buffer, pFormat, pCodecCtx->width, pCodecCtx->height);

    int res;
    int frameFinished;
    AVPacket packet;
    while (res = av_read_frame(pFormatCtx, &packet) >= 0)

    {

        if (packet.stream_index == videoStream)
        {

            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            if (frameFinished)
            {

                // struct SwsContext *img_convert_ctx;
                // img_convert_ctx = sws_getCachedContext(NULL, pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
                // sws_scale(img_convert_ctx, (pFrame)->data, (pFrame)->linesize, 0, pCodecCtx->height, (pFrameRGB)->data, (pFrameRGB)->linesize);

                encodeFrame(pFrame);

                av_free_packet(&packet);
                // sws_freeContext(img_convert_ctx);
            }
        }
        frameCount++;
    }

    av_free_packet(&packet);

    avcodec_close(pCodecCtx);
    av_free(pFrame);
    av_free(pFrameRGB);
    avformat_close_input(&pFormatCtx);

    return 0;
}