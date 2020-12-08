include_directories(${Qt5Core_PRIVATE_INCLUDE_DIRS})
include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})

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

set(QT5PLATFORMSUPPORT_LIBS
    Qt5::FontDatabaseSupportPrivate
    Qt5::ThemeSupportPrivate
    Qt5::EventDispatcherSupportPrivate
)

target_link_libraries(KWinQpaPlugin
    ${QT5PLATFORMSUPPORT_LIBS}
    ${FREETYPE_LIBRARIES} # Must be after QT5PLATFORMSUPPORT_LIBS
    Fontconfig::Fontconfig
    kwin
)
