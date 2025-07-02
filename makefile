include config.mk

BUILDDIR   = build
SRCDIR     = src
INCLUDEDIR = include

VERSION = $(shell git describe --tags --always)

SRC = $(shell find $(SRCDIR) -name "*.c")
OBJ = $(SRC:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

CFLAGS += -I$(INCLUDEDIR)
CFLAGS += -DTASH_VERSION='"$(VERSION)"'

all : $(BUILDDIR)/tash

test : $(BUILDDIR)/tash
	@$(CC) -o $@ $^

$(BUILDDIR)/tash : $(OBJ)
	@echo '[linking into $@]'
	@mkdir -p $(shell dirname $@)
	@$(CC) -o $@ $^


$(BUILDDIR)/%.o : $(SRCDIR)/%.c 
	@echo '[compiling $^]'
	@mkdir -p $(shell dirname $@)
	@$(CC) -o $@ -c $^ $(CFLAGS)

install : all
	@echo '[installing tash]'
	@mkdir -p $(PREFIX)/bin
	@cp $(BUILDDIR)/tash $(PREFIX)/bin/tash

uninstall :
	rm -f $(PREFIX)/bin/tash

clean :
	rm -fr build

config.mk :
	$(error run ./configure before running make)

.PHONY : all $(BUILDIR)/tash install uninstall clean
