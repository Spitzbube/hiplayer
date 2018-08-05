
SRCS  = HiDecoder.cpp
SRCS += HiAdec.cpp
SRCS += HiPlayerAudio.cpp

LIB = hiplayer_my.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

