# $Id: CMakeLists.txt,v 1.8 2009/06/21 19:57:19 hpereira Exp $

include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/lib  )

find_package (KDE4 REQUIRED)
add_definitions (-DQT3_SUPPORT -DQT3_SUPPORT_WARNINGS)

include (KDE4Defaults)
include (MacroLibrary)
add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories (${KDE4_INCLUDES} ${CMAKE_BINARY_DIR})

########### next target ###############

set(kwin_nitrogen_config_PART_SRCS 
    config.cpp 
    itemmodel.cpp
    ../nitrogenconfiguration.cpp
    ../nitrogenexception.cpp
    ../nitrogenexceptionlist.cpp
    nitrogenconfigurationui.cpp 
    nitrogenexceptiondialog.cpp
    nitrogenexceptionlistdialog.cpp
    nitrogenexceptionmodel.cpp
  )

kde4_add_plugin(kwin_nitrogen_config ${kwin_nitrogen_config_PART_SRCS})

target_link_libraries(
    kwin_nitrogen_config  
    ${KDE4_KDEUI_LIBS}
    ${QT_QTGUI_LIBRARY}
  )

install(TARGETS kwin_nitrogen_config  DESTINATION ${PLUGIN_INSTALL_DIR} )

