CC := $(CROSS_COMPILE)gcc

# The LDFLAGS variable sets flags for linker
#  -lm   says to link in libm (the math library)
#  LDFLAGS = -lm

SRCS = $(wildcard *.c)
DEPS = $(wildcard ../socket/*.c ../srv/device.c ../srv/ubx.c)
LDLIBS  = -lpthread
TARGETS = $(patsubst %.c,%,$(SRCS))
override CFLAGS += -Wall

all: srv client

srv:
	$(CC) $(CFLAGS) test_socket_srv.c $(DEPS) -o $@ $(LDLIBS)

client:
	$(CC) $(CFLAGS) test_socket_clnt.c $(DEPS) -o $@ $(LDLIBS)

test_ack:
	$(CC) $(CFLAGS) test_ack.c $(DEPS) -o $@ $(LDLIBS)

test_read:
	$(CC) $(CFLAGS) test_read.c $(DEPS) -o $@ $(LDLIBS)

# Phony means not a "real" target, it doesn't build anything
# The phony target "clean" is used to remove all compiled object files.
.PHONY: clean
clean:
	@rm -rvf *.o ${TARGETS} srv client