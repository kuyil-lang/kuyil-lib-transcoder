# Transcoder Utils Library Makefile
# Compatible with Makefile.libs structure

CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -fPIC -std=c99
LDFLAGS = -shared
LIBS = -lz -larchive
TARGET ?= ../../libs/libkyltranscoder.so

# Check for libarchive
HAS_ARCHIVE := $(shell pkg-config --exists libarchive 2>/dev/null && echo yes || echo no)

# Source files
SOURCES = transcoder_utils.c libkyltranscoder.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = transcoder_utils.h

# Test files
TEST_SOURCE = test_transcoder.c
TEST_BINARY = test_transcoder

# Installation directories
PREFIX = /usr/local
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include

.PHONY: all clean install uninstall test deps

all: check-deps $(TARGET)

# Install dependencies
deps:
	@echo "Installing transcoder library dependencies..."
	@echo "Run: sudo apt-get install libarchive-dev zlib1g-dev"
	@which apt-get > /dev/null 2>&1 && sudo apt-get install -y libarchive-dev zlib1g-dev || echo "Please install libarchive-dev and zlib1g-dev manually"

# Check for dependencies
check-deps:
ifeq ($(HAS_ARCHIVE),no)
	@echo "Error: libarchive not found."
	@echo "Install with: sudo apt-get install libarchive-dev"
	@echo "Or run: make -C additional_libs/transcoder deps"
	@exit 1
endif

# Build shared library
$(TARGET): $(OBJECTS)
	mkdir -p $(dir $(TARGET))
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

# Compile object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Build test program
test: $(TEST_BINARY)

$(TEST_BINARY): $(TEST_SOURCE) $(SHARED_LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_SOURCE) -L. -ltranscoder_utils $(LIBS)

# Run tests
check: test
	LD_LIBRARY_PATH=. ./$(TEST_BINARY)

# Install library and headers
install: $(TARGET)
	install -d $(LIBDIR)
	install -d $(INCLUDEDIR)
	install -m 644 $(TARGET) $(LIBDIR)
	install -m 644 $(HEADERS) $(INCLUDEDIR)
	ldconfig

# Uninstall library and headers
uninstall:
	rm -f $(LIBDIR)/libkyltranscoder.so
	rm -f $(INCLUDEDIR)/transcoder_utils.h
	ldconfig

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET) $(TEST_BINARY) libtranscoder_utils.so libtranscoder_utils.a

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: all

# Example usage
example:
	@echo "Example usage:"
	@echo "  make all          - Build both shared and static libraries"
	@echo "  make test         - Build test program"
	@echo "  make check        - Run tests"
	@echo "  make install      - Install library system-wide"
	@echo "  make clean        - Clean build files"

# Package info
info:
	@echo "Transcoder Utils Library"
	@echo "========================"
	@echo "Version: 1.0.0"
	@echo "Features:"
	@echo "  - GZIP compression/decompression"
	@echo "  - DEFLATE compression/decompression" 
	@echo "  - URL encoding/decoding"
	@echo "  - HTML entity encoding"
	@echo "  - Hex encoding/decoding"
	@echo "  - CRC32 checksums"
	@echo ""
	@echo "Dependencies:"
	@echo "  - zlib (for compression)"
	@echo "  - libarchive (for archive operations)"
	@echo ""
	@echo "Build targets:"
	@echo "  - $(SHARED_LIB) (shared library)"
	@echo "  - $(STATIC_LIB) (static library)"