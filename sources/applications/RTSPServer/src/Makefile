prefix=$(PREFIX)

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
STRIP=arm-linux-strip
MKDIR_P = /bin/mkdir -p

SYS=posix

OPT=-O2
INC=-I.
CFLAGS=-Wall $(XCFLAGS) $(INC) $(DEF) $(OPT)
LDFLAGS=-Wall -static $(XLDFLAGS)

bindir=$(prefix)/bin
sbindir=$(prefix)/sbin
BINDIR=$(DESTDIR)$(bindir)
SBINDIR=$(DESTDIR)$(sbindir)

LIBS_posix=
LIBS= $(LIBS_$(SYS)) $(XLIBS)

THREADLIB_posix= -lpthread
THREADLIB=$(THREADLIB_$(SYS))
SLIBS=$(THREADLIB) $(LIBS)

EXT_posix=
EXT=$(EXT_$(SYS))

OBJS =	RTSPServer.o \
		access_log.o \
		conf_parse.o \
		conf_scan.o \
		conversions.o \
		event.o \
		frame.o \
		http-auth.o \
		http.o \
		http.o \
		log.o \
		md5.o \
		pmsg.o \
		qos-analyzer.o \
		rtcp-parse.o \
		rtp.o \
		rtp-jpeg.o \
		rtp-rawaudio.o \
		rtsp.o \
		session.o \
		spook.o \
		stream.o \
		tcp.o \
		utils.o \
		live.o \
		V4L.o \
		JpegEnc.o \
		encoder-jpeg.o \
		OSS.o \
		encoder-alaw.o

all: progs

progs: RTSPServer

install: progs
	test -z "$(bindir)" || $(MKDIR_P) "$(bindir)"
	$(STRIP) RTSPServer$(EXT) 
	cp RTSPServer$(EXT) $(BINDIR)

clean:
	rm -f *.o RTSPServer$(EXT)

#$(CC) -c -o $@ $< $(CFLAGS)
%.o: %.c
	$(CC) -c $< $(CFLAGS)

RTSPServer: $(OBJS)
	$(CC) $(LDFLAGS) $^ $> -o $@$(EXT) $(SLIBS) $(LIBS) 
