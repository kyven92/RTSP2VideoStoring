#include "../include/IPCameraWriter.h"

IPCameraWriter::IPCameraWriter()
{
    frameCount = 0;

    av_register_all();
    avcodec_register_all();
    avformat_network_init();
    this->input_ctx = avformat_alloc_context();
    this->dicts = NULL;
    this->output_ctx = NULL;

    this->LOGTYPE = CONSOLE_LOG;
}

int IPCameraWriter::fetchStreamVariables(std::string streamPath)
{
    int rc = av_dict_set(&this->dicts, "rtsp_transport", "tcp", 0); // default udp. Set tcp interleaved mode
    if (rc < 0)
    {
        LOG("[IPCameraWriter][fetchStreamVariables][ERROR]Could not set av dictionary to tcp");
        return 0;
    }

    //open rtsp
    if (avformat_open_input(&this->input_ctx, streamPath.c_str(), NULL, &this->dicts) != 0)
    {

        LOG("[IPCameraWriter][fetchStreamVariables][ERROR]Could not RTSP Stream Path");
        return 0;
    }
    // get context
    if (avformat_find_stream_info(input_ctx, NULL) < 0)
    {

        LOG("[IPCameraWriter][fetchStreamVariables][ERROR]Could not find stream info");
        return 0;
    }
    this->CAMERAFOUND = true;
    return 1;
}

int IPCameraWriter::setupContainerParameters(int FPS, double BITRATE)
{
    if (this->CAMERAFOUND)
    {
        if (FPS > 0 && BITRATE > 0)
        {
            this->FPS = FPS;
            this->BITRATE = BITRATE;

            return 1;
        }
        else
        {
            LOG("[IPCameraWriter][setupContainerParameters][ERROR]Please Check Input arguments");
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

int IPCameraWriter::initiateContainer(std::string fileName)
{
    if (this->CAMERAFOUND)
    {
        if (fileName == "")
        {
            LOG("[IPCameraWriter][initiateContainer][ERROR]File Name for Container is Empty");
            return 0;
        }
        else
        {
            this->fileName = fileName;
            std::cout << "File Name: " << fileName << std::endl;
            if ((avformat_alloc_output_context2(&this->output_ctx, NULL, "mpeg2video", this->fileName.c_str())) >= 0)
            {
                std::cout << "Context Allocation Success" << std::endl;
            }

            // search video stream , audio stream
            std::cout << "NB Stream: " << this->input_ctx->nb_streams << std::endl;
            for (int i = 0; i < this->input_ctx->nb_streams; i++)
            {
                if (this->input_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
                {
                    this->vidx = i;
                    // std::cout << "Found Stream Index: " << vidx << std::endl;
                }
            }

            //open output file
            if (this->output_ctx == NULL)
            {
                std::cout << "[IPCameraWriter][initiateContainer][ERROR]Out put file is not set" << std::endl;
                LOG("[IPCameraWriter][initiateContainer][ERROR]Out put file is not set");
                return 0;
            }
            //Assign codec from input to output
            this->outPutVideoStream = avformat_new_stream(this->output_ctx, this->input_ctx->streams[vidx]->codec->codec);

            //Copy Codec contex of input to output
            avcodec_copy_context(this->outPutVideoStream->codec, this->input_ctx->streams[vidx]->codec);

            this->outPutVideoStream->time_base = (AVRational){1, FPS};
            this->outPutVideoStream->sample_aspect_ratio = this->input_ctx->streams[vidx]->codec->sample_aspect_ratio;
            this->output_ctx->bit_rate = this->BITRATE;

            avio_open(&this->output_ctx->pb, this->fileName.c_str(), AVIO_FLAG_WRITE);
            avformat_write_header(this->output_ctx, NULL);
            return 1;
        }
    }
    else
    {
        return 0;
    }
}

void IPCameraWriter::writeFrames()
{
    if (this->CAMERAFOUND)
    {
        pthread_create(&this->writer_Thread, NULL, this->readFrames, (void *)this);
    }
}

void *IPCameraWriter::readFrames(void *arguments)
{
    int ts = 0;
    int nRecvPacket = 0;

    IPCameraWriter *IPCamObject = (IPCameraWriter *)arguments;
    av_init_packet(&IPCamObject->readBufferPacket);

    while (!IPCamObject->stopStreamRead)
    {
        if (IPCamObject->CAMERAFOUND)
        {
            nRecvPacket = av_read_frame(IPCamObject->input_ctx, &IPCamObject->readBufferPacket);

            if (nRecvPacket >= 0)
            {
                if (IPCamObject->readBufferPacket.stream_index == IPCamObject->vidx) // video frame
                {
                    // packet.stream_index = vidx;

                    IPCamObject->readBufferPacket.pts = ts;
                    IPCamObject->readBufferPacket.dts = ts;

                    IPCamObject->outPutVideoStream->id = IPCamObject->vidx;
                    if (IPCamObject->readBufferPacket.pts != AV_NOPTS_VALUE)
                    {
                        IPCamObject->readBufferPacket.duration = av_rescale_q(IPCamObject->readBufferPacket.duration, IPCamObject->input_ctx->streams[IPCamObject->readBufferPacket.stream_index]->time_base, IPCamObject->outPutVideoStream->time_base);
                        ts += IPCamObject->readBufferPacket.duration;

                        IPCamObject->readBufferPacket.pos = -1;
                    }
                    IPCamObject->frameCount++;
                    std::cout << "Frame Number: " << IPCamObject->frameCount++ << std::endl;

                    if ((av_write_frame(IPCamObject->output_ctx, &IPCamObject->readBufferPacket)) < 0)
                    {
                        av_packet_unref(&IPCamObject->readBufferPacket);
                        break;
                    }
                }

                av_packet_unref(&IPCamObject->readBufferPacket);
            }
            else
            {
                std::cout << "No Frame Received" << std::endl;
            }
        }
    }
}

void IPCameraWriter::dumpTrailer()
{
    if (this->CAMERAFOUND)
    {
        av_write_trailer(this->output_ctx);
        avio_close(this->output_ctx->pb);
        avformat_free_context(this->output_ctx);
        av_dict_free(&this->dicts);
    }
}

bool IPCameraWriter::getWriterStatus()
{
    return this->stopStreamRead;
}

void IPCameraWriter::stopWriter()
{
    if (!this->stopStreamRead)
    {
        this->stopStreamRead = true;
    }
}
template <class T>
void IPCameraWriter::LOG(T logData)
{
    if (this->LOGTYPE = CONSOLE_LOG)
    {
        std::cout << logData << std::endl;
    }
    else
    {
    }
}