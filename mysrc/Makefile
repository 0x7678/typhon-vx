
OSMOCOM = ../osmocom-bb-sylvain-burst_ind


LIB_MOBILE = $(OSMOCOM)/src/host/layer23/src/mobile/libmobile.a
LIB_LAYER23 = $(OSMOCOM)/src/host/layer23/src/common/liblayer23.a
LIB_OSMOCORE = $(OSMOCOM)/src/shared/libosmocore/build-host/src/.libs/libosmocore.a
LIB_OSMOVTY = $(OSMOCOM)/src/shared/libosmocore/build-host/src/vty/.libs/libosmovty.a
LIB_OSMOGSM = $(OSMOCOM)/src/shared/libosmocore/build-host/src/gsm/.libs/libosmogsm.a
LIB_OSMOCODEC = $(OSMOCOM)/src/shared/libosmocore/build-host/src/codec/.libs/libosmocodec.a
LIB_MAIN = $(OSMOCOM)/src/host/layer23/src/misc/main.o

LIBS = $(LIB_MOBILE) $(LIB_LAYER23) $(LIB_OSMOCORE) $(LIB_OSMOVTY) $(LIB_OSMOGSM) $(LIB_OSMOCODEC)
# $(LIB_MAIN)


INCLUDES = -I$(OSMOCOM)/src/shared/libosmocore/include -I$(OSMOCOM)/src/host/layer23/include

#SUFFIXES:
#SUFFIXES: .c .o

PROGRAMS = show_tmsi sniff gsm_convert frame2bursts

all: $(PROGRAMS)

.c.o:
	cc -g $(INCLUDES) -c -o $@ $<

openbtsstuff/libopenbts.a:
	cd openbtsstuff; make

iniparser/libiniparser.a:
	cd iniparser; make

show_tmsi: main.o show_tmsi.o $(LIBS)
	cc -g main.o show_tmsi.o $(LIBS) -o show_tmsi

sniff: main.o sniff.o $(LIBS) l3.o crypto.o gsmstack.o cch.o interleave.o fire_crc.o common.o custom_ms.o openbtsstuff/libopenbts.a iniparser/libiniparser.a
	cc -g main.o sniff.o $(LIBS) l3.o crypto.o gsmstack.o cch.o interleave.o fire_crc.o common.o custom_ms.o openbtsstuff/libopenbts.a iniparser/libiniparser.a -lstdc++ -lsqlite3 -lrt -o sniff

gsm_convert:  gsm_convert.o crypto.o gsmstack.o cch.o interleave.o fire_crc.o common.o openbtsstuff/libopenbts.a iniparser/libiniparser.a
	cc -g gsm_convert.o crypto.o gsmstack.o cch.o interleave.o fire_crc.o common.o $(LIB_OSMOCORE) $(LIB_OSMOGSM) openbtsstuff/libopenbts.a iniparser/libiniparser.a -lstdc++ -lsqlite3  -o ../bin/gsm_convert

frame2bursts: frame2bursts.o common.o cch.o fire_crc.o interleave.o iniparser/libiniparser.a
	cc -g frame2bursts.o common.o cch.o fire_crc.o interleave.o iniparser/libiniparser.a -o frame2bursts

clean:
	rm -f *.o $(PROGRAMS)
	cd openbtsstuff ; make clean
	cd iniparser ; make veryclean

