TARGET := libblipper.a

SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
HEADERS := $(wildcard *.h)

CFLAGS += -ansi -pedantic -Wall
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
	$(AR) rcs $@ $^

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean

