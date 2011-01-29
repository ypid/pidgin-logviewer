# Copyright (C) 2011 Tirtha Chaterjee <tirtha.p.chatterjee@gmail.com>
#  
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without 
# modifications, as long as this notice is preserved.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

localedir = $(datadir)/locale
pidgindir = $(libdir)/pidgin

pidgin_LTLIBRARIES = logplugin.la

logplugin_la_SOURCES = logplugin.c
logplugin_la_LDFLAGS = -shared -module -avoid-version -Wl,--as-needed
logplugin_la_LIBADD = $(GLIB_LIBS) $(GTK_LIBS) $(DBUS_LIBS) @LTLIBINTL@

AM_CPPFLAGS = \
        -DDATADIR=\"$(PIDGIN_DATADIR)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-W -Wall -Wno-unused-parameter \
	-std=gnu99 \
        $(PIDGIN_CFLAGS) \
        $(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(DEBUG_CFLAGS)
