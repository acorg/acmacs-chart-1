# -*- Makefile -*-
# Eugene Skepner 2016

# ----------------------------------------------------------------------

MAKEFLAGS = -w

# ----------------------------------------------------------------------

SOURCES = chart-base.cc chart.cc bounding-ball.cc layout-base.cc layout.cc ace.cc lispmds.cc
PY_SOURCES = py.cc $(SOURCES)
BACKEND = $(DIST)/acmacs_chart_backend$(PYTHON_MODULE_SUFFIX)

# ----------------------------------------------------------------------

TARGET_ROOT=$(shell if [ -f /Volumes/rdisk/ramdisk-id ]; then echo /Volumes/rdisk/AD; else echo $(ACMACSD_ROOT); fi)
include $(TARGET_ROOT)/share/Makefile.g++
include $(TARGET_ROOT)/share/Makefile.dist-build.vars

PYTHON_VERSION = $(shell python3 -c 'import sys; print("{0.major}.{0.minor}".format(sys.version_info))')
PYTHON_CONFIG = python$(PYTHON_VERSION)-config
PYTHON_MODULE_SUFFIX = $(shell $(PYTHON_CONFIG) --extension-suffix)

ACMACS_CHART_LIB = $(DIST)/libacmacschart.so

CXXFLAGS = -g -MMD $(OPTIMIZATION) $(PROFILE) -fPIC -std=$(STD) $(WEVERYTHING) $(WARNINGS) -Icc -I$(AD_INCLUDE) $(PKG_INCLUDES)
LDFLAGS = $(OPTIMIZATION) $(PROFILE)
LDLIBS = -L$(AD_LIB) -lacmacsbase -llocationdb -lboost_filesystem -lboost_system $$(pkg-config --libs liblzma)
PY_LDLIBS = $(LDLIBS) $(shell $(PYTHON_CONFIG) --ldflags | sed -E 's/-Wl,-stack_size,[0-9]+//')

PKG_INCLUDES = $(shell pkg-config --cflags liblzma) $(shell $(PYTHON_CONFIG) --includes)

# ----------------------------------------------------------------------

all: check-acmacsd-root install-headers $(ACMACS_CHART_LIB) $(BACKEND)

install: check-acmacsd-root install-headers $(ACMACS_CHART_LIB) $(BACKEND)
	ln -sf $(ACMACS_CHART_LIB) $(AD_LIB)
	if [ $$(uname) = "Darwin" ]; then /usr/bin/install_name_tool -id $(AD_LIB)/$(notdir $(ACMACS_CHART_LIB)) $(AD_LIB)/$(notdir $(ACMACS_CHART_LIB)); fi
	ln -sf $(BACKEND) $(AD_PY)
	ln -sf $(abspath py)/* $(AD_PY)
	ln -sf $(abspath bin)/acmacs-chart-* $(AD_BIN)

install-headers: check-acmacsd-root
	if [ ! -d $(AD_INCLUDE)/acmacs-chart ]; then mkdir $(AD_INCLUDE)/acmacs-chart; fi
	for h in $(abspath cc)/*.hh; do if [ ! -f $(AD_INCLUDE)/acmacs-chart/$$(basename $$h) ]; then ln -sf $$h $(AD_INCLUDE)/acmacs-chart; fi; done

test: install
	test/test

include $(AD_SHARE)/Makefile.rtags

# ----------------------------------------------------------------------

-include $(BUILD)/*.d

# ----------------------------------------------------------------------

$(BACKEND): $(patsubst %.cc,$(BUILD)/%.o,$(PY_SOURCES)) | $(DIST)
	@echo "SHARED     " $@ # '<--' $^
	@$(CXX) -shared $(LDFLAGS) -o $@ $^ $(PY_LDLIBS)

$(ACMACS_CHART_LIB): $(patsubst %.cc,$(BUILD)/%.o,$(SOURCES)) | $(DIST) $(LOCATION_DB_LIB)
	@echo "SHARED     " $@ # '<--' $^
	@$(CXX) -shared $(LDFLAGS) -o $@ $^ $(LDLIBS)

# ----------------------------------------------------------------------

$(BUILD)/%.o: cc/%.cc | $(BUILD) install-headers
	@echo $(CXX_NAME) $<
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

# ----------------------------------------------------------------------

include $(AD_SHARE)/Makefile.dist-build.rules

# ======================================================================
### Local Variables:
### eval: (if (fboundp 'eu-rename-buffer) (eu-rename-buffer))
### End:
