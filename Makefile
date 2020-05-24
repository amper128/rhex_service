LDFLAGS=-lm -lrt
LDFLAGS=-lwiringPi -lm -lrt
CPPFLAGS=-Wall -O2 -march=native -mtune=native -D _GNU_SOURCE
CPPFLAGS=-Wall -O2 -march=native -mtune=native -mfpu=vfp -mfloat-abi=hard -D _GNU_SOURCE
CFLAGS := $(CFLAGS) $(CPPFLAGS) -Iinclude/

all: rhex_service

rhex_service: canbus.o crc.o gps.o log.o main.o minmea.o motion.o rhex_telemetry.o rhex_rc.o sensors.o sharedmem.o timerfd.o wfb_tx.o
	gcc -o $@ $^ $(LDFLAGS)

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

clean:
	rm -f rhex_service *~ *.o
