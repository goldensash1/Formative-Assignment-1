CC = gcc
CFLAGS = -Wall -Wextra -std=c11
SRC_DIR = src
BUILD_DIR = build
TARGET = library_tracker

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/blockchain.c $(SRC_DIR)/registry.c $(SRC_DIR)/crypto.c

# Homebrew OpenSSL on macOS is keg-only, so point at it explicitly if present.
OPENSSL_PREFIX := $(shell brew --prefix openssl@3 2>/dev/null)
ifeq ($(OPENSSL_PREFIX),)
    OPENSSL_PREFIX := $(shell brew --prefix openssl 2>/dev/null)
endif

ifneq ($(OPENSSL_PREFIX),)
    CFLAGS += -I$(OPENSSL_PREFIX)/include
    LDFLAGS += -L$(OPENSSL_PREFIX)/lib
endif

LDFLAGS += -lssl -lcrypto

# On Apple Silicon Macs where only an Intel (x86_64) Homebrew OpenSSL is
# installed, build an x86_64 binary (runs fine under Rosetta 2). Override
# with `make ARCH_FLAG=` if you have a native arm64 OpenSSL instead.
UNAME_ARCH := $(shell uname -m)
OPENSSL_ARCH := $(shell file $(OPENSSL_PREFIX)/lib/libcrypto.dylib 2>/dev/null | grep -o 'x86_64\|arm64')
ifeq ($(UNAME_ARCH),arm64)
    ifeq ($(OPENSSL_ARCH),x86_64)
        ARCH_FLAG = -arch x86_64
    endif
endif
CFLAGS += $(ARCH_FLAG)
LDFLAGS += $(ARCH_FLAG)

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
	rm -f chain.dat

distclean: clean
	rm -f ec_private.pem ec_public.pem

.PHONY: all run clean distclean
