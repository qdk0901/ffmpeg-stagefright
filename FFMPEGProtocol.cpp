#include "FFMPEGProtocol.h"

#include <sys/stat.h>
extern "C" {
#include "libavformat/url.h"
}

#define LOG_TAG "FFMPEG"
namespace android{


class FFSource
{
public:
    FFSource(sp<DataSource> source);
    ssize_t read(unsigned char *buf, size_t size);
    int64_t seek(int64_t pos);
    off64_t getSize();
    ~FFSource();
protected:    
    sp<DataSource> mSource;
    int64_t mOffset;
};

FFSource::FFSource(sp<DataSource> source)
    :   mSource(source),
        mOffset(0)
{
}
FFSource::~FFSource()
{
}
ssize_t FFSource::read(unsigned char *buf, size_t size)
{
    FFLOG(DL1,"%s  %lld,%d\n",__func__,mOffset,size);

    int n  = mSource->readAt(mOffset,buf,size);
    
    if(n > 0) mOffset += n;
    return n;
}
int64_t FFSource::seek(int64_t pos)
{
    mOffset = pos;
    return 0;
}
off64_t FFSource::getSize()
{
    off_t sz;
    mSource->getSize(&sz);
    return sz;
}

static int android_open(URLContext *h, const char *url, int flags)
{
    FFLOG(DL1,"%s (%s)\n",__func__,url);
    
    // the url in form of "android:<DataSource Ptr>",
    // the DataSource Pointer passed by the ffmpeg extractor
    sp<DataSource>* source = NULL;  
    sscanf(url + strlen("android:"), "%x", &source);
    if(source == NULL){
        FFLOG(DL2,"Open data source error!");
        return -1;    
    }
    FFSource* ffs = new FFSource(*source);
    h->priv_data = (void*)ffs;
    return 0;
}
static int android_read(URLContext *h, unsigned char *buf, int size)
{
    FFSource* ffs = (FFSource*)h->priv_data;
    return ffs->read(buf,size);
}
static int android_write(URLContext *h, const unsigned char *buf, int size)
{
    return -1;
}
static int64_t android_seek(URLContext *h, int64_t pos, int whence)
{
    FFSource* ffs = (FFSource*)h->priv_data;

    if (whence == AVSEEK_SIZE) {
        return ffs->getSize();
    }

    ffs->seek(pos);
    return 0;
}
static int android_close(URLContext *h)
{
    ENTER_FUNC();
    FFSource* ffs = (FFSource*)h->priv_data;
    delete ffs;
    return 0;
}
static int android_get_handle(URLContext *h)
{
    ENTER_FUNC();
    return (intptr_t)h->priv_data;
}

URLProtocol ff_android_protocol  = {   
    "android",  
    android_open,
    NULL,
    android_read,
    android_write,  
    android_seek,
    android_close,
    NULL,  // next
    NULL,  //url_read_pause
    NULL,  //url_read_seek
    android_get_handle, 
    0,  
    NULL,  
    0,  
    NULL  
};
#define MSGSIZE_MAX 3072
char loginfo[MSGSIZE_MAX]={0,};
char* loginfo_ptr = loginfo;

void log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
    char tmp[MSGSIZE_MAX];

    int has_nl = 0,i = 0;   
    
    vsnprintf(tmp, MSGSIZE_MAX, fmt, vl);
    
    while(tmp[i] != '\0' && i < (MSGSIZE_MAX - 1)){
        if(tmp[i] == '\n' || tmp[i] == '\r'){
            tmp[i] = ' ';
            has_nl = 1;
            break;
        }
        i++;    
    }

    if(i == MSGSIZE_MAX - 1)
        has_nl = 1;
    if(has_nl){
        sprintf(loginfo_ptr,"%s",tmp);
        loginfo[MSGSIZE_MAX - 1] = 0;
        if(level >= DEBUG_LEVEL)
            LOGD("%s",loginfo);

        loginfo_ptr = loginfo;
        loginfo[0] = 0;
    }else{
        loginfo_ptr += sprintf(loginfo_ptr,"%s",tmp);
    }  
}

int hasRegistered = 0;
int FFMPEGRegisterProtocol()
{
    ENTER_FUNC();
    if(hasRegistered) return 0;
    hasRegistered = 1;

    av_log_set_callback(log_callback);
    ffurl_register_protocol(&ff_android_protocol,sizeof(URLProtocol));
    return 0;
}
}
