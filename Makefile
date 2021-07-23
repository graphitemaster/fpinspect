SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

CFLAGS := -Wall
CFLAGS += -Wextra
CFLAGS += -O2

all: fpinspect

fpinspect: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f $(OBJS) fpinspect

.PHONY: clean