# see LICENSE for copyright and license details
PREFIX = /usr/local
CC = cc
CFLAGS ?= -O2 -Wall -Wextra -DDELVE_USE_READLINE
LDFLAGS ?= -lreadline
OBJ = delve.o
BIN = delve
CONF = delve.conf

default: $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)

.PHONY: clean
clean:
	@rm -f $(BIN) $(OBJ)

install: default
	@mkdir -p $(DESTDIR)$(PREFIX)/bin/
	@install $(BIN) $(DESTDIR)$(PREFIX)/bin/${BIN}
	@mkdir -p $(DESTDIR)$(PREFIX)/etc
	@install $(CONF) $(DESTDIR)$(PREFIX)/etc/${CONF}

uninstall:
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
	@rm -f $(DESTDIR)$(PREFIX)/etc/$(CONF)
