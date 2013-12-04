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
    Qt5::Widgets
    Qt5::X11Extras
    KF5::KConfigCore
    KF5::KI18n
    KF5::KService # KLibrary
)
# target_link_libraries(kdecorations LINK_INTERFACE_LIBRARIES ${KDE4_KDEUI_LIBS})

set_target_properties(kdecorations PROPERTIES
   VERSION ${GENERIC_LIB_VERSION}
   SOVERSION ${GENERIC_LIB_SOVERSION}
   )
install(TARGETS kdecorations EXPORT kdeworkspaceLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS} )

########### install files ###############

install( FILES
    kdecoration.h
    kdecorationfactory.h
    kcommondecoration.h
    ${CMAKE_CURRENT_BINARY_DIR}/kdecorations_export.h
    DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel )
