########### next target ###############
include(ECMSetupVersion)

ecm_setup_version(${PROJECT_VERSION}
                  VARIABLE_PREFIX KWINEFFECTS
                  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kwineffects_version.h"
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KWinEffectsConfigVersion.cmake"
                  SOVERSION 13
)

### xrenderutils lib ###
set(kwin_XRENDERUTILS_SRCS
    kwinxrenderutils.cpp
    logging.cpp
)

add_library(kwinxrenderutils SHARED ${kwin_XRENDERUTILS_SRCS})
generate_export_header(kwinxrenderutils EXPORT_FILE_NAME kwinxrenderutils_export.h)
target_link_libraries(kwinxrenderutils
    PUBLIC
        Qt::Core
        Qt::Gui

        XCB::RENDER
        XCB::XCB
        XCB::XFIXES
)

set_target_properties(kwinxrenderutils PROPERTIES
    VERSION ${KWINEFFECTS_VERSION}
    SOVERSION ${KWINEFFECTS_SOVERSION}
)
set_target_properties(kwinxrenderutils PROPERTIES OUTPUT_NAME ${KWIN_NAME}xrenderutils)

install(TARGETS kwinxrenderutils EXPORT KWinEffectsTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

###  effects lib  ###
set(kwin_EFFECTSLIB_SRCS
    anidata.cpp
    kwinanimationeffect.cpp
    kwindeformeffect.cpp
    kwineffects.cpp
    kwinoffscreenquickview.cpp
    kwinquickeffect.cpp
    logging.cpp
)

add_library(kwineffects SHARED ${kwin_EFFECTSLIB_SRCS})
generate_export_header(kwineffects EXPORT_FILE_NAME kwineffects_export.h)
target_link_libraries(kwineffects
    PUBLIC
        KF5::ConfigCore
        KF5::CoreAddons
        KF5::WindowSystem
        XCB::XCB
    PRIVATE
        Qt::Quick
        KF5::Declarative
        kwinglutils
)
set_target_properties(kwineffects PROPERTIES
    VERSION ${KWINEFFECTS_VERSION}
    SOVERSION ${KWINEFFECTS_SOVERSION}
)
set_target_properties(kwineffects PROPERTIES OUTPUT_NAME ${KWIN_NAME}effects)

install(TARGETS kwineffects EXPORT KWinEffectsTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# kwingl(es)utils library
set(kwin_GLUTILSLIB_SRCS
    kwinglplatform.cpp
    kwingltexture.cpp
    kwinglutils.cpp
    kwinglutils_funcs.cpp
    kwineglimagetexture.cpp
    logging.cpp
)

add_library(kwinglutils SHARED ${kwin_GLUTILSLIB_SRCS})
generate_export_header(kwinglutils BASE_NAME kwinglutils EXPORT_FILE_NAME kwinglutils_export.h)
target_link_libraries(kwinglutils PUBLIC XCB::XCB KF5::CoreAddons KF5::ConfigCore KF5::WindowSystem epoxy::epoxy)
set_target_properties(kwinglutils PROPERTIES
    VERSION ${KWINEFFECTS_VERSION}
    SOVERSION ${KWINEFFECTS_SOVERSION}
    OUTPUT_NAME ${KWIN_NAME}glutils
)

install(TARGETS kwinglutils EXPORT KWinEffectsTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kwinconfig.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwineffects_export.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwinglutils_export.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwinxrenderutils_export.h
    kwinanimationeffect.h
    kwindeformeffect.h
    kwineffects.h
    kwinglobals.h
    kwinglplatform.h
    kwingltexture.h
    kwinglutils.h
    kwinglutils_funcs.h
    kwinoffscreenquickview.h
    kwinquickeffect.h
    kwinxrenderutils.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR} COMPONENT Devel)

set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KWinEffects")


install(FILES  "${CMAKE_CURRENT_BINARY_DIR}/KWinEffectsConfig.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/KWinEffectsConfigVersion.cmake"
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        COMPONENT Devel )

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/KWinEffectsConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/KWinEffectsConfig.cmake"
  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
)

install(EXPORT KWinEffectsTargets DESTINATION "${CMAKECONFIG_INSTALL_DIR}" FILE KWinEffectsTargets.cmake NAMESPACE KWinEffects:: )
