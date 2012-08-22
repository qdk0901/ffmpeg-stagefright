
#ifndef _FFMPEG_DECODER_H_
#define _FFMPEG_DECODER_H_

#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MediaDebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/MediaBuffer.h>
#include "FFLog.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}

#define ERROR_NOT_FINISHED 0x12345678
#define AUDIO_TARGET_FORMAT AV_SAMPLE_FMT_S16


namespace android {

struct FFMPEGDecoder : public MediaSource {
    FFMPEGDecoder(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

protected:
    virtual ~FFMPEGDecoder();

private:
    int getAVFrame(const ReadOptions *options,int64_t* pts_out,int* size);
    sp<MediaSource> mSource;
    AVStream* mAvStream;
    AVCodecContext* mAvctx;
    AVCodec* mCodec;
    AVFrame* mFrame;
    AVFrame* mYUVFrame;
    SwsContext* mSwsctx;
    SwrContext* mSwrctx;
    AVPacket  mPacket,*mPacketPtr;
    // use for timestamp calculation
    double mVideoClock;
    double mAudioClock;
    AVSampleFormat mTargetFormat;
    AVSampleFormat mSourceFormat;
    DECLARE_ALIGNED(16,uint8_t,mAudioBuf)[AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
    int64_t mLastTS;
};

}
#endif
