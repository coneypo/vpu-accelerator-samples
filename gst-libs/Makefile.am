noinst_LTLIBRARIES = libgapiobject.la 

libgapiobject_la_SOURCES = gapiobject.c 
#nodist_EXTRA_libgapiobject_la_SOURCES = dummy.cxx

libgapiobject_la_CFLAGS = $(GST_CFLAGS)  $(JSONC_CFLAGS)
libgapiobject_la_LIBADD = $(GST_LIBS) $(JSONC_LIBS)
libgapiobject_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)  

# headers we need but don't want installed
noinst_HEADERS = gapiobject.h 
