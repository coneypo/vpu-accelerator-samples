# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstgapiosd.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

## Plugin 1

# sources used to compile this plug-in
libgstgapiosd_la_SOURCES = gstgapiosd.cpp

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstgapiosd_la_CFLAGS = $(GST_CFLAGS) $(JSONC_CFLAGS) -I $(top_builddir)/gst-libs/
libgstgapiosd_la_CXXFLAGS = $(GST_CFLAGS)  $(JSONC_CFLAGS) -I $(top_builddir)/gst-libs/ -std=c++11
libgstgapiosd_la_LIBADD = $(GST_LIBS) $(JSONC_LIBS) $(top_builddir)/gst-libs/libgapiobject.la
libgstgapiosd_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstgapiosd_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gstgapiosd.h
