#
# Makefile for a Video Disk Recorder plugin
#
# -- based on pvrinput v20071028, Winfried Koehler --

#SET_VIDEO_WINDOW=1

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.
# IMPORTANT: the presence of this macro is important for the Make.config
# file. So it must be defined, even if it is not used here!
#
PLUGIN = pvr350

### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'static const char \*VERSION *=' pvr350.c | awk '{ print $$6 }' | sed -e 's/[";]//g')

LIBS = -ltwolame -la52 -lm -lmpg123

### The C++ compiler and options:

CXX      ?= g++
CXXFLAGS ?= -fPIC -g -O2 -Wall -Woverloaded-virtual

### The directory environment:

DVBDIR = ../../../../DVB
VDRDIR = ../../..
LIBDIR = ../../lib
TMPDIR = /tmp

### Test whether VDR has locale support
VDRLOCALE = $(shell grep 'I18N_DEFAULT_LOCALE' $(VDRDIR)/i18n.h)

### Allow user defined options to overwrite defaults:

-include $(VDRDIR)/Make.config

### The version number of VDR's plugin API (taken from VDR's "config.h"):

APIVERSION = $(shell sed -ne '/define APIVERSION/s/^.*"\(.*\)".*$$/\1/p' $(VDRDIR)/config.h)

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### Includes and Defines (add further entries here):

INCLUDES += -I$(VDRDIR)/include -I$(DVBDIR)/include

DEFINES += -D_GNU_SOURCE -DPLUGIN_NAME_I18N='"$(PLUGIN)"' -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE

ifdef SET_VIDEO_WINDOW
	DEFINES += -DSET_VIDEO_WINDOW
endif

### The object files (add further files here):

OBJS = $(PLUGIN).o $(PLUGIN)device.o $(PLUGIN)osd.o $(PLUGIN)audio.o $(PLUGIN)tools.o $(PLUGIN)setup.o $(PLUGIN)menu.o
### The main target:

all:  i18n libvdr-$(PLUGIN).so

### Implicit rules:

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) $<

### Dependencies:

MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(subst i18n.c,,$(OBJS:%.o=%.c)) > $@

-include $(DEPFILE)

### Internationalization (I18N):

PODIR     = po
LOCALEDIR = $(VDRDIR)/locale
I18Npo    = $(wildcard $(PODIR)/*.po)
I18Nmo    = $(addsuffix .mo, $(foreach file, $(I18Npo), $(basename $(file))))
I18Ndirs  = $(notdir $(foreach file, $(I18Npo), $(basename $(file))))
I18Npot   = $(PODIR)/$(PLUGIN).pot
I18Nvdrmo = vdr-$(PLUGIN).mo
ifeq ($(strip $(APIVERSION)),1.5.7)
  I18Nvdrmo = $(PLUGIN).mo
endif

ifneq ($(strip $(VDRLOCALE)),)
### do gettext based i18n stuff

%.mo: %.po
	msgfmt -c -o $@ $<

$(I18Npot): $(subst i18n.c,,$(wildcard *.c))
	xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --msgid-bugs-address='<see README>' -o $@ $(subst i18n.c,,$(wildcard *.c))

$(I18Npo): $(I18Npot)
	msgmerge -U --no-wrap --no-location --backup=none -q $@ $<

i18n: $(I18Nmo)
	@mkdir -p $(LOCALEDIR)
	for i in $(I18Ndirs); do\
	    mkdir -p $(LOCALEDIR)/$$i/LC_MESSAGES;\
	    cp $(PODIR)/$$i.mo $(LOCALEDIR)/$$i/LC_MESSAGES/$(I18Nvdrmo);\
 	    done

i18n.c: ### nothing to do

else ### do i18n.c based i18n stuff

OBJS += i18n.o

i18n:   $(@cp $(PODIR)/i18n.h i18n.h)
	@### nothing to do

i18n.h: i18n.c

### i18n compatibility generator:

i18n.c: $(PODIR)/i18n-template.c po2i18n.pl $(I18Npo)
	./po2i18n.pl < $(PODIR)/i18n-template.c > i18n.c
	@cp $(PODIR)/i18n.h i18n.h

endif


### Targets:

libvdr-$(PLUGIN).so: $(OBJS)
	$(CXX) $(CXXFLAGS) -shared $(OBJS) -o $@ $(LIBS)
	@cp --remove-destination $@ $(LIBDIR)/$@.$(APIVERSION)

dist: clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(OBJS) $(DEPFILE) i18n.c  i18n.h *.so *.tgz core* *~
	@-rm -f $(PODIR)/*.mo $(PODIR)/*.pot $(PODIR)/*~
