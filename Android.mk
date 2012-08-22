LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=\
        FFMPEGExtractor.cpp   \
        FFMPEGProtocol.cpp \
        FFMPEGTrack.cpp \
        FFMPEGDecoder.cpp

LOCAL_C_INCLUDES:= \
	$(JNI_H_INCLUDE) \
	$(TOP)/frameworks/base/include/media/stagefright/openmax \
    $(TOP)/frameworks/base/media/libstagefright \
    $(TOP)/frameworks/base/media/libstagefright/ffmpeg/include \
    $(TOP)/external/ffmpeg

LOCAL_SHARED_LIBRARIES += \
        libffmpeg

LOCAL_MODULE:= libstagefright_ffmpeg

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi -D__STDC_CONSTANT_MACROS
endif

include $(BUILD_STATIC_LIBRARY)
