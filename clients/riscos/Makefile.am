INCLUDES = $(all_includes) 

lib_LTLIBRARIES = libkwinriscos.la

libkwinriscos_la_SOURCES = \
Button.cpp \
CloseButton.cpp \
DBWidget.cpp \
IconifyButton.cpp \
LowerButton.cpp \
Manager.cpp \
MaximiseButton.cpp \
ResizeBar.cpp \
ResizeMid.cpp \
ResizeSide.cpp \
TitleBar.cpp \
TitleText.cpp \
Static.cpp

METASOURCES = AUTO
noinst_HEADERS = \
Button.h \
CloseButton.h \
DBWidget.h \
IconifyButton.h \
LowerButton.h \
Manager.h \
MaximiseButton.h \
ResizeBar.h \
ResizeMid.h \
ResizeSide.h \
TitleBar.h \
TitleText.h \
Static.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = riscos.desktop

EXTRA_DIST = $(lnk_DATA)

libkwinriscos_la_LDFLAGS = $(all_libraries) -version-info 1:0:0 -module -rdynamic

