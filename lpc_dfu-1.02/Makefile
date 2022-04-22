# $Id: Makefile,v 1.9 2013/11/07 00:32:14 claudio Exp $

SHELL = /bin/bash
OSNAME := $(shell uname -s | cut -c 1-5)

ifneq ($(OSNAME),Linux)
	TARGETOS = WIN32
endif

#Nome del progetto (con cui verra` creato l'eseguibile)
PROJECT_NAME = lpcdfu

USE_DEBUG ?= no

#Aggiungere i sorgenti del progetto
SOURCE = \
	src/main.c \
	src/lpc_dfu.c \
	src/dfu.c \

#Aggiungere eventuali directory include
INCDIR = \
	-I ./inc \

#TRGT =

ifeq ($(OSNAME),Linux)
ifeq ($(TARGETOS),WIN32)
	TRGT ?= i686-w64-mingw32-
#	TRGT ?= i586-mingw32msvc-
endif
endif

# Toolchain definition.(non toccare)
CC = $(TRGT)gcc
OBJCOPY = $(TRGT)objcopy
OBJSIZE = $(TRGT)size
OBJDUMP = $(TRGT)objdump
NM = $(TRGT)nm
AR = $(TRGT)ar
AS = $(TRGT)gcc -x assembler-with-cpp
RM = rm
SEVENZ ?= 7z

ifeq ($(USE_DEBUG),yes)
	DBGFLAG = -ggdb
	OPTFLAG = -O0
	O_DIR = Debug
else
	DBGFLAG =
	OPTFLAG = -O2 -fomit-frame-pointer
	O_DIR = Release
endif

ifeq ($(TARGETOS),WIN32)
	OUTDIR = $(O_DIR)Win32
else
	OUTDIR = $(O_DIR)
endif

PRJ_TARGET = $(OUTDIR)/$(PROJECT_NAME)

ifeq ($(TARGETOS),WIN32)
	PRJ_TARGET_EXT = $(PRJ_TARGET).exe
else
	PRJ_TARGET_EXT = $(PRJ_TARGET)
endif

CSTDFLAG = -std=gnu99

#Aggiungere librerie addizionali
LIBS =
#LIBS += -lm

#Aggiungere opzioni personalizzate per il linker
LDFLAGS = 
#LDFLAGS += -Xlinker --print-gc-sections

#Aggiungere simboli #Define personalizzati
DDEFS = \
		-D ENABLE_MYLOG=1 \

# Define C warning options here
CWARN =	\
		-Wall \
		-Wextra \
		-Wmissing-prototypes \
		-Wmissing-declarations \
		-Wredundant-decls \
		-Wundef \
		-Wunreachable-code \
		-Wunknown-pragmas \
		-Winline \
		-Wstrict-prototypes \

# Compiler flags definition. (non toccare)
CFLAGS = \
		$(DBGFLAG) \
		$(OPTFLAG) \
		$(CSTDFLAG) \
		$(CWARN) \
		$(INCDIR) \
		$(DDEFS) \

#		-Wa,-ahlms=$(@:.o=.lst)


ifeq ($(OSNAME),Linux)
USBLIB_CFLAGS ?= $(shell pkg-config --cflags libusb-1.0)
USBLIB_LDFLAGS ?= $(shell pkg-config --libs libusb-1.0)
else
USBLIB_CFLAGS ?= -I$(HOME)/include
USBLIB_LDFLAGS ?= -L$(HOME)/dll -lusb-1.0
endif

CFLAGS += $(USBLIB_CFLAGS)
LDFLAGS += $(USBLIB_LDFLAGS)

DEFAULT_MAKE_TARGET = 
#DEFAULT_MAKE_TARGET += begin gccversion
DEFAULT_MAKE_TARGET += $(PRJ_TARGET_EXT)
#DEFAULT_MAKE_TARGET += end

#Questa deve essere l'ultima riga del file
include Makefile.rules
