SRCPATH=.
prefix=/usr/local
exec_prefix=${prefix}
bindir=${exec_prefix}/bin
libdir=${exec_prefix}/lib
includedir=${prefix}/include
SYS_ARCH=X86_64
SYS=LINUX
CC=gcc
CFLAGS=-Wno-maybe-uninitialized -Wshadow -O3 -ffast-math -m64  -Wall -I. -I$(SRCPATH) -std=gnu99 -D_GNU_SOURCE -mpreferred-stack-boundary=6  -I/usr/local/include  -I/usr/local/include -fPIC -fomit-frame-pointer -fno-tree-vectorize
COMPILER=GNU
COMPILER_STYLE=GNU
DEPMM=-MM -g0
DEPMT=-MT
LD=gcc -o 
LDFLAGS=-m64  -lm -lpthread -ldl
LIBX264=libx264.a
AR=gcc-ar rc 
RANLIB=gcc-ranlib
STRIP=strip
INSTALL=install
AS=nasm
ASFLAGS= -I. -I$(SRCPATH) -DARCH_X86_64=1 -I$(SRCPATH)/common/x86/ -f elf64 -DSTACK_ALIGNMENT=64 -DPIC
RC=
RCFLAGS=
EXE=
HAVE_GETOPT_LONG=1
DEVNULL=/dev/null
PROF_GEN_CC=-fprofile-generate
PROF_GEN_LD=-fprofile-generate
PROF_USE_CC=-fprofile-use
PROF_USE_LD=-fprofile-use
HAVE_OPENCL=yes
CC_O=-o $@
default: cli
install: install-cli
SOSUFFIX=so
SONAME=libx264.so.157
SOFLAGS=-shared -Wl,-soname,$(SONAME)  -Wl,-Bsymbolic
default: lib-shared
install: install-lib-shared
LDFLAGSCLI = -ldl  -L/usr/local/lib -lavformat -lm -lz -lavcodec -pthread -lm -lz -lswresample -lm -lavutil -pthread -lm  -L/usr/local/lib -lswscale -lm -lavutil -pthread -lm 
CLI_LIBX264 = $(LIBX264)
