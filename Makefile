# Path to root directory
ROOT ?= .

CC ?= gcc
AR ?= ar

ifeq ($(CFG),debug)
  CFLAGS += -g
else    
  CFLAGS += -O3 -DNDEBUG
endif
            
TARGET = pmu_sched
SRCDIR = $(ROOT)/src
OUTPUTDIR = $(ROOT)/lib


CFLAGS += -Wall -Wno-unused-function -Wno-unused-label -fno-strict-aliasing -D_REENTRANT -pthread -m64
CFLAGS += -I$(SRCDIR)

LDFLAGS += -L$(OUTPUTDIR) -l$(TARGET) -lpthread

TARGETLIB = $(OUTPUTDIR)/lib$(TARGET).a

# GCC thread-local storage (enable if supported on target architecture)
ifdef NOTLS
  DEFINES += -UTLS
else
  DEFINES += -DTLS
endif

MODULES := $(patsubst %.c,%.o,$(wildcard $(SRCDIR)/*.c))

.PHONY: all test clean

all:    $(TARGETLIB)

%.o:    %.c
	$(CC) $(CFLAGS)  -c -o $@ $<

$(TARGETLIB):     $(MODULES)
	$(AR) cru $@ $^

test:   $(TARGETLIB)
	$(MAKE) -C test


clean:
	rm -f $(TARGETLIB) $(SRCDIR)/*.o
	TARGET=clean $(MAKE) -C test
