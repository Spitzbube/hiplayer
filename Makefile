
##OBJS  = HiPlayerAudio.o
SRCS  = HiPlayerAudio.cpp
SRCS += HiPlayerVideo.cpp
SRCS += HiCodecAudio.cpp
SRCS += HiCodecVideo.cpp
SRCS += HiAdec.cpp
SRCS += HiVdec.cpp
SRCS += HiDecoder.cpp

LIB = hiplayer_my.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

