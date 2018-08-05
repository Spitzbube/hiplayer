
SRCS  = HiDecoder.cpp
SRCS += HiAdec.cpp
SRCS += HiPlayerAudio.cpp

LIB = hiplayer_my.a

include /home/spitzbube/projekte/openatv/build-enviroment/builds/openatv/release/u5pvr/tmp/work/u5pvr-oe-linux-gnueabi/stb-kodi-u5pvr/17.6+gitAUTOINC+7e52c1d94d-r0/git/Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))

