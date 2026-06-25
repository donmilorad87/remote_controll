CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -I../shared

GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0 2>/dev/null)
PULSE_CFLAGS = $(shell pkg-config --cflags libpulse 2>/dev/null)
PULSE_LIBS = $(shell pkg-config --libs libpulse 2>/dev/null)

CFLAGS += $(GTK_CFLAGS) $(PULSE_CFLAGS)
LDFLAGS = $(GTK_LIBS) $(PULSE_LIBS) -lX11 -lXtst -lpthread

SOURCES = main.c server_net.c input_x11.c volume_pulse.c tray.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = remotecontrol-desktop

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "=== Build OK: $(TARGET) ==="

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)
