ifndef ICONS_PREFIX
ICONS_PREFIX=$(shell realpath icons)
endif

CFLAGS ?= -O2 -Wall -g

CF=$(shell pkg-config --cflags gtk+-3.0 appindicator3-0.1) -DICONS_PREFIX="$(ICONS_PREFIX)" $(CPPFLAGS) $(CFLAGS)
LF=$(LDFLAGS)
LL=$(shell pkg-config --libs gtk+-3.0 appindicator3-0.1) $(LDLISB)

BIN=loot
OBJS=loot.o icons.o
ICONS=icons/box-closed.png icons/box-opened.png icons/box-error.png icons/reload.png icons/quit.png

all: $(BIN)

icons.o: $(ICONS)
	ld -r -b binary -o $@ $(ICONS)

loot.o: loot.c
	$(CC) $(CF) -c $< -o $@

loot: $(OBJS)
	$(CC) $(CF) $(LF) $(OBJS) $(LL) -o $@

clean:
	rm -f $(OBJS) $(BIN)

.PHONY: all clean install
