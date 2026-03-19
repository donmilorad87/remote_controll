CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -I../shared
LDFLAGS = -lX11 -lXtst -lpulse -lgtk-3 -lgdk-3 -lgobject-2.0 -lglib-2.0 \
          -lcurl -ljansson -lpthread -lappindicator3-0.1

# Get GTK3 flags from pkg-config
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 appindicator3-0.1 2>/dev/null)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0 appindicator3-0.1 2>/dev/null)
PULSE_CFLAGS = $(shell pkg-config --cflags libpulse 2>/dev/null)
PULSE_LIBS = $(shell pkg-config --libs libpulse 2>/dev/null)

CFLAGS += $(GTK_CFLAGS) $(PULSE_CFLAGS)
LDFLAGS = $(GTK_LIBS) $(PULSE_LIBS) -lX11 -lXtst -lcurl -ljansson -lpthread

SOURCES = main.c input_x11.c volume_pulse.c client_net.c login_window.c tray.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = remotecontrol-desktop

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)
