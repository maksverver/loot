CFLAGS=`pkg-config --cflags gtk+-3.0` -Wall -O2 -std=c99 -g -Wno-deprecated-declarations
LDLIBS=`pkg-config --libs gtk+-3.0` 

BIN=loot
OBJS=loot.o icons.o
ICONS=icons/box-closed.png icons/box-opened.png icons/box-error.png icons/reload.png icons/quit.png

all: $(BIN)

icons.o: $(ICONS)
	ld -r -b binary -o $@ $(ICONS)

loot.o: loot.c
	$(CC) $(CFLAGS) -c $< -o $@

loot: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

clean:
	rm -f $(OBJS) $(BIN)

install: $(BIN)
	install -D -s $(BIN) "${DESTDIR}"/usr/bin/$(BIN)

.PHONY: all clean install
