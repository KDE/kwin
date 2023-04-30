########### next target ###############
include(ECMSetupVersion)

ecm_setup_version(${PROJECT_VERSION}
                  VARIABLE_PREFIX KWINEFFECTS
                  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kwineffects_version.h"
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KWinEffectsConfigVersion.cmake"
                  SOVERSION 14
)

###  effects lib  ###
set(kwin_EFFECTSLIB_SRCS
    anidata.cpp
    kwinanimationeffect.cpp
    kwineffects.cpp
    kwinoffscreeneffect.cpp
    kwinoffscreenquickview.cpp
    kwinquickeffect.cpp
    logging.cpp
)

add_library(kwineffects SHARED ${kwin_EFFECTSLIB_SRCS})
generate_export_header(kwineffects EXPORT_FILE_NAME kwineffects_export.h)
target_link_libraries(kwineffects
    PUBLIC
        KF6::ConfigCore
        KF6::CoreAddons
        KF6::WindowSystem
        XCB::XCB
    PRIVATE
        Qt::GuiPrivate
        Qt::Quick
        KF6::I18n
        kwinglutils
)
target_include_directories(kwineffects INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}/kwin>")
set_target_properties(kwineffects PROPERTIES
    VERSION ${KWINEFFECTS_VERSION}
    SOVERSION ${KWINEFFECTS_SOVERSION}
)

install(TARGETS kwineffects EXPORT KWinEffectsTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# kwingl(es)utils library
set(kwin_GLUTILSLIB_SRCS
    colorspace.cpp
    kwineglimagetexture.cpp
    kwinglplatform.cpp
    kwingltexture.cpp
    kwinglutils.cpp
    kwinglutils_funcs.cpp
    logging.cpp
    openglcontext.cpp
    rendertarget.cpp
    renderviewport.cpp
)

add_library(kwinglutils SHARED ${kwin_GLUTILSLIB_SRCS})
generate_export_header(kwinglutils BASE_NAME kwinglutils EXPORT_FILE_NAME kwinglutils_export.h)
target_link_libraries(kwinglutils PUBLIC XCB::XCB KF6::CoreAddons KF6::ConfigCore KF6::WindowSystem epoxy::epoxy)
target_include_directories(kwinglutils INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}/kwin>")
set_target_properties(kwinglutils PROPERTIES
    VERSION ${KWINEFFECTS_VERSION}
    SOVERSION ${KWINEFFECTS_SOVERSION}
)

install(TARGETS kwinglutils EXPORT KWinEffectsTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kwinconfig.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwineffects_export.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwinglutils_export.h
    colorspace.h
    kwinanimationeffect.h
    kwineffects.h
    kwinglobals.h
    kwinglplatform.h
    kwingltexture.h
    kwinglutils.h
    kwinglutils_funcs.h
    kwinoffscreeneffect.h
    kwinoffscreenquickview.h
    kwinquickeffect.h
    rendertarget.h
    renderviewport.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin/libkwineffects COMPONENT Devel)

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
