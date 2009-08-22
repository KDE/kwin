# $Id: CMakeLists.txt,v 1.30 2009/07/05 18:27:08 hpereira Exp $

find_package (KDE4 REQUIRED)
add_definitions (-DQT3_SUPPORT -DQT3_SUPPORT_WARNINGS)

set(CPACK_PACKAGE_NAME "kde4-windeco-nitrogen" )
set(CPACK_SOURCE_PACKAGE_NAME "kde4-windeco-nitrogen" )
set(CPACK_SOURCE_GENERATOR TGZ)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Kde4 Nitrogen window decoration")
set(CPACK_PACKAGE_VENDOR "hugo.pereira@free.fr")
set(CPACK_PACKAGE_VERSION_MAJOR "2")
set(CPACK_PACKAGE_VERSION_MINOR "3")
set(CPACK_PACKAGE_VERSION_PATCH "5")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "CMake ${CMake_VERSION_MAJOR}.${CMake_VERSION_MINOR}")

include (CPack)
include (KDE4Defaults)
include (MacroLibrary)
add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})

include_directories (${KDE4_INCLUDES} ${CMAKE_BINARY_DIR})

########### add version number into compilation defines
add_definitions ( -DAPP_VERSION=\\\"${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}\\\")
add_subdirectory( config ) 

########### next target ###############
set(kwin_nitrogen_SRCS
     lib/helper.cpp
     lib/tileset.cpp
     nitrogen.cpp
     nitrogenbutton.cpp
     nitrogenclient.cpp
     nitrogenconfiguration.cpp
     nitrogenexception.cpp
     nitrogenexceptionlist.cpp
     nitrogensizegrip.cpp 
     x11util.cpp
  )

kde4_add_plugin(kwin3_nitrogen ${kwin_nitrogen_SRCS})
target_link_libraries( 
    kwin3_nitrogen 
    ${KDE4_KDEUI_LIBS} 
    ${X11_X11_LIB} 
    ${X11_Xrender_LIB} 
    kdecorations
  )

install(TARGETS kwin3_nitrogen  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### next target ###############
set( conversion_SRCS
    nitrogenconfiguration.cpp
    nitrogenexception.cpp
    nitrogenexceptionlist.cpp
    nitrogenconvertexceptions.cpp 
)
  
kde4_add_executable(nitrogen-convert-exceptions ${conversion_SRCS})

target_link_libraries( 
    nitrogen-convert-exceptions 
    ${KDE4_KDEUI_LIBS} 
  )

install(TARGETS nitrogen-convert-exceptions  DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS} )

########### install files ###############

install( FILES nitrogenclient.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )

