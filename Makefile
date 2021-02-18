CXXFLAGS += -std=c++20 -D_POSIX_C_SOURCE=199309L -O3 -g -Wall -Wextra -Werror -Wno-type-limits
TIMEOUT ?= 10

INSTALL_FILE := /opt/interception/interception-vimproved

# the build target executable:
TARGET = interception-vimproved

all: $(TARGET)

$(TARGET): $(TARGET).cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(TARGET).cpp

.PHONY: clean
clean:
	rm -f $(TARGET) $(TARGET).o

.PHONY: build
build:
	g++ -o interception-vimproved interception-vimproved.cpp

.PHONY: install
install:
	# If you have run `make test` then do not forget to run `make clean` after. Otherwise you may install with debug logs on.
	install -D --strip -T $(INSTALL_FILE) $(TARGET)

.PHONY: test
test:
	CXXFLAGS=-DVERBOSE make
	make install
	timeout $(TIMEOUT) udevmon -c /etc/udevmon.yaml
