PROG_NAME = interception-vimproved
BUILD_DIR = build
INSTALL_DIR = /opt/interception
TARGET = $(BUILD_DIR)/$(PROG_NAME)
INSTALL_FILE = $(INSTALL_DIR)/$(PROG_NAME)

.PHONY: all
all: build

.PHONY: build
build:
	meson build
	ninja -C build

.PHONY: install
install: build
	install -D --strip -T $(TARGET) $(INSTALL_FILE)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(INSTALL_FILE)
