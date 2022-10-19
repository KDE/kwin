set(kglobalaccel_plugin_SRCS
    kglobalaccel_plugin.cpp
)

add_library(KF5GlobalAccelKWinPlugin OBJECT ${kglobalaccel_plugin_SRCS})
set_property(TARGET KF5GlobalAccelKWinPlugin PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(KF5GlobalAccelKWinPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF5GlobalAccelKWinPlugin KF5::GlobalAccelPrivate kwin)
