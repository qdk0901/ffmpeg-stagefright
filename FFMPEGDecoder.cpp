#include "FFMPEGDecoder.h"
#include "OMX_Video.h"
#define LOG_TAG "FFMPEGDecoder"

namespace android {

FFMPEGDecoder::FFMPEGDecoder(const sp<MediaSource> &source)
    :   mSource(source),
        mSwsctx(NULL),
        mYUVFrame(NULL),
        mFrame(mFrame),
        mVideoClock(0),
        mAudioClock(0),
        mTargetFormat(AUDIO_TARGET_FORMAT),
        mSourceFormat(AUDIO_TARGET_FORMAT)
{
    ENTER_FUNC();
    sp<MetaData> srcFormat = mSource->getFormat();
    
    // only decode content demux by ffmpeg
    // the ffmpeg extractor passes the AVCodecContext throught meta data
    srcFormat->findPointer(kKeyFFMPEGAVStream,(void**)&mAvStream);
    mAvctx = mAvStream->codec;
}

status_t FFMPEGDecoder::start(MetaData *params)
{
    ENTER_FUNC();

    mLastTS = av_gettime();
    av_init_packet(&mPacket);
    mPacket.size = 0;
    mPacketPtr = NULL;

    CHECK(mAvctx);
    mCodec = avcodec_find_decoder(mAvctx->codec_id);
    CHECK(mCodec);

    mFrame = avcodec_alloc_frame();
    CHECK(mFrame);

    if(mAvctx->codec_type == AVMEDIA_TYPE_VIDEO){
        mYUVFrame = avcodec_alloc_frame();
        CHECK(mYUVFrame);
    }

    if (avcodec_open2(mAvctx, mCodec, NULL) < 0) {
        FFLOG(DL2,"Could not open codec!!\n");
        return ERROR_END_OF_STREAM;
    }
    return OK;
}
status_t FFMPEGDecoder::stop()
{
    ENTER_FUNC();
    avcodec_close(mAvctx);
    av_free(mAvctx);
    mAvctx = NULL;

    av_free(mFrame);
    mFrame = NULL;

    if(mYUVFrame) av_free(mYUVFrame);
    return OK;
}
sp<MetaData> FFMPEGDecoder::getFormat()
{
    CHECK(mAvctx);
    sp<MetaData> meta = new MetaData;
    const char *mime;

    meta->setCString(kKeyDecoderComponent, "FFMPEGDecoder");

    if(mAvctx->codec_type == AVMEDIA_TYPE_VIDEO){
        meta->setInt32(kKeyWidth,mAvctx->width);
        meta->setInt32(kKeyHeight,mAvctx->height);
        meta->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420Planar);
    }else if(mAvctx->codec_type == AVMEDIA_TYPE_AUDIO){
        meta->setInt32(kKeyChannelCount,mAvctx->channels);
        meta->setInt32(kKeySampleRate,mAvctx->sample_rate);
        meta->setCString(kKeyMIMEType,MEDIA_MIMETYPE_AUDIO_RAW);
    }
    return meta;
}

int FFMPEGDecoder::getAVFrame(const ReadOptions *options,int64_t* pts_out,int* size)
{
    MediaBuffer *inputBuffer = NULL;
    int ds = 0 , len = 0;
    if(!mPacketPtr){
        status_t err = mSource->read(&inputBuffer, options);

        if (err != OK) {
            return ERROR_END_OF_STREAM;
        }
        inputBuffer->meta_data()->findPointer(kKeyFFMPEGAVPacket,(void**)&mPacketPtr);
        
        CHECK(mPacketPtr);
        memcpy(&mPacket,mPacketPtr,sizeof(mPacket));
        avcodec_get_frame_defaults(mFrame);
    }
    if(mAvctx->codec_type == AVMEDIA_TYPE_VIDEO){
        len = avcodec_decode_video2(mAvctx, mFrame, &ds, &mPacket);
        if(len >= 0){
            if(ds){
                // doing video sync
                // audio packet pts passed from extractor
                int64_t pts = av_frame_get_best_effort_timestamp(mFrame);

                double dpts = pts;

                if(pts != AV_NOPTS_VALUE)
                    mVideoClock = dpts;  
                else
                    dpts = mVideoClock;

                double delay = av_q2d(mAvStream->time_base);

                delay += mFrame->repeat_pict * (delay * 0.5);
                mVideoClock += delay;
                
                dpts = dpts * av_q2d(mAvStream->time_base) * 1000000;

                *pts_out = (int64_t)dpts;
            }
        }
    }else if(mAvctx->codec_type == AVMEDIA_TYPE_AUDIO){
        len = avcodec_decode_audio4(mAvctx, mFrame, &ds, &mPacket);
        if(len >= 0){
            if(ds){
                int data_size = av_samples_get_buffer_size(NULL, mAvctx->channels,
                                                       mFrame->nb_samples,
                                                       mAvctx->sample_fmt, 1);
                *size = data_size;


                int dec_channel_layout = 0;
                if(mAvctx->channel_layout && mAvctx->channels == av_get_channel_layout_nb_channels(mAvctx->channel_layout)){
                    dec_channel_layout = mAvctx->channel_layout;
                }else{
                    dec_channel_layout = av_get_default_channel_layout(mAvctx->channels);    
                }

                if (mAvctx->sample_fmt != mTargetFormat || 
                    !mSwrctx) {
                    if (mSwrctx){
                        swr_free(&mSwrctx);
                        mSwrctx = NULL;
                    }
                    mSwrctx = swr_alloc_set_opts(NULL,dec_channel_layout, mTargetFormat, mAvctx->sample_rate,
                                             dec_channel_layout,mAvctx->sample_fmt,mAvctx->sample_rate,
                                             0, NULL);
                    if (!mSwrctx || swr_init(mSwrctx) < 0) {
                        goto finished;
                    }
                    mSourceFormat = mAvctx->sample_fmt;
                }

                if (mSwrctx) {
                    const uint8_t *in[] = { mFrame->data[0] };
                    uint8_t *out[] = {mAudioBuf};
                    
                    int len2 = swr_convert(mSwrctx, out, sizeof(mAudioBuf) / mAvctx->channels / av_get_bytes_per_sample(mTargetFormat),
                                                    in, mFrame->nb_samples);
                    if (len2 < 0) {
                        FFLOG(DL2,"audio_resample() failed\n");
                        goto finished;
                    }
                    if (len2 == sizeof(mAudioBuf) / mAvctx->channels / av_get_bytes_per_sample(mTargetFormat)) {
                        FFLOG(DL2,"warning: audio buffer is probably too small\n");
                        swr_init(mSwrctx);
                    }
                    *size = len2 * mAvctx->channels * av_get_bytes_per_sample(mTargetFormat);
                }

                int64_t pts = mPacket.pts;
                double dpts = av_q2d(mAvStream->time_base) * 1000000;
                if(pts != AV_NOPTS_VALUE) {
                    dpts = pts * dpts;
                    mAudioClock = dpts;
                }else{
                    dpts = mAudioClock * dpts;
                    mAudioClock += (double)data_size /
                                (mAvctx->channels * mAvctx->sample_rate * av_get_bytes_per_sample(mAvctx->sample_fmt));
                }
                *pts_out = dpts;
            }
        }
    }
finished:
    if(inputBuffer){
        inputBuffer->release();
        inputBuffer = NULL;
    }
    if(ds){
        mPacket.size -= len;
        mPacket.data += len;
        if(mPacket.size <= 0){
            av_free_packet(mPacketPtr);
            av_free(mPacketPtr);
            mPacketPtr = NULL;        
        }
        return OK;
    }else{
        av_free_packet(mPacketPtr);
        av_free(mPacketPtr);
        mPacketPtr = NULL;          
        return ERROR_NOT_FINISHED;
    }
}
static int saved = 0;
static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,
                     char *filename)
{
    if(saved == 0){
        saved = 1;        
    }else
        return;

    FILE *f;
    int i;

    f=fopen(filename,"w");
    fprintf(f,"P5\n%d %d\n%d\n",xsize,ysize,255);
    for(i=0;i<ysize;i++)
        fwrite(buf + i * wrap,1,xsize,f);
    fclose(f);
}

status_t FFMPEGDecoder::read(
            MediaBuffer **buffer, const ReadOptions *options)
{
    int ret = OK;
    int64_t pts = 0;
    int size;
    do{
        ret = getAVFrame(options,&pts,&size);
    }while(ret == ERROR_NOT_FINISHED);

    MediaBuffer* outBuffer;
    if(mAvctx->codec_type == AVMEDIA_TYPE_VIDEO){
        // decoding success
        size = avpicture_get_size(PIX_FMT_YUV420P,mAvctx->width,mAvctx->height);
        outBuffer =new MediaBuffer(size);

        avpicture_fill((AVPicture*)mYUVFrame, (uint8_t*)outBuffer->data(), PIX_FMT_YUV420P, mAvctx->width, mAvctx->height);
        mSwsctx = sws_getCachedContext(mSwsctx,mAvctx->width, mAvctx->height, (PixelFormat)mFrame->format, mAvctx->width, mAvctx->height,
                                PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
        
        CHECK(mSwsctx);
        sws_scale(mSwsctx, mFrame->data, mFrame->linesize,
              0, mAvctx->height, mYUVFrame->data, mYUVFrame->linesize);

        //pgm_save(mYUVFrame->data[0], mYUVFrame->linesize[0],
          //               mAvctx->width, mAvctx->height, "/data/save.pgm");

    }else if(mAvctx->codec_type == AVMEDIA_TYPE_AUDIO){
        
        outBuffer =new MediaBuffer(size);
        if(mSwrctx){
            memcpy(outBuffer->data(),mAudioBuf,size);
        }else{
            memcpy(outBuffer->data(),mFrame->data[0],size);
        }
    }
    FFLOG(DL1,"codec[%d] PTS=%f\n",mAvctx->codec_type,pts/1000000.0f);
    outBuffer->meta_data()->setInt64(kKeyTime,pts);
    *buffer = outBuffer;

    return ret;
}
FFMPEGDecoder::~FFMPEGDecoder()
{

}
}

