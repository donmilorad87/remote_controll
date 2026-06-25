CC = clang
CFLAGS = -Wall -Wextra -Werror -std=c11 -I../shared
OBJCFLAGS = -fobjc-arc
FRAMEWORKS = -framework Cocoa -framework CoreGraphics -framework CoreAudio \
             -framework AudioToolbox -framework Carbon
LDFLAGS = $(FRAMEWORKS) -lpthread

SOURCES = main.m server_net.c input_macos.c volume_macos.c
OBJECTS = $(notdir $(SOURCES:.c=.o))
OBJECTS := $(OBJECTS:.m=.o)
TARGET = remotecontrol-desktop

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.m
	$(CC) $(CFLAGS) $(OBJCFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)
