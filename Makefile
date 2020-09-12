CFLAGS += -std=c99 -D_POSIX_C_SOURCE=199309L -O3 -g -Wall -Wextra -Werror -Wno-type-limits
TIMEOUT ?= 10

INSTALL_DIR := /opt/interception

.PHONY: all
all: $(TARGETS)

remap: remap.c 
	$(CC) $(CFLAGS) $< -o interception-pipe-maricn-remap

.PHONY: clean
clean:
	rm -f interception-pipe-maricn-remap

.PHONY: install
install:
	# If you have run `make test` then do not forget to run `make clean` after. Otherwise you may install with debug logs on.
	install -D --strip -t $(INSTALL_DIR) interception-pipe-maricn-remap

.PHONY: test
test:
	CFLAGS=-DVERBOSE make
	make install
	timeout $(TIMEOUT) udevmon -c /etc/udevmon.yaml
