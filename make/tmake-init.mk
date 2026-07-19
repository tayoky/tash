# makefile include with default values

MAKEFLAGS += --no-builtin-rules

TOP ?= $(CURDIR)
-include $(TOP)/config.mk

CC ?= cc
LD ?= ld
AS ?= as
PREFIX ?= /usr/local
BUILDDIR ?= $(CURDIR)/build
TMAKE_DIR ?= $(TOP)/make

ifeq ($(V),1)
	Q =
else
	Q = @
endif
