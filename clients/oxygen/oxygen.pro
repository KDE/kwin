HEADERS = oxrender.h oxygen.h dynamicbrush.h \
          tileset.h debug.h
SOURCES = oxrender.cpp oxygen.cpp tileset.cpp stylehint.cpp \
          sizefromcontents.cpp qsubcmetrics.cpp \
          pixelmetric.cpp stdpix.cpp \
          drawcomplexcontrol.cpp drawcontrol.cpp \
          drawprimitive.cpp dynamicbrush.cpp
TEMPLATE = lib
PLUGIN = true
RESOURCES = oxygen.qrc
CONFIG += qt x11 plugin
QT += qt3support opengl
VERSION       = 0.1
