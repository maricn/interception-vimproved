PROG_NAME = interception-vimproved
BUILD_DIR = build
TARGET = $(BUILD_DIR)/$(PROG_NAME)
BIN_DIR = /opt/interception
BIN = $(BIN_DIR)/$(PROG_NAME)
PATH_DIR = /usr/local/bin
PATH_BIN = $(PATH_DIR)/$(PROG_NAME)
CONFIG_DIR = /etc/interception-vimproved
CONFIG = config.yaml

.PHONY: all
all: build

.PHONY: build
build:
	meson build
	ninja -C build

.PHONY: install
install: build
	install -D --strip -T $(TARGET) $(BIN)
	ln -sf $(BIN) $(PATH_BIN)
	mkdir -p $(CONFIG_DIR)
	cp --no-clobber $(CONFIG) $(CONFIG_DIR)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: uninstall
uninstall: clean
	rm -f $(BIN)
	rm -f $(PATH_BIN)
	rm -rf $(CONFIG_DIR)
