INCLUDES = $(all_includes) 

kde_module_LTLIBRARIES = libkwinriscos.la

libkwinriscos_la_SOURCES = \
Button.cpp \
CloseButton.cpp \
IconifyButton.cpp \
LowerButton.cpp \
Manager.cpp \
MaximiseButton.cpp \
StickyButton.cpp \
HelpButton.cpp \
Static.cpp
libkwinriscos_la_LIBADD = ../../kwin.la
libkwinriscos_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = \
Button.h \
CloseButton.h \
IconifyButton.h \
LowerButton.h \
MaximiseButton.h \
StickyButton.h \
HelpButton.h \
Manager.h \
Static.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = riscos.desktop

EXTRA_DIST = $(lnk_DATA)


