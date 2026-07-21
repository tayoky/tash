# makefile include to manage .mo/.po/.pot files

LOCALES ?= $(wildcard locale/*.po)
LOCALES_MO ?= $(LOCALES:%.po=$(BUILDDIR)/%.mo)

all : $(LOCALES_MO)
$(BUILDDIR)/%.mo : %.po
	@mkdir -p "$(@D)"
	@echo "GEN $@"
	$(Q)msgfmt -o "$@" $^

.PHONY : generate_template
generate_template : $(BUILDDIR)/template.pot
$(BUILDDIR)/template.pot : $(SRCS)
	@mkdir -p "$(@D)"
	@echo "GEN template.pot"
	$(Q)xgettext --keyword=_ -o $@ $^
	$(Q)for I in $(LOCALES) ; do \
		msgmerge --update $$I $@; \
	done

.PHONY : install-locale
install : install-locale
install-locale : $(LOCALES)
	@mkdir -p "$(LOCALEDIR)/$(PROG)"
	@echo "INSTALL $(LOCALES_MO)"
	@cp $(LOCALES_MO) "$(LOCALEDIR)/$(PROG)"

.PHONY : uninstall-locale
uninstall : uninstall-locale
uninstall-locale :
	@echo "UNINSTALL $(LOCALEDIR)/$(PROG)"
	@rm -fr "$(LOCALEDIR)/$(PROG)"
