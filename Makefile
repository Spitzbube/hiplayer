
#SRCS  = HiPlayerAudio.cpp
#SRCS += HiPlayerVideo.cpp
#SRCS += HiCodecAudio.cpp
#SRCS += HiCodecVideo.cpp
#SRCS += HiAdec.cpp
#SRCS += HiVdec.cpp

OBJS  = HiPlayerAudio.o
OBJS += HiPlayerVideo.o
OBJS += HiCodecAudio.o
OBJS += HiCodecVideo.o
OBJS += HiAdec.o
OBJS += HiVdec.o
SRCS += HiDecoder.cpp

LIB = hiplayer_my.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

