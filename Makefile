# automatically generated from tmakegen
# DO NOT EDIT

# tconf might have generated a config.mk
-include config.mk

# a few standard variables
VERSION ?= $(shell git describe --tags --always 2>/dev/null || echo unknown)
NAME ?= tash
BUILDDIR ?= build
PREFIX ?= /usr/local
STATIC ?= yes
SHARED ?= no
CFLAGS ?= -Wall -Wextra
CFLAGS += -DVERSION='"$(VERSION)"'

ifeq ($(HAVE_MMD) $(HAVE_MP),yes yes)
	CFLAGS += -MMD -MP
endif

ifeq ($(V),1)
	Q =
else
	Q = @
endif

.PHONY : all
all :

.PHONY : install
install :

.PHONY : uninstall
uninstall :

# ==== tash target ====

ALL_tash = $(BUILDDIR)/tash/tash
SRC_tash = src/builtin.c src/error.c src/execute.c src/expand.c src/func.c src/globing.c src/init.c src/jobs.c src/lexer.c src/main.c src/mem.c src/parser.c src/prompt.c src/stub.c src/var.c
OBJ_tash = $(SRC_tash:%=$(BUILDDIR)/tash/%.o)
DEPS_tash = $(SRC_tash:%=$(BUILDDIR)/tash/%.d)

.PHONY : all-tash
all : all-tash
all-tash : $(ALL_tash)

# include dependencies files
-include $(DEPS_tash)

.PHONY : install-tash
install : install-tash
install-tash : all-tash
	@mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	cp $(ALL_tash) "$(DESTDIR)$(PREFIX)/bin"

.PHONY : uninstall-tash
uninstall : uninstall-tash
uninstall-tash :
	rm -f "$(DESTDIR)$(PREFIX)/bin/tash"

.PHONY : clean-tash
clean-tash :
	@echo "CLEAN $(BUILDDIR)/tash"
	$(Q)rm -fr "$(BUILDDIR)/tash"

$(BUILDDIR)/tash/%.c.o : %.c
	@mkdir -p "$(@D)"
	@echo "CC $<"
	$(Q)$(CC) $(CFLAGS) -Iinclude --std=c99 -D_POSIX_C_SOURCE=200809L -o $@ -c $<

$(BUILDDIR)/tash/tash : $(OBJ_tash)
	@mkdir -p "$(@D)"
	@echo "CCLD tash"
	$(Q)$(CC) $(CFLAGS) -Iinclude --std=c99 -D_POSIX_C_SOURCE=200809L $(LDFLAGS) -o $@ $^

.PHONY : targets
targets :
	@echo "====== tash targets ======"
	@echo "====== globals targets ======"
	@echo "all       : build every component"
	@echo "install   : install every component"
	@echo "uninstall : uninstall every component"
	@echo "clean     : clean every component"
	@echo "====== tash targets ======"
	@echo "all-tash       : build tash"
	@echo "install-tash   : install tash"
	@echo "uninstall-tash : uninstall tash"
	@echo "clean-tash     : clean tash"

Makefile : tmakegen tmake.sh
	@echo "regenerate Makefile"
	./tmakegen

.PHONY : clean
clean :
	@echo "CLEAN $(BUILDDIR)"
	$(Q)rm -fr "$(BUILDDIR)"
