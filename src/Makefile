ifeq ($(wildcard config.mk),)
$(warning config.mk does not exist, configuring.)
config.mk:
		sh ./configure
		$(MAKE) synchronator
endif

include config.mk

PREFIX ?= /usr/local
PROGRAM-PREFIX ?=
PROGRAM-SUFFIX ?=
output_devices=1

SRCS := synchronator.c volume.c volumeLinear.c volumeLog.c interfaces.c processData.c processAscii.c processNumeric.c verifyConfig.c mixerAlsa.c mods.c modVolumeResponseRange.c modVolumeResponseCondResize.c modCommandDynaudio.c modCommandDenonAVR.c
DEPS := config.mk common.h synchronator.h volume.h interfaces.h processData.h verifyConfig.h mixer.h mods.h

ifndef DISABLE_SERIAL
SRCS += interfaceSerial.c
output_devices=$((output_devices-1))
endif

ifdef ENABLE_I2C
SRCS += interfaceI2C.c
output_devices=$((output_devices+1))
endif

ifdef ENABLE_TCP
SRCS += interfaceTCP.c
output_devices=$((output_devices+1))
endif

ifndef DISABLE_MSQ
SRCS += messageQueue.c
DEPS += messageQueue.h
endif


#ifeq ($(shell test $(output_devices) -lt 1; echo $$?),0)
#	printf >&2 'FATAL: at least one output device must be set\n'
#	rm -f config.mk config.h
#	exit 1
#endif


# default target
all: synchronator

install: synchronator
		install -m 755 -d $(PREFIX)/bin
		install -m 755 $^ $(PREFIX)/bin/$(PROGRAM-PREFIX)$^$(PROGRAM-SUFFIX)

GITREV=$(shell git describe --tags)
GITCOMM=$(shell git describe --always)
DIRTY:=$(shell if ! git diff --quiet --exit-code; then echo -dirty; fi)
VERSION=$(GITREV)$(DIRTY) ($(GITCOMM))

__version_file:
	@if [ ! -f version.h -o "`cat .version 2>/dev/null`" != '$(VERSION)' ]; then \
		echo '#define PROGRAM_VERSION "$(VERSION)"' > version.h; \
	fi
	@echo '#include "version.h"' >> config.h

%.o: %.c $(DEPS)
		$(CC) -c $(CFLAGS) $< -DINCLUDE_CONFIG

synchronator.o: __version_file

OBJS := $(SRCS:.c=.o)
synchronator:  $(OBJS)
		$(CC) -o $@ $(OBJS) $(LDFLAGS)
		@echo now run make install

clean:
		rm -f synchronator version.h
		rm -f $(OBJS)

uninstall: synchronator
		rm -f $(PREFIX)/bin/$(PROGRAM-PREFIX)$^$(PROGRAM-SUFFIX)