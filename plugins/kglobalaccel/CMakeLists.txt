set(kglobalaccel_plugin_SRCS
    kglobalaccel_plugin.cpp
)

add_library(KF5GlobalAccelPrivateKWin OBJECT ${kglobalaccel_plugin_SRCS})
target_compile_definitions(KF5GlobalAccelPrivateKWin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF5GlobalAccelPrivateKWin KF5::GlobalAccelPrivate kwin)
