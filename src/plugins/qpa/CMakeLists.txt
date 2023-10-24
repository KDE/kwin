add_library(KWinQpaPlugin OBJECT)
target_sources(KWinQpaPlugin PRIVATE
    backingstore.cpp
    eglhelpers.cpp
    eglplatformcontext.cpp
    integration.cpp
    main.cpp
    offscreensurface.cpp
    platformcursor.cpp
    screen.cpp
    swapchain.cpp
    window.cpp
)

ecm_qt_declare_logging_category(KWinQpaPlugin
    HEADER logging.h
    IDENTIFIER KWIN_QPA
    CATEGORY_NAME kwin_qpa_plugin
    DEFAULT_SEVERITY Critical
)

target_compile_definitions(KWinQpaPlugin PRIVATE QT_STATICPLUGIN)

target_link_libraries(KWinQpaPlugin PRIVATE
    Qt::Concurrent
    Qt::CorePrivate
    Qt::GuiPrivate
    Freetype::Freetype # Must be after Qt6 platform support libs
    Fontconfig::Fontconfig
    kwin
)
