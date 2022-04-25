CC := $(CROSS_COMPILE)gcc

# The LDFLAGS variable sets flags for linker
#  -lm   says to link in libm (the math library)
#  LDFLAGS = -lm

SRCS = $(wildcard *.c)
DEPS = $(wildcard ../socket/*.c)
TARGETS = $(patsubst %.c,%,$(SRCS))
override CFLAGS += -Wall

all: srv client

srv:
	$(CC) $(CFLAGS) test_socket_srv.c $(DEPS) -o $@

client:
	$(CC) $(CFLAGS) test_socket_clnt.c $(DEPS) -o $@

# Phony means not a "real" target, it doesn't build anything
# The phony target "clean" is used to remove all compiled object files.
.PHONY: clean
clean:
	@rm -rvf *.o ${TARGETS} srv client