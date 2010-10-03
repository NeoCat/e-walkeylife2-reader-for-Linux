CXXFLAGS = -Os
OBJS = show-history dec-dump usb-dump

all: $(OBJS)

usb-dump: usb-dump.cpp
	g++ -o usb-dump -Os usb-dump.cpp -lusb-1.0

clean:
	rm -f $(OBJS)
