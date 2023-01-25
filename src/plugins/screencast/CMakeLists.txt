add_library(KWinScreencastPlugin OBJECT)
target_sources(KWinScreencastPlugin PRIVATE
    eglnativefence.cpp eglnativefence.h
    main.cpp
    outputscreencastsource.cpp outputscreencastsource.h
    pipewirecore.cpp pipewirecore.h
    regionscreencastsource.cpp regionscreencastsource.h
    screencastmanager.cpp screencastmanager.h
    screencastsource.cpp screencastsource.h
    screencaststream.cpp screencaststream.h
    windowscreencastsource.cpp windowscreencastsource.h
)

ecm_qt_declare_logging_category(KWinScreencastPlugin
    HEADER kwinscreencast_logging.h
    IDENTIFIER KWIN_SCREENCAST
    CATEGORY_NAME kwin_screencast
    DEFAULT_SEVERITY Warning
)

target_compile_definitions(KWinScreencastPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KWinScreencastPlugin kwin PkgConfig::PipeWire)
