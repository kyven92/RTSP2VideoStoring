#ifndef WRITER_H
#define WRITER_H

#include <iostream>
#include <linux/videodev2.h>

//FFMPEG LIBRARIES
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavcodec/avfft.h"

#include "libavdevice/avdevice.h"

#include "libavfilter/avfilter.h"
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"

#include "libavformat/avformat.h"
#include "libavformat/avio.h"

	// libav resample

#include "libavutil/opt.h"
#include "libavutil/common.h"
#include "libavutil/channel_layout.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/file.h"

	// lib swresample

#include "libswscale/swscale.h"
}

//#include "Config.h"
//#include "Logger.h"
//#include "Utility.h"

#define VIDEOWRITER_STATUS_DEFAULT 0
#define VIDEOWRITER_STATUS_OK 1
#define VIDEOWRITER_STATUS_ERR 2
#define VIDEOWRITER_STATUS_CLOSED 3

class writer
{
  private:
	char *source;
	char fileName[512];
	const char *output_file;
	int unscaled_width;
	int unscaled_height;
	int scaled_width;
	int scaled_height;
	int bitrate;
	int fps;
	int gopsize;
	int maxbframes;
	char logStr[200];
	//Logger logger;
	FILE *fp;
	int status;
	unsigned int camformat;
	AVPixelFormat pixFmt;

	/* encoder variables */
	AVCodecID codecId;
	AVFrame *frame;
	AVPacket pkt;
	AVCodecContext *context;
	AVCodec *codec;

	/* container variables */
	int value;
	AVFormatContext *outAVFormatContext;
	AVOutputFormat *output_format;
	AVStream *video_st;

	/* decoder variables */
	AVCodec *dec_codec;
	AVCodecContext *dec_context;
	AVFrame *dec_frame;
	AVPacket dec_pkt;

	/* conversion variables */
	SwsContext *conversionContext;

	/* scale variables */
	AVFrame *scaled_frame;
	SwsContext *scaleContext;
	bool scale;

	//void log(char[], int);
	void writeDelayedFrames();
	void destroy();
	void initEncoder();

	void initContainer();
	void makeContainer();

	void initDecoder(AVCodecID, AVPixelFormat);
	void initConverter();
	void initScaler();
	void decodeMJPEG(unsigned char *, int);
	void encodeFrame(AVFrame *);
	void encodeFrame(AVPacket writePacket);

  public:
	int inputSize;
	long long frameCount;

	writer();
	~writer();
	void flow_continue();

	void initialize(char[], AVCodecID, int, int, int, int, int, int, int, AVPixelFormat, int, unsigned int);
	void initialize(char[], AVCodecID, int, int, int, int, int, int, int, AVPixelFormat, int, unsigned int, char[]);
	void writeFrame(unsigned char *, int);
	void writeFrame(AVPacket inputPacket);
	/* conversion variables */
	void dumpTrailer();

	void close();
};

#endif
