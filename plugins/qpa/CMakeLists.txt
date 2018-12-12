include_directories(${Qt5Core_PRIVATE_INCLUDE_DIRS})
include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})

set(QPA_SOURCES
    abstractplatformcontext.cpp
    backingstore.cpp
    integration.cpp
    main.cpp
    nativeinterface.cpp
    platformcontextwayland.cpp
    platformcursor.cpp
    screen.cpp
    sharingplatformcontext.cpp
    window.cpp
)

include(ECMQtDeclareLoggingCategory)
ecm_qt_declare_logging_category(QPA_SOURCES HEADER logging.h IDENTIFIER KWIN_QPA CATEGORY_NAME kwin_qpa_plugin DEFAULT_SEVERITY Critical)

add_library(KWinQpaPlugin MODULE ${QPA_SOURCES})

set(QT5PLATFORMSUPPORT_LIBS
     Qt5FontDatabaseSupport::Qt5FontDatabaseSupport
     Qt5ThemeSupport::Qt5ThemeSupport
     Qt5EventDispatcherSupport::Qt5EventDispatcherSupport
)

target_link_libraries(KWinQpaPlugin
    kwin
    KF5::WaylandClient
    ${QT5PLATFORMSUPPORT_LIBS}
    Fontconfig::Fontconfig
    ${FREETYPE_LIBRARIES}
)

if(HAVE_WAYLAND_EGL)
    target_link_libraries(KWinQpaPlugin Wayland::Egl)
endif()

install(
    TARGETS
        KWinQpaPlugin
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/platforms/
)
