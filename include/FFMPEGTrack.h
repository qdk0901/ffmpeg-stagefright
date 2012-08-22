#ifndef _FFMPEG_SOURCE_H
#define _FFMPEG_SOURCE_H

#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MediaBuffer.h>
#include "FFMPEGExtractor.h"
#include "FFLog.h"

namespace android{

class FFMPEGExtractor;

class FFMPEGTrack : public MediaSource {
public:
    FFMPEGTrack(const sp<FFMPEGExtractor> &extractor,int trackIndex,int streamIndex);

    virtual status_t start(MetaData *params = NULL);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options = NULL);

    int queueBuffer(MediaBuffer *buffer);
    int streamIndex();
protected:
    virtual ~FFMPEGTrack();

private:
    int dequeueBuffer(MediaBuffer** buffer);
    sp<FFMPEGExtractor> mExtractor;
    int mTrackIndex;
    int mStreamIndex;
    Vector<MediaBuffer*> mBuffers;
};


}
#endif
