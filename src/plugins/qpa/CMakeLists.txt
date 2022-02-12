set(QPA_SOURCES
    backingstore.cpp
    eglhelpers.cpp
    eglplatformcontext.cpp
    integration.cpp
    main.cpp
    offscreensurface.cpp
    platformcursor.cpp
    screen.cpp
    window.cpp
)

include(ECMQtDeclareLoggingCategory)
ecm_qt_declare_logging_category(QPA_SOURCES HEADER logging.h IDENTIFIER KWIN_QPA CATEGORY_NAME kwin_qpa_plugin DEFAULT_SEVERITY Critical)

add_library(KWinQpaPlugin OBJECT ${QPA_SOURCES})
target_compile_definitions(KWinQpaPlugin PRIVATE QT_STATICPLUGIN)

target_link_libraries(KWinQpaPlugin PRIVATE
    Qt::CorePrivate
    Qt::GuiPrivate
    Qt::FontDatabaseSupportPrivate
    Qt::ThemeSupportPrivate
    Qt::EventDispatcherSupportPrivate
    Qt::ServiceSupportPrivate
    Freetype::Freetype # Must be after Qt5 platform support libs
    Fontconfig::Fontconfig
    kwin
)
