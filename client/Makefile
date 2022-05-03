CC := $(CROSS_COMPILE)gcc

# The LDFLAGS variable sets flags for linker
#  -lm   says to link in libm (the math library)
#  LDFLAGS = -lm

SRCS = $(wildcard *.c)
DEPS = $(wildcard ../socket/*.c)
OBJS = $(SRCS:.c=.o)
TARGET = gps
override CFLAGS += -Wall

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) $(TARGET).c $(DEPS) -o $@


# Phony means not a "real" target, it doesn't build anything
# The phony target "clean" is used to remove all compiled object files.
.PHONY: clean
clean:
	@rm -rvf *.o ${TARGET}
