########### next target ###############
include(ECMSetupVersion)

ecm_setup_version(${PROJECT_VERSION}
                  VARIABLE_PREFIX KWINEFFECTS
                  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kwineffects_version.h"
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KWinEffectsConfigVersion.cmake"
                  SOVERSION 11
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
        Qt5::Core
        Qt5::Gui
        XCB::XCB
        XCB::XFIXES
        XCB::RENDER
        KF5::WaylandServer
    )

set_target_properties(kwinxrenderutils PROPERTIES
    VERSION ${KWINEFFECTS_VERSION_STRING}
    SOVERSION ${KWINEFFECTS_SOVERSION}
)
set_target_properties(kwinxrenderutils PROPERTIES OUTPUT_NAME ${KWIN_NAME}xrenderutils)

install(TARGETS kwinxrenderutils EXPORT kdeworkspaceLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

###  effects lib  ###
set(kwin_EFFECTSLIB_SRCS
    kwineffects.cpp
    anidata.cpp
    kwinanimationeffect.cpp
    logging.cpp
    )

set(kwineffects_QT_LIBS
    Qt5::DBus
    Qt5::Widgets
)

set(kwineffects_KDE_LIBS
    KF5::ConfigCore
    KF5::CoreAddons
    KF5::WindowSystem
)

set(kwineffects_XCB_LIBS
    XCB::XCB
)

add_library(kwineffects SHARED ${kwin_EFFECTSLIB_SRCS})
generate_export_header(kwineffects EXPORT_FILE_NAME kwineffects_export.h)
target_link_libraries(kwineffects
PUBLIC
    ${kwineffects_QT_LIBS}
    ${kwineffects_KDE_LIBS}
    ${kwineffects_XCB_LIBS}
)
if( KWIN_HAVE_XRENDER_COMPOSITING )
    target_link_libraries(kwineffects PRIVATE kwinxrenderutils XCB::XFIXES)
endif()
set_target_properties(kwineffects PROPERTIES
    VERSION ${KWINEFFECTS_VERSION_STRING}
    SOVERSION ${KWINEFFECTS_SOVERSION}
)
set_target_properties(kwineffects PROPERTIES OUTPUT_NAME ${KWIN_NAME}effects)

install(TARGETS kwineffects EXPORT kdeworkspaceLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

# kwingl(es)utils library
set(kwin_GLUTILSLIB_SRCS
    kwinglutils.cpp
    kwingltexture.cpp
    kwinglutils_funcs.cpp
    kwinglplatform.cpp
    logging.cpp
    )

macro( KWIN4_ADD_GLUTILS_BACKEND name glinclude )
    include_directories(${glinclude})
    add_library(${name} SHARED ${kwin_GLUTILSLIB_SRCS})
    generate_export_header(${name} BASE_NAME kwinglutils EXPORT_FILE_NAME kwinglutils_export.h)
    target_link_libraries(${name} PUBLIC XCB::XCB KF5::CoreAddons KF5::ConfigCore KF5::WindowSystem)
    set_target_properties(${name} PROPERTIES
        VERSION ${KWINEFFECTS_VERSION_STRING}
        SOVERSION ${KWINEFFECTS_SOVERSION}
    )
    target_link_libraries(${name} PUBLIC ${ARGN})

    install(TARGETS ${name} EXPORT kdeworkspaceLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})
endmacro()

kwin4_add_glutils_backend(kwinglutils ${epoxy_INCLUDE_DIR} ${epoxy_LIBRARY})
set_target_properties(kwinglutils PROPERTIES OUTPUT_NAME ${KWIN_NAME}glutils)

target_link_libraries(kwinglutils PUBLIC ${epoxy_LIBRARY})

if (Vulkan_FOUND)
    set(kwin_VULKANUTILSLIB_SRCS
        kwinvulkanutils.cpp
        kwinvulkanutils_funcs.cpp
        logging.cpp
    )

    add_library(kwinvulkanutils SHARED ${kwin_VULKANUTILSLIB_SRCS})
    generate_export_header(kwinvulkanutils BASE_NAME kwinvulkanutils EXPORT_FILE_NAME kwinvulkanutils_export.h)
    set_target_properties(kwinvulkanutils PROPERTIES
                          OUTPUT_NAME ${KWIN_NAME}vulkanutils
                          VERSION ${KWINEFFECTS_VERSION_STRING}
                          SOVERSION ${KWINEFFECTS_SOVERSION}
                         )

    target_link_libraries(kwinvulkanutils PUBLIC Qt5::Core Qt5::Gui KF5::ConfigCore KF5::CoreAddons KF5::WindowSystem Vulkan::Vulkan)
    install(TARGETS kwinvulkanutils EXPORT kdeworkspaceLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})
endif()

install( FILES
    kwinglobals.h
    kwineffects.h
    kwinanimationeffect.h
    kwinglplatform.h
    kwinglutils.h
    kwinglutils_funcs.h
    kwingltexture.h
    kwinxrenderutils.h
    kwinvulkanutils.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwinconfig.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwineffects_export.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwinglutils_export.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwinvulkanutils_export.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwinxrenderutils_export.h
    DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel)
