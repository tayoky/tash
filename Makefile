TOP = $(CURDIR)
TMAKE_DIR = $(TOP)/make
include $(TMAKE_DIR)/tmake-init.mk

PACKAGE = tash
VERSION := $(shell git describe --tags --always 2>/dev/null || echo unknown)
SRCS = $(wildcard src/*.c)
CFLAGS += -Iinclude --std=c99 -D_POSIX_C_SOURCE=200809L
CFLAGS += -DVERSION='"$(VERSION)"'

include $(TMAKE_DIR)/tmake-prog.mk

FILES = COPYING.txt README.md
FILESDIR = $(DOCDIR)/tash
include $(TMAKE_DIR)/tmake-files.mk

include $(TMAKE_DIR)/tmake-locale.mk
