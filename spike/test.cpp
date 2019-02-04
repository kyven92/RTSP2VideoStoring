#include <iostream>
#include <unistd.h>
#include <signal.h>

#include "../include/IPCameraWriter.h"

int FPS = 30;
double BITRATE = 2000000;

bool stopProgram = false;

void shutDown()
{
    stopProgram = true;
}

void recv_signal(int s)
{

    std::cout << "[Info][Vision][Signal Handler] Signal Recieved is " << s
              << std::endl;
    if (s == SIGPIPE)
    {
        // Just ignore, sock class will close the client the next time
    }
    // Catch ctrl-c, segmentation fault, abort signal
    if (s == SIGINT || s == SIGSEGV || s == SIGABRT || s == SIGTSTP ||
        s == SIGKILL || s == SIGTERM || s == SIGHUP)
    {
        shutDown();
        std::cout << "Calling Shutdown" << std::endl;
    }
}

void initSigHandler()
{

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = recv_signal;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    sigaction(SIGTSTP, &sigIntHandler, NULL);
    sigaction(SIGPIPE, &sigIntHandler, NULL);
    sigaction(SIGABRT, &sigIntHandler, NULL);
    sigaction(SIGSEGV, &sigIntHandler, NULL);
    sigaction(SIGKILL, &sigIntHandler, NULL);
    sigaction(SIGTERM, &sigIntHandler, NULL);
    sigaction(SIGHUP, &sigIntHandler, NULL);
}

int main(int argc, char **argv)
{
    initSigHandler();

    IPCameraWriter testCamera;
    if (testCamera.fetchStreamVariables("rtsp://admin:planysch4pn@192.168.0.102:554"))
    {
        testCamera.setupContainerParameters(30, 1000000);
        testCamera.initiateContainer("http://localhost:8082/q/1280/720");
        testCamera.writeFrames();
    }
    else
    {
        std::cout << "Camera Not Detected" << std::endl;
    }

    while (!stopProgram)
    {
        usleep(100000);
    }

    testCamera.stopWriter();
    usleep(100000);
    testCamera.dumpTrailer();

    std::cout << "Waiting to exit Child Threads" << std::endl;
    if (testCamera.CAMERAFOUND)
    {
        pthread_join(testCamera.writer_Thread, NULL);
    }
    std::cout << "Exiting Main" << std::endl;
}