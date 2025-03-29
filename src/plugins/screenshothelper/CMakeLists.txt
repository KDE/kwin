#######################################
# Effect

set(screenshothelper_SOURCES
    main.cpp
    screenshothelper.cpp
)

kwin_add_builtin_effect(screenshothelper ${screenshothelper_SOURCES})
target_link_libraries(screenshothelper PRIVATE
    kwin
    Qt::DBus
)
