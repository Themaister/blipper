TARGET := libblipper.a test_decimator test_sawtooth test_square test_triangle

OBJECTS := blipper_float.o blipper_double.o blipper_fixed.o
HEADERS := $(wildcard *.h)

CFLAGS += -ansi -D_GNU_SOURCE -pedantic -Wall $(shell pkg-config sndfile --cflags) -DHAVE_STDINT_H -DBLIPPER_LOG_PERFORMANCE=1
LDFLAGS += -lm -Wl,-no-undefined $(shell pkg-config sndfile --libs)

ifeq ($(DEBUG), 1)
   CFLAGS += -O0 -g
else
   CFLAGS += -O2 -ffast-math -g
endif

all: $(TARGET)

blipper_float.o: blipper.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS) -DBLIPPER_FIXED_POINT=0 -DBLIPPER_REAL_T=float

blipper_double.o: blipper.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS) -DBLIPPER_FIXED_POINT=0 -DBLIPPER_REAL_T=double

blipper_fixed.o: blipper.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS) -DBLIPPER_FIXED_POINT=1

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS) -DBLIPPER_FIXED_POINT=1

libblipper.a: $(OBJECTS)
	$(AR) rcs $@ $^

test_decimator: tests/decimator.o libblipper.a 
	$(CC) -o $@ $^ $(LDFLAGS)

test_sawtooth: tests/sawtooth.o libblipper.a 
	$(CC) -o $@ $^ $(LDFLAGS)

test_square: tests/square.o libblipper.a 
	$(CC) -o $@ $^ $(LDFLAGS)

test_triangle: tests/triangle.o libblipper.a 
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJECTS) $(TARGET) tests/*.o

.PHONY: clean

