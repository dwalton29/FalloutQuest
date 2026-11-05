#include "fo3-bink-decoder.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#include <cmath>
#include <cstring>
namespace fo3movie {
namespace {
std::string Failure(const char* operation,int status){char message[AV_ERROR_MAX_STRING_SIZE]{};av_strerror(status,message,sizeof(message));return std::string(operation)+": "+message;}
}
struct Decoder::Impl {
    AVFormatContext* format=nullptr;AVCodecContext* video=nullptr;AVCodecContext* audio=nullptr;
    AVPacket* packet=nullptr;AVFrame* frame=nullptr;SwsContext* scaler=nullptr;
    int videoStream=-1,audioStream=-1,pending=-1,drain=0;bool eof=false;
    double fps=0,videoClock=0,audioClock=0;
    ~Impl(){sws_freeContext(scaler);av_frame_free(&frame);av_packet_free(&packet);avcodec_free_context(&video);avcodec_free_context(&audio);avformat_close_input(&format);}
    bool OpenCodec(int stream,AVCodecContext*& context,std::string& error){
        const auto* codec=avcodec_find_decoder(format->streams[stream]->codecpar->codec_id);
        if(!codec){error="Bink codec unavailable";return false;}
        context=avcodec_alloc_context3(codec);if(!context){error="Movie decoder allocation failed";return false;}
        context->max_pixels=1920*1080;context->thread_count=1;
        int status=avcodec_parameters_to_context(context,format->streams[stream]->codecpar);
        if(status>=0)status=avcodec_open2(context,codec,nullptr);
        if(status<0){error=Failure("Open Bink codec",status);return false;}return true;
    }
    bool Convert(int stream,Decoded& out,std::string& error){
        const auto* source=format->streams[stream];const bool isVideo=stream==videoStream;
        out={};out.kind=isVideo?Decoded::Kind::Video:Decoded::Kind::Audio;
        const auto timestamp=frame->best_effort_timestamp;
        out.pts=timestamp==AV_NOPTS_VALUE?(isVideo?videoClock:audioClock):timestamp*av_q2d(source->time_base);
        if(!std::isfinite(out.pts)||out.pts<0||out.pts>7200){error="Invalid movie timestamp";return false;}
        if(isVideo){
            if(frame->width<=0||frame->height<=0||frame->width>1920||frame->height>1080){error="Unsupported movie dimensions";return false;}
            out.width=frame->width;out.height=frame->height;out.duration=1/fps;
            scaler=sws_getCachedContext(scaler,out.width,out.height,AVPixelFormat(frame->format),out.width,out.height,AV_PIX_FMT_RGBA,SWS_BILINEAR,nullptr,nullptr,nullptr);
            if(!scaler){error="Movie colour conversion unavailable";return false;}
            out.rgba.resize(size_t(out.width)*out.height*4);uint8_t* pixels[4]{out.rgba.data(),nullptr,nullptr,nullptr};int stride[4]{out.width*4,0,0,0};
            if(sws_scale(scaler,frame->data,frame->linesize,0,out.height,pixels,stride)!=out.height){error="Incomplete movie frame";return false;}
            videoClock=out.pts+out.duration;
        }else{
            out.channels=frame->ch_layout.nb_channels;out.sampleRate=frame->sample_rate;
            if((out.channels!=1&&out.channels!=2)||out.sampleRate<8000||out.sampleRate>96000||frame->nb_samples<0||frame->nb_samples>65536){error="Unsupported movie audio format";return false;}
            if(frame->format!=AV_SAMPLE_FMT_FLT&&frame->format!=AV_SAMPLE_FMT_FLTP){error="Unsupported Bink sample format";return false;}
            out.duration=double(frame->nb_samples)/out.sampleRate;out.samples.resize(size_t(frame->nb_samples)*out.channels);
            if(frame->format==AV_SAMPLE_FMT_FLT)std::memcpy(out.samples.data(),frame->extended_data[0],out.samples.size()*sizeof(float));
            else for(int f=0;f<frame->nb_samples;++f)for(int c=0;c<out.channels;++c)out.samples[size_t(f)*out.channels+c]=reinterpret_cast<const float*>(frame->extended_data[c])[f];
            audioClock=out.pts+out.duration;
        }
        return true;
    }
};
Decoder::Decoder()=default;Decoder::~Decoder()=default;
bool Decoder::Open(const std::string& path,std::string& error){
    error.clear();av_max_alloc(32u*1024u*1024u);impl_=std::make_unique<Impl>();auto& s=*impl_;
    s.format=avformat_alloc_context();if(!s.format){error="Movie format allocation failed";return false;}s.format->max_streams=32;
    int status=avformat_open_input(&s.format,path.c_str(),av_find_input_format("bink"),nullptr);
    if(status<0){error=Failure("Open original Bink movie",status);impl_.reset();return false;}
    // The Bink demuxer reads its stream descriptions directly from the header.
    // Do not probe/decode ahead on the render thread.
    if(s.format->nb_streams==0||s.format->nb_streams>32){error="Invalid Bink stream count";impl_.reset();return false;}
    for(unsigned i=0;i<s.format->nb_streams;++i){const auto id=s.format->streams[i]->codecpar->codec_id;
        if(id==AV_CODEC_ID_BINKVIDEO&&s.videoStream<0)s.videoStream=int(i);
        if((id==AV_CODEC_ID_BINKAUDIO_DCT||id==AV_CODEC_ID_BINKAUDIO_RDFT)&&s.audioStream<0)s.audioStream=int(i);
    }
    if(s.videoStream<0){error="Bink video stream unavailable";impl_.reset();return false;}
    s.fps=av_q2d(s.format->streams[s.videoStream]->avg_frame_rate);
    if(!std::isfinite(s.fps)||s.fps<=0||s.fps>120){error="Invalid Bink frame rate";impl_.reset();return false;}
    if(!s.OpenCodec(s.videoStream,s.video,error)||(s.audioStream>=0&&!s.OpenCodec(s.audioStream,s.audio,error))){impl_.reset();return false;}
    s.packet=av_packet_alloc();s.frame=av_frame_alloc();if(!s.packet||!s.frame){error="Movie packet allocation failed";impl_.reset();return false;}return true;
}
bool Decoder::Next(Decoded& out,std::string& error){
    error.clear();out={};if(!impl_){error="Movie is not open";return false;}auto& s=*impl_;
    for(;;){
        if(s.pending>=0){auto* codec=s.pending==s.videoStream?s.video:s.audio;
            const int status=avcodec_receive_frame(codec,s.frame);
            if(status>=0){const bool converted=s.Convert(s.pending,out,error);av_frame_unref(s.frame);return converted;}
            if(status!=AVERROR(EAGAIN)&&status!=AVERROR_EOF){error=Failure("Decode Bink frame",status);return false;}
            s.pending=-1;
        }
        if(s.eof){
            if(s.drain<2){const int stream=s.drain++==0?s.videoStream:s.audioStream;if(stream<0)continue;
                auto* codec=stream==s.videoStream?s.video:s.audio;const int status=avcodec_send_packet(codec,nullptr);
                if(status<0&&status!=AVERROR_EOF){error=Failure("Drain Bink codec",status);return false;}s.pending=stream;continue;
            }return false;
        }
        int status=av_read_frame(s.format,s.packet);
        if(status==AVERROR_EOF){s.eof=true;continue;}
        if(status<0){error=Failure("Read Bink packet",status);return false;}
        const int stream=s.packet->stream_index;auto* codec=stream==s.videoStream?s.video:stream==s.audioStream?s.audio:nullptr;
        if(!codec){av_packet_unref(s.packet);continue;}
        status=avcodec_send_packet(codec,s.packet);av_packet_unref(s.packet);
        if(status<0){error=Failure("Submit Bink packet",status);return false;}s.pending=stream;
    }
}
}
