#ifndef _FFLOG_H_
#define _FFLOG_H_

extern "C"{
#include <libavutil/log.h>
};

#define DEBUG_LEVEL AV_LOG_DEBUG //AV_LOG_INFO

#define DL0 AV_LOG_INFO
#define DL1 AV_LOG_DEBUG
#define DL2 AV_LOG_ERROR

#define FFLOG(LEVEL,...) av_log(NULL, (LEVEL), __VA_ARGS__)

#define ENTER_FUNC() FFLOG(DL0,"%s\n",__func__)

#endif
