# Compiler stuff
CC     = gcc
CFLAGS = -O2 -g0 -std=c99 -I. \
 -Werror \
 -Wall \
 -Wextra \
 -pedantic-errors \
 -pedantic \
 -Wshadow \
 -Wmissing-noreturn \
 -Wmissing-declarations \
 -Wmissing-prototypes \
 -Wmissing-format-attribute \
 -Wpointer-arith \
 -Wwrite-strings \
 -Wformat \
 -Wformat-nonliteral \
 -Wformat-security \
 -Wswitch-default \
 -Winit-self \
 -Wundef \
 -Waggregate-return \
 -Wnested-externs \
 -Wno-unused-parameter

# Folders
SOURCES  = src
TARGET   = target
BUILDDIR = $(TARGET)/build
DEBSDIR  = $(TARGET)/debian
DESTDIR  = $(HOME)/.purple/plugins

PKG_CONFIG = pkg-config glib-2.0 json-glib-1.0 pidgin libsoup-2.4

# Artifacts
LIBNAME = budicons
MODULES = $(patsubst $(SOURCES)/%.c,$(BUILDDIR)/%.lo,$(wildcard $(SOURCES)/module-*.c) )

# Product information
BUILD_NAME=$(shell cat debian/control | grep 'Package' | sed 's/Package: //')
BUILD_VERSION=$(shell cat debian/changelog | head -n 1 | perl -pe 's/^.*\((.*)\).*\Z/\1/')


.PHONY: build
build: plugin


.PHONY: all
all: plugin


.PHONY: install
install: $(DESTDIR)/$(LIBNAME).so
$(DESTDIR)/$(LIBNAME).so: $(BUILDDIR)/$(LIBNAME).so
	install --mode 644 $(BUILDDIR)/$(LIBNAME).so $(DESTDIR)/$(LIBNAME).so


.PHONY: plugin
plugin: $(BUILDDIR)/$(LIBNAME).so
$(BUILDDIR)/$(LIBNAME).so: $(MODULES) $(BUILDDIR)/plugin.so
	cp $(BUILDDIR)/plugin.so $@


$(BUILDDIR)/%.so: $(BUILDDIR)/%.la
	@echo "\nBuild .so $< -> $@"
	cp $(BUILDDIR)/.libs/`basename $@` $@
	chmod a-x $@


$(BUILDDIR)/%.la: $(BUILDDIR)/%.lo
	@echo "\nBuild .la $< -> $@"
	libtool --quiet --mode=link $(CC) $(CFLAGS) -o $@ -rpath /usr/lib/purple-2 $< `$(PKG_CONFIG) --libs` -module -avoid-version $(MODULES)


$(BUILDDIR)/%.lo: $(SOURCES)/%.c
	@echo "\nBuild .lo $< -> $@"
	libtool --quiet --mode=compile $(CC) $(CFLAGS) -c -o $@ `$(PKG_CONFIG) --cflags` $<


.PHONY: clean
clean:
	-rm -rf $(TARGET) > /dev/null 2>&1 || true
