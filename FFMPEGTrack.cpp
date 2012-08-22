#include <media/stagefright/MediaSource.h>
#include "FFMPEGTrack.h"

#define LOG_TAG "FFMPEGTrack"
namespace android{

FFMPEGTrack::FFMPEGTrack(const sp<FFMPEGExtractor> &extractor,int trackIndex,int streamIndex)
    :   mExtractor(extractor),
        mTrackIndex(trackIndex),
        mStreamIndex(streamIndex)
{
}

status_t FFMPEGTrack::start(MetaData *params)
{
    return 0;
}
status_t FFMPEGTrack::stop()
{
    return 0;
}

sp<MetaData> FFMPEGTrack::getFormat()
{
    return mExtractor->getTrackMetaData(mTrackIndex,0);
}
int FFMPEGTrack::streamIndex()
{
    return mStreamIndex;
}
int FFMPEGTrack::queueBuffer(MediaBuffer* buffer)
{
    ENTER_FUNC();
    mBuffers.push(buffer);
    return 0;
}

int FFMPEGTrack::dequeueBuffer(MediaBuffer **buffer)
{
    ENTER_FUNC();
    if(mBuffers.isEmpty())
        return -1;
    
    MediaBuffer*  mb = mBuffers[0];
    *buffer = mb;
    mBuffers.removeAt(0);
    return OK;
}

status_t FFMPEGTrack::read(
            MediaBuffer **buffer, const ReadOptions *options)
{
    ENTER_FUNC();
    while(dequeueBuffer(buffer) < 0){
            int ret = mExtractor->feedMore(options);
            if(ret != OK) {
                return ret; 
            }
    }
    return OK;
}

FFMPEGTrack::~FFMPEGTrack()
{
}
}
