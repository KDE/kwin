set(screencast_SOURCES
    eglnativefence.cpp
    main.cpp
    outputscreencastsource.cpp
    pipewirecore.cpp
    screencastmanager.cpp
    screencastsource.cpp
    screencaststream.cpp
    windowscreencastsource.cpp
)

ecm_qt_declare_logging_category(screencast_SOURCES
    HEADER kwinscreencast_logging.h
    IDENTIFIER KWIN_SCREENCAST
    CATEGORY_NAME kwin_screencast
    DEFAULT_SEVERITY Warning
)

add_library(KWinScreencastPlugin OBJECT ${screencast_SOURCES})
target_compile_definitions(KWinScreencastPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KWinScreencastPlugin kwin PkgConfig::PipeWire)
