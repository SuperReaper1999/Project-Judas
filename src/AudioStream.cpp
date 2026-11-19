#include "AudioStream.h"
#include "PerformanceProfiler.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cmath>
#include <thread>
#include <filesystem>
#include <cctype>
namespace {
// Judas publishes all seeks before prefill. Backend restart-after-EOF must not overwrite that target.
ma_result Seek(ma_data_source*,ma_uint64){return MA_SUCCESS;}
ma_result Format(ma_data_source*,ma_format* f,ma_uint32* c,ma_uint32* r,ma_channel* map,size_t cap){
 if(f)*f=ma_format_f32;
 if(c)*c=2;
 if(r)*r=48000;
 if(map)ma_channel_map_init_standard(ma_standard_channel_map_default,map,cap,2);
 return MA_SUCCESS;
}
ma_result Cursor(ma_data_source* p,ma_uint64* n){*n=static_cast<PreparedAudioStream*>(p)->cursor.load();return MA_SUCCESS;}
ma_result Length(ma_data_source* p,ma_uint64* n){*n=static_cast<PreparedAudioStream*>(p)->metadata->length.load();return *n?MA_SUCCESS:MA_NOT_IMPLEMENTED;}
ma_result Loop(ma_data_source* p,ma_bool32 on){static_cast<PreparedAudioStream*>(p)->loop.store(on!=0);return MA_SUCCESS;}
const ma_data_source_vtable table={PreparedAudioStream::Read,Seek,Format,Cursor,Length,Loop,MA_DATA_SOURCE_SELF_MANAGED_RANGE_AND_LOOP_POINT};
}
PreparedAudioStream::PreparedAudioStream(std::shared_ptr<AudioStreamMetadata> m,unsigned n):metadata(std::move(m)),pageFrames(n){
 for(auto& page:pages)page.pcm.resize(pageFrames*2);
 auto config=ma_data_source_config_init();config.vtable=&table;ma_data_source_init(&config,&base);
}
PreparedAudioStream::~PreparedAudioStream(){
 // The graph has detached and queued/running jobs hold their own shared
 // references. The last owner must close even if a cleanup job never ran.
 Close();
 ma_data_source_uninit(&base);
}
void PreparedAudioStream::Seek(std::uint64_t frame){seekFrame.store(frame);generation.fetch_add(1,std::memory_order_release);ready.store(false);ended.store(false);cursor.store(frame);}
void PreparedAudioStream::Close(){if(decoderReady){ma_decoder_uninit(&decoder);decoderReady=false;}for(auto& p:pages)std::vector<float>().swap(p.pcm);}
void PreparedAudioStream::Fill(JobContext& context){
 JUDAS_PROFILE_SCOPE("Audio stream decode worker");
 if(retired.load()||context.CancelRequested())return;
 const auto delay=delayMilliseconds.load();if(delay)std::this_thread::sleep_for(std::chrono::milliseconds(delay));
 if(!decoderReady){
  auto config=ma_decoder_config_init(ma_format_f32,2,48000);
  auto extension=std::filesystem::path(metadata->path).extension().string();std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
  config.encodingFormat=extension==".wav"?ma_encoding_format_wav:extension==".flac"?ma_encoding_format_flac:extension==".mp3"?ma_encoding_format_mp3:ma_encoding_format_unknown;
  if(config.encodingFormat==ma_encoding_format_unknown){error="Streamed audio requires a WAV/MP3/FLAC file extension";failed.store(true,std::memory_order_release);return;}
  if(ma_decoder_init_file(metadata->path.c_str(),&config,&decoder)!=MA_SUCCESS){error="Cannot decode streamed WAV/MP3/FLAC: "+metadata->path;failed.store(true,std::memory_order_release);return;}
  decoderReady=true;
  // Pinned MP3 length lookup can scan the entire file. Never do that for prefill.
  // WAV/FLAC length comes from their headers; MP3 learns its length at real EOF.
  ma_uint64 length=0;if(config.encodingFormat!=ma_encoding_format_mp3&&ma_decoder_get_length_in_pcm_frames(&decoder,&length)==MA_SUCCESS)metadata->length.store(length);
 }
 const auto gen=generation.load(std::memory_order_acquire);
 if(workerGeneration!=gen){
  const auto frame=seekFrame.load();
  if(ma_decoder_seek_to_pcm_frame(&decoder,frame)!=MA_SUCCESS){error="Audio stream seek failed";failed.store(true,std::memory_order_release);return;}
  workerGeneration=gen;workerSequence=0;workerAtEnd=false;
 }
 if(workerAtEnd)return;
 unsigned filled=0;
 for(auto& page:pages){
  if(filled>=2||retired.load()||context.CancelRequested()||generation.load()!=gen)break;
  int expected=0;
  if(!page.state.compare_exchange_strong(expected,1,std::memory_order_acq_rel)){
   expected=2;if(!page.state.compare_exchange_strong(expected,1,std::memory_order_acq_rel))continue;
   if(page.generation==gen){page.state.store(2,std::memory_order_release);continue;}
  }
  page.generation=gen;page.sequence=workerSequence++;page.frames=0;page.end=false;
  ma_uint64 start=0;ma_decoder_get_cursor_in_pcm_frames(&decoder,&start);page.start=start;
  while(page.frames<pageFrames){
   ma_uint64 read=0;const auto result=ma_decoder_read_pcm_frames(&decoder,page.pcm.data()+page.frames*2,pageFrames-page.frames,&read);
   bool finite=true;for(ma_uint64 i=0;i<read*2;++i)finite&=std::isfinite(page.pcm[page.frames*2+i]);
   if(!finite){error="Nonfinite streamed PCM";failed.store(true,std::memory_order_release);page.frames=0;page.end=true;break;}
   page.frames+=static_cast<unsigned>(read);decodedFrames.fetch_add(read);
   if(result!=MA_SUCCESS&&result!=MA_AT_END){error="Audio stream decode failed";failed.store(true,std::memory_order_release);page.end=true;break;}
   if(!read||result==MA_AT_END){
    ma_uint64 end=0;if(metadata->length.load()==0&&ma_decoder_get_cursor_in_pcm_frames(&decoder,&end)==MA_SUCCESS&&end>0)metadata->length.store(end);
    if(loop.load()&&page.frames<pageFrames){
     if(ma_decoder_seek_to_pcm_frame(&decoder,0)!=MA_SUCCESS||(!read&&metadata->length.load()==0)){page.end=true;break;}
    }else{page.end=!loop.load();break;}
   }
  }
  page.state.store(2,std::memory_order_release);++filled;
  if(generation.load()==gen&&!failed.load()){readyGeneration.store(gen);ready.store(true,std::memory_order_release);}
  if(page.end){workerAtEnd=true;break;}
 }
}
ma_result PreparedAudioStream::Read(ma_data_source* p,void* output,ma_uint64 count,ma_uint64* read){
 auto& s=*static_cast<PreparedAudioStream*>(p);auto* out=static_cast<float*>(output);ma_uint64 done=0;
 const auto gen=s.generation.load(std::memory_order_acquire);
 if(s.audioGeneration!=gen){
  if(s.reading>=0)s.pages[s.reading].state.store(0,std::memory_order_release);
  s.reading=-1;s.offset=0;s.audioSequence=0;s.audioGeneration=gen;
 }
 while(done<count){
  if(s.reading<0){
   for(unsigned i=0;i<s.pages.size();++i){
    auto& page=s.pages[i];int expected=2;if(!page.state.compare_exchange_strong(expected,3,std::memory_order_acquire))continue;
    if(page.generation!=gen){page.state.store(0,std::memory_order_release);continue;}
    if(page.sequence!=s.audioSequence){page.state.store(2,std::memory_order_release);continue;}
    s.reading=int(i);s.offset=0;break;
   }
  }
  if(s.reading<0)break;
  auto& page=s.pages[s.reading];const auto n=std::min<ma_uint64>(count-done,page.frames-s.offset);
  if(out&&n)std::memcpy(out+done*2,page.pcm.data()+s.offset*2,n*2*sizeof(float));
  s.offset+=unsigned(n);done+=n;
  auto cursor=page.start+s.offset;const auto length=s.metadata->length.load();if(s.loop.load()&&length)cursor%=length;if(gen==s.generation.load())s.cursor.store(cursor);
  if(s.offset==page.frames){
   const bool end=page.end;s.reading=-1;++s.audioSequence;page.state.store(0,std::memory_order_release);
   if(end){s.ended.store(true);break;}
  }
 }
 if(done<count&&!s.ended.load()){
  if(out)std::fill(out+done*2,out+count*2,0.f);
  s.starved.store(true);s.underruns.fetch_add(1);if(read)*read=count;return MA_SUCCESS; // media cursor holds through starvation
 }
 s.starved.store(false);
 if(read)*read=done;
 return s.ended.load()?MA_AT_END:MA_SUCCESS;
}
