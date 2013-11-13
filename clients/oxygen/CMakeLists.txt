########### add version number into compilation defines
# FIXME: CPACK_PACKAGE_VERSION_* are empty
add_definitions (-DAPP_VERSION=\\\"${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}\\\")
add_definitions(-DHAVE_X11=${X11_FOUND})
add_subdirectory(config)
add_subdirectory(demo)
add_subdirectory(data)

include_directories(${KDEBASE_WORKSPACE_SOURCE_DIR}/libs/oxygen)

########### next target ###############
set(kwin_oxygen_SRCS
     oxygenbutton.cpp
     oxygenclient.cpp
     oxygenclientgroupitemdata.cpp
     oxygendecohelper.cpp
     oxygenexceptionlist.cpp
     oxygenfactory.cpp
     oxygensizegrip.cpp
     oxygentitleanimationdata.cpp
  )

kconfig_add_kcfg_files(kwin_oxygen_SRCS oxygenconfiguration.kcfgc )

add_library(kwin3_oxygen MODULE ${kwin_oxygen_SRCS})
target_link_libraries(kwin3_oxygen Qt5::Widgets )
target_link_libraries(kwin3_oxygen KF5::KI18n KF5::KWindowSystem KF5::KStyle)

find_library( KDECORATIONS_LIB NAMES kdecorations )
target_link_libraries(kwin3_oxygen ${KDECORATIONS_LIB})

target_link_libraries(kwin3_oxygen oxygenstyle)

if(X11_FOUND)
  target_link_libraries(kwin3_oxygen ${XCB_XCB_LIBRARIES})
endif()

install(TARGETS kwin3_oxygen  DESTINATION ${PLUGIN_INSTALL_DIR})

########### install files ###############
install(FILES oxygenclient.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/)
