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

if(Qt5Core_VERSION VERSION_LESS "5.8.0")
    add_definitions(-DQ_FONTCONFIGDATABASE)
    set(QT5PLATFORMSUPPORT_LIBS Qt5PlatformSupport::Qt5PlatformSupport)
else()
    set(QT5PLATFORMSUPPORT_LIBS
        Qt5FontDatabaseSupport::Qt5FontDatabaseSupport
        Qt5ThemeSupport::Qt5ThemeSupport
        Qt5EventDispatcherSupport::Qt5EventDispatcherSupport
)
endif()

target_link_libraries(KWinQpaPlugin
    kwin
    KF5::WaylandClient
    ${QT5PLATFORMSUPPORT_LIBS}
    ${FONTCONFIG_LIBRARIES}
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
