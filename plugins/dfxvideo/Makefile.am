STRIP_FPIC = sh $(top_srcdir)/strip_fPIC.sh

SUFFIXES = .asm

.asm.lo:
	$(LIBTOOL) --tag=CC --mode=compile \
	$(STRIP_FPIC) $(NASM) -f elf -d ELF -I${srcdir}/ $<

AM_CPPFLAGS = -DLOCALE_DIR=\"${datadir}/locale/\" \
	-DDATADIR=\"${datadir}/psemu/\" \
	$(GTK2_CFLAGS) $(GLADE2_CFLAGS) \
	-I../../libpcsxcore \
	-I../../include

bindir = @libdir@/games/psemu/
libdir = @libdir@/games/psemu/

lib_LTLIBRARIES = libDFXVideo.la

libDFXVideo_la_SOURCES = gpu.c cfg.c draw.c fps.c key.c menu.c prim.c soft.c zn.c
if X86_NASM
libDFXVideo_la_SOURCES += i386.asm
AM_CPPFLAGS += -DUSE_NASM=1
endif
libDFXVideo_la_LDFLAGS = -module -avoid-version
libDFXVideo_la_LIBADD = -lX11 -lXv -lXext -lm

bin_PROGRAMS = cfgDFXVideo
cfgDFXVideo_SOURCES = gpucfg-0.1df/main.c
cfgDFXVideo_LDADD = $(GTK2_LIBS) $(GLADE2_LIBS) -lXext

glade_DATA = gpucfg-0.1df/dfxvideo.glade2
gladedir = $(datadir)/psemu/
EXTRA_DIST = $(glade_DATA)
