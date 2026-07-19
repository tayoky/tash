TOP = $(CURDIR)
TMAKE_DIR = $(TOP)/make
include $(TMAKE_DIR)/tmake-init.mk

PROG = tash
VERSION := $(shell git describe --tags --always 2>/dev/null || echo unknown)
SRCS = $(wildcard src/*.c)
CFLAGS += -Iinclude --std=c99 -D_POSIX_C_SOURCE=200809L
CFLAGS += -DVERSION='"$(VERSION)"'

include $(TMAKE_DIR)/tmake-prog.mk
