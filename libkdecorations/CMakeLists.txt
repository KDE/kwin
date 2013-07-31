# kdecorations library
set(kdecorations_LIB_SRCS
   kdecoration.cpp
   kdecoration_p.cpp
   kdecoration_plugins_p.cpp
   kdecorationfactory.cpp
   kcommondecoration.cpp
   kcommondecoration_p.cpp )

kde4_add_library(kdecorations SHARED ${kdecorations_LIB_SRCS})

target_link_libraries(kdecorations
    ${Qt5Widgets_LIBRARIES}
    ${Qt5X11Extras_LIBRARIES}
    ${KDE4Support_LIBRARIES} # kdebug
    ${KDE4_KDEUI_LIBRARY} # KGlobalSettings::windowTitleFont
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
    DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel )
