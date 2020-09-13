CFLAGS += -std=c99 -D_POSIX_C_SOURCE=199309L -O3 -g -Wall -Wextra -Werror -Wno-type-limits
TIMEOUT ?= 10

INSTALL_FILE := /opt/interception/interception-pipe-maricn-remap

# the build target executable:
TARGET = remap

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

.PHONY: clean
clean:
	rm -f $(TARGET) $(TARGET).o

.PHONY: install
install:
	# If you have run `make test` then do not forget to run `make clean` after. Otherwise you may install with debug logs on.
	install -D --strip -T $(INSTALL_FILE) $(TARGET)

.PHONY: test
test:
	CFLAGS=-DVERBOSE make
	make install
	timeout $(TIMEOUT) udevmon -c /etc/udevmon.yaml
