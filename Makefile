TARGET := blipper

SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
HEADERS := $(wildcard *.h)

CFLAGS += -std=gnu99 -pedantic -Wall $(shell pkg-config sndfile --cflags)
LDFLAGS += -lm $(shell pkg-config sndfile --libs)

ifeq ($(DEBUG), 1)
   CFLAGS += -O0 -g
else
   CFLAGS += -O2 -ffast-math -g
endif

ifeq ($(PROFILING), 1)
   CFLAGS += -pg
   LDFLAGS += -pg
endif

all: $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean

