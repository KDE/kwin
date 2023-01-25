add_library(KWinQpaPlugin OBJECT)
set_property(TARGET KWinQpaPlugin PROPERTY POSITION_INDEPENDENT_CODE ON)
target_sources(KWinQpaPlugin PRIVATE
    backingstore.cpp backingstore.h
    eglhelpers.cpp eglhelpers.h
    eglplatformcontext.cpp eglplatformcontext.h
    integration.cpp integration.h
    main.cpp main.h
    offscreensurface.cpp offscreensurface.h
    platformcursor.cpp platformcursor.h
    screen.cpp screen.h
    window.cpp window.h
)

ecm_qt_declare_logging_category(KWinQpaPlugin
    HEADER logging.h
    IDENTIFIER KWIN_QPA
    CATEGORY_NAME kwin_qpa_plugin
    DEFAULT_SEVERITY Critical
)

target_compile_definitions(KWinQpaPlugin PRIVATE QT_STATICPLUGIN)

target_link_libraries(KWinQpaPlugin PRIVATE
    Qt::CorePrivate
    Qt::GuiPrivate
    Freetype::Freetype # Must be after Qt5 platform support libs
    Fontconfig::Fontconfig
    kwin
)
if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(KWinQpaPlugin PRIVATE
        Qt::FontDatabaseSupportPrivate
        Qt::ThemeSupportPrivate
        Qt::EventDispatcherSupportPrivate
        Qt::ServiceSupportPrivate
    )
endif()
