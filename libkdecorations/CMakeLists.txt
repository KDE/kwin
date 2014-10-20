# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"libkdecorations\")

# kdecorations library
set(kdecorations_LIB_SRCS
   kdecoration.cpp
   kdecoration_p.cpp
   kdecoration_plugins_p.cpp
   kdecorationfactory.cpp
   kcommondecoration.cpp
   kcommondecoration_p.cpp )

add_library(kdecorations SHARED ${kdecorations_LIB_SRCS})
generate_export_header(kdecorations EXPORT_FILE_NAME kdecorations_export.h)
target_link_libraries(kdecorations
    PUBLIC
        Qt5::Widgets
        KF5::ConfigCore
        KF5::Service # KPluginLoader
        KF5::WindowSystem # NETWM defines
    PRIVATE
        KF5::I18n
)
# target_link_libraries(kdecorations LINK_INTERFACE_LIBRARIES ${KDE4_KDEUI_LIBS})

# create a Config.cmake and a ConfigVersion.cmake file and install them
include(ECMPackageConfigHelpers)
include(ECMSetupVersion)

ecm_setup_version(${PROJECT_VERSION}
                  VARIABLE_PREFIX KDECORATIONS
                  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kdecorations_version.h"
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KDecorationsConfigVersion.cmake"
                  SOVERSION ${PROJECT_VERSION_MAJOR}
)

set_target_properties(kdecorations PROPERTIES
   VERSION ${KDECORATIONS_VERSION_STRING}
   SOVERSION ${KDECORATIONS_SOVERSION}
   )
install(TARGETS kdecorations EXPORT kdecorationsTargets ${INSTALL_TARGETS_DEFAULT_ARGS} )

set(CMAKECONFIG_INSTALL_DIR "${CMAKECONFIG_INSTALL_PREFIX}/KDecorations")

ecm_configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/KDecorationsConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/KDecorationsConfig.cmake"
  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/KDecorationsConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/KDecorationsConfigVersion.cmake"
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  COMPONENT Devel
)

install(EXPORT kdecorationsTargets
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        FILE KDecorationsTargets.cmake
        COMPONENT Devel)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kdecorations_version.h
        DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel )

########### install files ###############

install( FILES
    kdecoration.h
    kdecorationfactory.h
    kcommondecoration.h
    ${CMAKE_CURRENT_BINARY_DIR}/kdecorations_export.h
    DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel )
