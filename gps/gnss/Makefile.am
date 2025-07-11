ACLOCAL_AMFLAGS = -I m4

AM_CFLAGS = \
     $(LOCPLA_CFLAGS) \
     $(LOCHAL_CFLAGS) \
     $(GPSUTILS_CFLAGS) \
     $(LOCCORE_CFLAGS) \
     $(LIBXML_CFLAGS) \
     -I./ \
     -I../location \
     -std=c++14

libgnss_la_SOURCES = \
    location_gnss.cpp \
    GnssAdapter.cpp \
    XtraSystemStatusObserver.cpp \
    Agps.cpp \
    XmlFileParser.cpp \
    NativeAgpsHandler.cpp

if USE_FEATURE_AUTOMOTIVE
AM_CFLAGS += -DFEATURE_AUTOMOTIVE
endif

if USE_TELSDK
AM_CFLAGS += -DUSE_FEATURE_TELSDK
endif

if USE_GLIB
libgnss_la_CFLAGS = -DUSE_GLIB $(AM_CFLAGS) @GLIB_CFLAGS@
libgnss_la_LDFLAGS = -lstdc++ -Wl,-z,defs -lpthread @GLIB_LIBS@ -shared -version-info 1:0:0
libgnss_la_CPPFLAGS = -DUSE_GLIB $(AM_CFLAGS) $(AM_CPPFLAGS) @GLIB_CFLAGS@
else
libgnss_la_CFLAGS = $(AM_CFLAGS)
libgnss_la_LDFLAGS = -Wl,-z,defs -lpthread -shared -version-info 1:0:0
libgnss_la_CPPFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS)
endif

libgnss_la_LIBADD = -lstdc++ -ldl $(GPSUTILS_LIBS) $(LOCCORE_LIBS)

#Create and Install libraries
lib_LTLIBRARIES = libgnss.la
