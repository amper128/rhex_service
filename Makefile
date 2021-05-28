LDFLAGS=-lm -lrt -lpcap
LDFLAGS=-lwiringPi -lm -lrt -lpcap
CPPFLAGS=-Wall -O2 -march=native -mtune=native -D _GNU_SOURCE
CPPFLAGS=-Wall -O2 -march=native -mtune=native -mfpu=vfp -mfloat-abi=hard -D _GNU_SOURCE
CFLAGS := $(CFLAGS) $(CPPFLAGS) -Iinclude/ -Wall -Wextra

all: rhex_service

rhex_service: src/canbus.o src/crc.o src/gps.o src/i2c.o src/ina226.o src/log.o \
    src/logger.o src/main.o src/minmea.o src/motion.o src/radiotap/radiotap_rc.o \
    src/rhex_telemetry.o src/rhex_rc.o src/sensors.o src/sharedmem.o src/spi.o \
    src/svc.o src/timerfd.o src/wfb_rx.o src/wfb_tx.o
	gcc -o $@ $^ $(LDFLAGS)

%.o: %.c Makefile
	gcc $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

clean:
	rm -f rhex_service *~ *.o */*.o */*/*.o
