#pragma once
// Backend-private, prepared PCM source. Only AudioSystem sees miniaudio types.
#include "miniaudio.h"
#include "JobSystem.h"
#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
struct AudioStreamMetadata {
    const std::string path;
    std::atomic<std::uint64_t> length{0}; // zero means unknown, never fabricated duration
    explicit AudioStreamMetadata(std::string p):path(std::move(p)){}
};
struct PreparedAudioStream {
    ma_data_source_base base{}; // required first member for miniaudio's data-source ABI
    struct Page {
        std::atomic<int> state{0}; // free / writing / ready / reading
        std::uint64_t generation=0,sequence=0,start=0;
        unsigned frames=0;
        bool end=false;
        std::vector<float> pcm;
    };
    std::array<Page,4> pages;
    std::shared_ptr<AudioStreamMetadata> metadata;
    std::atomic<std::uint64_t> readyGeneration{0},generation{1},seekFrame{0},cursor{0},underruns{0},decodedFrames{0};
    std::atomic<bool> ready{false},starved{false},failed{false},retired{false},loop{false},ended{false};
    std::atomic<unsigned> delayMilliseconds{0}; // deterministic narrow starvation fixture
    unsigned pageFrames=4096;
    std::string error; // read only after failed acquire / completed job
    ma_decoder decoder{};bool decoderReady=false,workerAtEnd=false;
    std::uint64_t workerGeneration=0,workerSequence=0;
    std::atomic<std::uint64_t> audioGeneration{0};
    std::uint64_t audioSequence=0;
    int reading=-1;unsigned offset=0;
    JobHandle job;
    explicit PreparedAudioStream(std::shared_ptr<AudioStreamMetadata>,unsigned frames);
    ~PreparedAudioStream();
    void Fill(JobContext& context); // ordered jobs, exactly one worker at a time
    void Close();                 // idempotent cleanup after graph detachment and decode completion
    void Seek(std::uint64_t frame);
    std::size_t BufferBytes()const{return std::size_t(pageFrames)*4*2*sizeof(float);}
    static ma_result Read(ma_data_source*,void*,ma_uint64,ma_uint64*);
};
