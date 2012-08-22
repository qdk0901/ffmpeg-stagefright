/**
*   
*/
#include "FFMPEGExtractor.h"
#include "FFMPEGProtocol.h"
#include "OMX_Video.h"
#include <utils/String8.h>

#define LOG_TAG "FFMPEG"

namespace android{
FFMPEGExtractor::FFMPEGExtractor(const sp<DataSource> &source)
    :   mDataSource(source),
        mAVFC(NULL)
{
    av_register_all();
    FFMPEGRegisterProtocol(); 

    mAVFC = avformat_alloc_context();

    char url[256]={0,};

    sprintf(url,"android:%x",&mDataSource);

    int err = avformat_open_input(&mAVFC,url,0,NULL);

    if (err < 0) {
        FFLOG(DL2,"%s: Open Input Error\n",__func__);
        return;
    }
    avformat_find_stream_info(mAVFC,NULL);

    int i,j=0;
    for (i = 0; i < mAVFC->nb_streams; i++)
        mAVFC->streams[i]->discard = AVDISCARD_ALL;
    
    int st_index[AVMEDIA_TYPE_NB];
    memset(st_index, -1, sizeof(st_index));

    st_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream(mAVFC, AVMEDIA_TYPE_VIDEO,-1, -1, NULL, 0);
    st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(mAVFC, AVMEDIA_TYPE_AUDIO,-1,
                                                    st_index[AVMEDIA_TYPE_VIDEO],NULL, 0);
    st_index[AVMEDIA_TYPE_SUBTITLE] = av_find_best_stream(mAVFC, AVMEDIA_TYPE_SUBTITLE,-1,
                                                    (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                                                     st_index[AVMEDIA_TYPE_AUDIO] :
                                                     st_index[AVMEDIA_TYPE_VIDEO]),
                                                    NULL, 0);

    for(i = 0; i < AVMEDIA_TYPE_NB ; i++){
        int k = st_index[i];
        if(k >= 0){
            AVCodecContext *avctx;
            AVCodec* codec;
            avctx = mAVFC->streams[k]->codec;
            codec = avcodec_find_decoder(avctx->codec_id);

            if(!codec) // do not handle a stream without codec
                continue;

            mAVFC->streams[k]->discard = AVDISCARD_DEFAULT;
            
            avctx->idct_algo = FF_IDCT_AUTO;
            avctx->skip_frame   = AVDISCARD_DEFAULT;
            avctx->skip_idct    = AVDISCARD_DEFAULT;
            avctx->skip_loop_filter  = AVDISCARD_DEFAULT;   
            avctx->error_concealment = 3;
        
            avctx->workaround_bugs = 1;
            avctx->lowres = 0;
            
            mTracks.push(new FFMPEGTrack(this,j++,k));
        }
    }
}
FFMPEGExtractor::~FFMPEGExtractor()
{
    ENTER_FUNC();
    avformat_close_input(&mAVFC);
    av_free(mAVFC);
    mAVFC = NULL;
}
size_t FFMPEGExtractor::countTracks()
{
    ENTER_FUNC();
    FFLOG(DL1,"Track Count = %d\n",mTracks.size());
    return mTracks.size();
}

sp<MediaSource> FFMPEGExtractor::getTrack(size_t index)
{
    ENTER_FUNC();
    if(index > mTracks.size())
        return NULL;
    return mTracks[index];
}

sp<MetaData> FFMPEGExtractor::getTrackMetaData(size_t index, uint32_t flags)
{
    ENTER_FUNC();
    int streamIndex;
    AVCodecContext *avctx;
    char mime[256] = {0,};

    sp<FFMPEGTrack> track = mTracks[index];
    
    streamIndex = track->streamIndex();
    avctx = mAVFC->streams[streamIndex]->codec;
    sp<MetaData> meta = new MetaData;
    
    sprintf(mime,"%s/%s",av_get_media_type_string(avctx->codec_type),avcodec_get_name(avctx->codec_id));
    
    if(avctx->codec_id == CODEC_ID_MPEG4){
        sprintf(mime,"video/mp4v-es");
    }
    meta->setCString(kKeyMIMEType,mime);  
    meta->setPointer(kKeyFFMPEGAVStream,mAVFC->streams[streamIndex]);

    if(avctx->codec_type == AVMEDIA_TYPE_VIDEO){
        meta->setInt32(kKeyWidth,avctx->width);
        meta->setInt32(kKeyHeight,avctx->height);
    }else if(avctx->codec_type == AVMEDIA_TYPE_AUDIO){
        meta->setInt32(kKeyChannelCount,avctx->channels);
        meta->setInt32(kKeySampleRate,avctx->sample_rate);
    }

    FFLOG(DL1,"Track index=%d, Stream Index =%d,  Mime=%s\n",index,streamIndex,mime);
    return meta;      
}
sp<MetaData> FFMPEGExtractor::getMetaData()
{
    ENTER_FUNC();
    sp<MetaData> meta = new MetaData;
    meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_CONTAINER_FFMPEG);

    return meta;    
}
uint32_t FFMPEGExtractor::flags() const
{
    ENTER_FUNC();
    int flags = CAN_PAUSE;
    flags |= CAN_SEEK_FORWARD | CAN_SEEK_BACKWARD | CAN_SEEK;
    
    return flags;
}
int FFMPEGExtractor::feedMore(const MediaSource::ReadOptions *options)
{
    ENTER_FUNC();
    AVPacket* pkt;
    int i;

    pkt = (AVPacket*)av_mallocz(sizeof(AVPacket));
    av_init_packet(pkt);

    int64_t t = av_gettime();
    int ret = av_read_frame(mAVFC, pkt);

    if(ret < 0){
        if(ret == AVERROR_EOF)
            return ERROR_END_OF_STREAM;    
    }

    sp<FFMPEGTrack> track;

    for(i = 0; i < mTracks.size(); i++){
        if(mTracks[i]->streamIndex() != pkt->stream_index)
            continue;
        track = mTracks[i];
    }
    if(track == NULL)
        return OK;

    int64_t pts = pkt->pts * av_q2d(mAVFC->streams[pkt->stream_index]->time_base) * 1000000;
    
    MediaBuffer* m;
    AVCodecContext *avctx = mAVFC->streams[pkt->stream_index]->codec;
    if(avctx->codec_id == CODEC_ID_MPEG4){
        m =new MediaBuffer(pkt->size + avctx->extradata_size);
        m->set_range(0,pkt->size + avctx->extradata_size);  

#if 0
        int i;
        char buf[2048],*p = buf;
        
        for(i = 0; i < avctx->extradata_size; i++){
            p += sprintf(p,"%02x ",((unsigned char*)avctx->extradata)[i]);    
        }
        FFLOG(DL0,"===========>%s",buf); 
#endif
        // data used by FFMPEGDecoder or other decoder
        
        memcpy(m->data(),avctx->extradata,avctx->extradata_size);
        memcpy(m->data() + avctx->extradata_size,pkt->data,pkt->size);
        
        m->meta_data()->setInt64(kKeyTime,pts);

        av_free_packet(pkt);
        pkt = NULL;
    }else{
        m =new MediaBuffer(pkt->size);
        m->set_range(0,pkt->size);  

        // data used by FFMPEGDecoder or other decoder
        memcpy(m->data(),pkt->data,pkt->size);
        
        // data used only by FFMPEGDecoder
        m->meta_data()->setPointer(kKeyFFMPEGAVPacket,pkt);
        m->meta_data()->setInt64(kKeyTime,pts);
    }
    track->queueBuffer(m);

    return OK;
}

bool SniffFFMPEG(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *)
{
    ENTER_FUNC();
    av_register_all();
    FFMPEGRegisterProtocol(); 
    
    AVFormatContext *ic;

    char url[256]={0,};

    sprintf(url,"android:%x",&source);

    ic = avformat_alloc_context();
    avformat_open_input(&ic,url,0,NULL);
    
    av_dump_format(ic, 0, url, 0);

    if(!strcmp(ic->iformat->name,"mp4") ||
        !strcmp(ic->iformat->name,"mp3") ||
        !strcmp(ic->iformat->name,"vorbis") || 
        !strcmp(ic->iformat->name,"vorbis") ||
        !strcmp(ic->iformat->name,"ogg") || 
        !strcmp(ic->iformat->name,"wav") || 
        !strcmp(ic->iformat->name,"mpeg4") ||
        !strcmp(ic->iformat->name,"mp2ts")
    ){
        *confidence = 0.1f; 
    }else{
        *confidence = 1.0f; 
    }
    avformat_close_input(&ic);
    av_free(ic);
    *mimeType = MEDIA_MIMETYPE_CONTAINER_FFMPEG;  
     
    return true;   
}


}














