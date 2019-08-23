noinst_LTLIBRARIES = libgapiobject.la

libgapiobject_la_SOURCES = gapiobject.cpp gapiobjecttext.cpp jsonHandle.cpp gapiobjectrectangle.cpp
#nodist_EXTRA_libgapiobject_la_SOURCES = dummy.cxx

libgapiobject_la_CFLAGS = $(GST_CFLAGS)  $(JSONC_CFLAGS)
libgapiobject_la_CXXFLAGS = $(GST_CFLAGS)  $(JSONC_CFLAGS) -std=c++11 -I/usr/local/include/opencv4/
libgapiobject_la_LIBADD = $(GST_LIBS) $(JSONC_LIBS) -lstdc++ -L/usr/local/lib  -lopencv_imgproc -lopencv_gapi -lopencv_core -lopencv_video -lopencv_highgui
libgapiobject_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gapiobject.h gapiobjecttext.h jsonHandle.h common.h gapiobjectrectangle.h
