# Makefile

TARGET = -Vgcc_ntox86_64
BUILD_PROFILE ?= 

CC = qcc $(TARGET)
LD = $(CC)

# compiler flags for build profiles
CFLAGS_release+=-O2
CFLAGS_debug+=-g -O0 -fno-builtin
CFLAGS_coverage+=-g -O0 -ftest-coverage -fprofile-arcs -nopipe -Wc,-auxbase-strip,$@
DEPS = -Wp,-MMD,$(@:%=%.d),-MT,$@

# generic compiler flags (which include build type flags)
CFLAGS_all+=$(CFLAGS_$(BUILD_PROFILE))
 
# binaries
BINS = ipc_sendfile ipc_receivefile

# default flags
CFLAGS += $(DEPS) -Wall -fmessage-length=0 $(CFLAGS_all) 

# make
all: $(BINS)

# make release 
release: CFLAGS+=$(CFLAGS_release)
release: $(BINS)

# make coverage (report)
coverage: CFLAGS+=$(CFLAGS_coverage)
coverage: $(BINS)

# make debug 
debug: CFLAGS += $(CFLAGS_debug) 
debug: $(BINS)

# dependencies
ipc_sendfile: ipc_sendfile.c ipc_common.c
ipc_receivefile: ipc_receivefile.c ipc_common.c

# make clean
clean:
	rm -f *.o  *.d  *.gcno $(BINS)

# make rebuild
rebuild: clean all
