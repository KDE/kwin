INCLUDES = $(all_includes) 

kde_module_LTLIBRARIES = libkwindefault.la

libkwindefault_la_SOURCES = \
Button.cpp \
CloseButton.cpp \
DBWidget.cpp \
IconifyButton.cpp \
StickyButton.cpp \
Manager.cpp \
MaximiseButton.cpp \
ResizeBar.cpp \
ResizeMid.cpp \
ResizeSide.cpp \
TitleBar.cpp \
TitleText.cpp \
Static.cpp
libkwindefault_la_LIBADD = ../../kwin.la
libkwindefault_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)

METASOURCES = AUTO
noinst_HEADERS = \
Button.h \
CloseButton.h \
DBWidget.h \
IconifyButton.h \
StickyButton.h \
Manager.h \
MaximiseButton.h \
ResizeBar.h \
ResizeMid.h \
ResizeSide.h \
TitleBar.h \
TitleText.h \
Static.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = kde.desktop

EXTRA_DIST = $(lnk_DATA)


