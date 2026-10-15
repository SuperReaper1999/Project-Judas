#pragma once
#include <android/log.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <string>
#include <thread>
// Bounded line bridge for the engine's existing diagnostics. No UI/JNI/graphics
// calls leave the owner thread. Android otherwise discards native stdout/stderr.
class AndroidLogBridge {
    int output=-1,error=-1;
    std::thread reader;
public:
    AndroidLogBridge(){
        int pipeEnds[2];if(pipe(pipeEnds))return;
        output=dup(STDOUT_FILENO);error=dup(STDERR_FILENO);
        if(output<0||error<0){close(pipeEnds[0]);close(pipeEnds[1]);if(output>=0)close(output);if(error>=0)close(error);output=error=-1;return;}
        dup2(pipeEnds[1],STDOUT_FILENO);dup2(pipeEnds[1],STDERR_FILENO);close(pipeEnds[1]);
        setvbuf(stdout,nullptr,_IOLBF,0);setvbuf(stderr,nullptr,_IOLBF,0);
        reader=std::thread([fd=pipeEnds[0]]{
            std::string line;char buffer[512];
            for(;;){auto n=read(fd,buffer,sizeof(buffer));if(n<0&&errno==EINTR)continue;if(n<=0)break;
                for(ssize_t i=0;i<n;++i){if(buffer[i]=='\n'||line.size()==3000){__android_log_write(ANDROID_LOG_INFO,"Judas.Native",line.c_str());line.clear();}if(buffer[i]!='\n')line+=buffer[i];}}
            if(!line.empty())__android_log_write(ANDROID_LOG_INFO,"Judas.Native",line.c_str());close(fd);
        });
    }
    ~AndroidLogBridge(){if(output<0)return;fflush(stdout);fflush(stderr);dup2(output,STDOUT_FILENO);dup2(error,STDERR_FILENO);close(output);close(error);if(reader.joinable())reader.join();}
};
