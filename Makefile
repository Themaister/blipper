TARGET := libblipper.a test_decimator

SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
HEADERS := $(wildcard *.h)

CFLAGS += -ansi -pedantic -Wall $(shell pkg-config sndfile --cflags)
LDFLAGS += -lm -Wl,-no-undefined $(shell pkg-config sndfile --libs)

ifeq ($(DEBUG), 1)
   CFLAGS += -O0 -g
else
   CFLAGS += -O2 -ffast-math -g
endif

all: $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

libblipper.a: $(OBJECTS)
	$(AR) rcs $@ $^

test_decimator: tests/decimator.o libblipper.a 
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJECTS) $(TARGET) tests/*.o

.PHONY: clean

