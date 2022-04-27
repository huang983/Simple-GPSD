CC := $(CROSS_COMPILE)gcc

# The LDFLAGS variable sets flags for linker
#  -lm   says to link in libm (the math library)
#  LDFLAGS = -lm

SRCS = $(wildcard *.c)
SRCS += $(wildcard ../socket/*.c)
OBJS = $(SRCS:.c=.o)
TARGET = gpsd
override CFLAGS += -Wall

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) $(SRCS) -o $@

%.o: %.c
	$(CC) -c $< $(CFLAGS)

# Phony means not a "real" target, it doesn't build anything
# The phony target "clean" is used to remove all compiled object files.
.PHONY: clean
clean:
	@rm -rvf *.o ${TARGET}
