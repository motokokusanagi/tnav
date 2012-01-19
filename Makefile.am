include $(top_srcdir)/Makefile.inc
AM_CPPFLAGS = @NAVIT_CFLAGS@ -I$(top_srcdir)/navit -DMODULE=map_traffic
if PLUGINS
  modulemap_LTLIBRARIES = libmap_traffic.la
else
  noinst_LTLIBRARIES = libmap_traffic.la
endif
libmap_traffic_la_SOURCES = traffic.c traffic.h
libmap_traffic_la_LDFLAGS = -module -avoid-version @NAVIT_MODULE_LDFLAGS@
