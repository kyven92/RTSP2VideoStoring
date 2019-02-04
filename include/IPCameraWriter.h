#ifndef IPCAMERAWRITER_H
#define IPCAMERAWRITER_H

#include <iostream>
#include <pthread.h>

/***
 * Log type defines
 * 
 */
#define DEFAULT 1
#define FILE_LOG 1
#define CONSOLE_LOG 2

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavdevice/avdevice.h>
#include <libavutil/timestamp.h>
}

class IPCameraWriter
{
private:
  int vidx = 0, aidx = 0; // Video, Audio Index
  std::string fileName;
  bool stopStreamRead = false;

  int FPS;
  double BITRATE;

  AVFormatContext *input_ctx;
  AVDictionary *dicts;
  AVFormatContext *output_ctx;
  AVPacket readBufferPacket;
  AVStream *outPutVideoStream;

public:
  int LOGTYPE;
  long frameCount;

  bool CAMERAFOUND = false;

  pthread_t writer_Thread;

  IPCameraWriter();

  int fetchStreamVariables(std::string streamPath);

  int setupContainerParameters(int FPS, double BITRATE);
  int initiateContainer(std::string fileName);

  void writeFrames();
  static void *readFrames(void *arguments);

  void dumpTrailer();

  bool getWriterStatus();
  void stopWriter();

  template <class T>
  void LOG(T logData);
};

#endif