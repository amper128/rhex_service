LDFLAGS=-lm -lrt -lpcap
LDFLAGS=-lwiringPi -lm -lrt -lpcap
CPPFLAGS=-Wall -O2 -march=native -mtune=native -D _GNU_SOURCE
CPPFLAGS=-Wall -O2 -march=native -mtune=native -mfpu=vfp -mfloat-abi=hard -D _GNU_SOURCE
CFLAGS := $(CFLAGS) $(CPPFLAGS) -Iinclude/ -Wall -Wextra -Werror

all: rhex_service

rhex_service: \
    src/system/crc.o src/system/i2c.o src/system/log.o \
    src/system/logger.o src/system/netlink.o src/system/sharedmem.o \
    src/system/spi.o src/system/svc.o src/system/timerfd.o \
    src/radiotap/radiotap_rc.o \
    src/wfb/fec.o src/wfb/wfb_rx.o src/wfb/wfb_tx.o src/wfb/wfb_tx_rawsock.o \
    src/sensors/gps.o src/sensors/ina226.o src/sensors/minmea.o src/sensors/sensors.o \
    src/control/camera.o src/control/canbus.o src/control/motion.o \
    src/control/rhex_rc.o src/control/rhex_telemetry.o src/control/rssi_tx.o \
    src/main.o
	gcc -o $@ $^ $(LDFLAGS)

%.o: %.c Makefile
	gcc $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

clean:
	rm -f rhex_service *~ *.o */*.o */*/*.o
