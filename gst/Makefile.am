# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstapi2d.la 

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

## Plugin 1

# sources used to compile this plug-in
libgstapi2d_la_SOURCES = gstapi2d.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstapi2d_la_CFLAGS = $(GST_CFLAGS) $(JSONC_CFLAGS) -I $(top_builddir)/gst-libs/
libgstapi2d_la_LIBADD = $(GST_LIBS) $(JSONC_LIBS) $(top_builddir)/gst-libs/libgapiobject.la
libgstapi2d_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) 
libgstapi2d_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstapi2d.h 
