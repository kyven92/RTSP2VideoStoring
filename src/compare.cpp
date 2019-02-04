#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <iostream>

#include "writer.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavdevice/avdevice.h>
#include <libavutil/timestamp.h>
}
int FPS = 25;
writer ipWriter;

int main(void)
{
    //avdevice_register_all();
    av_register_all();
    avcodec_register_all();
    avformat_network_init();
    std::string filename = "./output.avi";

    // Initialize

    int vidx = 0,
        aidx = 0; // Video, Audio Index
    AVFormatContext *input_ctx = avformat_alloc_context();
    AVDictionary *dicts = NULL;
    AVFormatContext *output_ctx = NULL;
    (
        avformat_alloc_output_context2(&output_ctx, NULL, NULL, filename.c_str()));

    int rc = av_dict_set(&dicts, "rtsp_transport", "tcp", 0); // default udp. Set tcp interleaved mode
    if (rc < 0)
    {
        return EXIT_FAILURE;
    }

    /*
    rc = av_dict_set(&dicts, "stimeout", "1 * 1000 * 1000", 0);
    if (rc < 0)
    {
        printf("errrrrrrr    %d\n", rc);
        return -1;
    }
    */

    //open rtsp
    if (avformat_open_input(&input_ctx, "rtsp://admin:planysch4pn@192.168.0.100:554", NULL, &dicts) != 0)
    {
        return EXIT_FAILURE;
    }

    // get context
    if (avformat_find_stream_info(input_ctx, NULL) < 0)
    {
        return EXIT_FAILURE;
    }

    //search video stream , audio stream
    for (int i = 0; i < input_ctx->nb_streams; i++)
    {
        if (input_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            vidx = i;
        // if (input_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        //     aidx = i;
    }

    AVPacket packet;

    //open output file
    if (output_ctx == NULL)
    {
        printf("failed\n");
        return EXIT_FAILURE;
    }

    AVStream *vstream = NULL;
    // AVStream *astream = NULL;

    vstream = avformat_new_stream(output_ctx, input_ctx->streams[vidx]->codec->codec);
    // astream = avformat_new_stream(output_ctx, input_ctx->streams[aidx]->codec->codec);

    avcodec_copy_context(vstream->codec, input_ctx->streams[vidx]->codec);
    vstream->time_base = (AVRational){1, FPS};
    vstream->sample_aspect_ratio = input_ctx->streams[vidx]->codec->sample_aspect_ratio;
    // avcodec_parameters_copy(output_ctx->codecpar, input_ctx->codecpar);

    // avcodec_copy_context(astream->codec, input_ctx->streams[aidx]->codec);
    // astream->time_base = (AVRational){1, 16000};

    int cnt = 0;
    long long nGap = 0;
    long long nGap2 = 1;

    av_read_play(input_ctx); //play RTSP

    avio_open(&output_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE);
    avformat_write_header(output_ctx, NULL);
    int ts = 0;

    while (true)
    {
        av_init_packet(&packet);
        int nRecvPacket = av_read_frame(input_ctx, &packet);

        if (nRecvPacket >= 0)
        {
            if (packet.stream_index == vidx) // video frame
            {

                vstream->id = vidx;
                if (packet.pts != AV_NOPTS_VALUE)
                {
                    packet.pts = av_rescale_q(packet.pts, input_ctx->streams[packet.stream_index]->time_base, vstream->time_base);
                }
                // nGap += 333;
                cnt++;
                std::cout << "Frame Number: " << cnt << std::endl;
            }

            packet.dts = packet.pts; // generally, dts is same as pts. it only differ when the stream has b-frame
            av_write_frame(output_ctx, &packet);
            av_packet_unref(&packet);
        }

        // if (cnt > 300) // 10 sec
        //     break;
    }

    av_read_pause(input_ctx);
    av_write_trailer(output_ctx);
    avio_close(output_ctx->pb);
    avformat_free_context(output_ctx);
    av_dict_free(&dicts);

    return (EXIT_SUCCESS);
}
