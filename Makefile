TARGET := libblipper.so

SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
HEADERS := $(wildcard *.h)

CFLAGS += -ansi -D_GNU_SOURCE -pedantic -Wall -fPIC
LDFLAGS += -lm -shared -Wl,-no-undefined

ifeq ($(DEBUG), 1)
   CFLAGS += -O0 -g
else
   CFLAGS += -O2 -ffast-math -g
endif

all: $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean

