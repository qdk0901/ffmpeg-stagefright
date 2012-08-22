
#ifndef _FFMPEGEXTRACTOR_H_
#define _FFMPEGEXTRACTOR_H_

#include <media/stagefright/MediaExtractor.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaDebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/MediaBuffer.h>

#include <utils/Vector.h>
#include "FFMPEGTrack.h"
#include "FFLog.h"


extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}


namespace android {

class FFMPEGTrack;

class FFMPEGExtractor : public MediaExtractor {

public:
    FFMPEGExtractor(const sp<DataSource> &source);

    virtual size_t countTracks();
    virtual sp<MediaSource> getTrack(size_t index);
    virtual sp<MetaData> getTrackMetaData(size_t index, uint32_t flags);

    virtual sp<MetaData> getMetaData();

    virtual uint32_t flags() const;

    int feedMore(const MediaSource::ReadOptions *options);
protected:
    virtual ~FFMPEGExtractor();

private:
    sp<DataSource> mDataSource;
    AVFormatContext *mAVFC;
    Vector<sp<FFMPEGTrack> > mTracks;
};

bool SniffFFMPEG(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *);
}
#endif
