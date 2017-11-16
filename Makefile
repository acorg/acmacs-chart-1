# -*- Makefile -*-
# Eugene Skepner 2017
# ----------------------------------------------------------------------

MAKEFLAGS = -w

# ----------------------------------------------------------------------

SOURCES = chart-base.cc chart.cc chart-plot-spec.cc bounding-ball.cc layout-base.cc layout.cc ace.cc lispmds.cc
PY_SOURCES = py.cc $(SOURCES)
BACKEND = $(DIST)/acmacs_chart_backend$(PYTHON_MODULE_SUFFIX)

# ----------------------------------------------------------------------

include $(ACMACSD_ROOT)/share/makefiles/Makefile.g++
include $(ACMACSD_ROOT)/share/makefiles/Makefile.dist-build.vars

PYTHON_VERSION = $(shell python3 -c 'import sys; print("{0.major}.{0.minor}".format(sys.version_info))')
PYTHON_CONFIG = python$(PYTHON_VERSION)-config
PYTHON_MODULE_SUFFIX = $(shell $(PYTHON_CONFIG) --extension-suffix)

ACMACS_CHART_LIB_MAJOR = 1
ACMACS_CHART_LIB_MINOR = 0
ACMACS_CHART_LIB = $(DIST)/$(call shared_lib_name,libacmacschart,$(ACMACS_CHART_LIB_MAJOR),$(ACMACS_CHART_LIB_MINOR))

CXXFLAGS = -g -MMD $(OPTIMIZATION) $(PROFILE) -fPIC -std=$(STD) $(WARNINGS) -Icc -I$(AD_INCLUDE) $(PKG_INCLUDES)
LDFLAGS = $(OPTIMIZATION) $(PROFILE)
LDLIBS = \
	$(AD_LIB)/$(call shared_lib_name,libacmacsbase,1,0) \
	$(AD_LIB)/$(call shared_lib_name,liblocationdb,1,0) \
	$$(pkg-config --libs liblzma) $(CXX_LIB)
PY_LDLIBS = $(LDLIBS) $(shell $(PYTHON_CONFIG) --ldflags | sed -E 's/-Wl,-stack_size,[0-9]+//')

PKG_INCLUDES = $(shell pkg-config --cflags liblzma) $(shell $(PYTHON_CONFIG) --includes)

# ----------------------------------------------------------------------

all: check-acmacsd-root install-headers $(ACMACS_CHART_LIB) $(BACKEND)

install: check-acmacsd-root install-headers $(ACMACS_CHART_LIB) $(BACKEND)
	$(call install_lib,$(ACMACS_CHART_LIB))
	ln -sf $(BACKEND) $(AD_PY)
	ln -sf $(abspath py)/* $(AD_PY)
	ln -sf $(abspath bin)/acmacs-chart-* $(AD_BIN)

test: install
	test/test

# ----------------------------------------------------------------------

-include $(BUILD)/*.d
include $(ACMACSD_ROOT)/share/makefiles/Makefile.dist-build.rules
include $(ACMACSD_ROOT)/share/makefiles/Makefile.rtags

# ----------------------------------------------------------------------

$(BACKEND): $(patsubst %.cc,$(BUILD)/%.o,$(PY_SOURCES)) | $(DIST)
	@printf "%-16s %s\n" "SHARED" $@
	@$(CXX) -shared $(LDFLAGS) -o $@ $^ $(PY_LDLIBS)

$(ACMACS_CHART_LIB): $(patsubst %.cc,$(BUILD)/%.o,$(SOURCES)) | $(DIST) $(LOCATION_DB_LIB)
	@printf "%-16s %s\n" "SHARED" $@
	@$(call make_shared,libacmacschart,$(ACMACS_CHART_LIB_MAJOR),$(ACMACS_CHART_LIB_MINOR)) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# ======================================================================
### Local Variables:
### eval: (if (fboundp 'eu-rename-buffer) (eu-rename-buffer))
### End:
