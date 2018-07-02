
########### next target ###############
# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwindesktop\")

include_directories(${KWIN_SOURCE_DIR}/effects)

set(kcm_kwindesktop_PART_SRCS main.cpp desktopnameswidget.cpp)
ki18n_wrap_ui(kcm_kwindesktop_PART_SRCS main.ui)
qt5_add_dbus_interface( kcm_kwindesktop_PART_SRCS
    ${KWIN_SOURCE_DIR}/org.kde.kwin.Effects.xml kwin_effects_interface)

add_library(kcm_kwindesktop MODULE ${kcm_kwindesktop_PART_SRCS})

target_link_libraries(kcm_kwindesktop
    Qt5::X11Extras
    KF5::KCMUtils
    KF5::Completion
    KF5::GlobalAccel
    KF5::I18n
    KF5::Package
    KF5::WindowSystem
    KF5::XmlGui
    ${X11_LIBRARIES}
    kwin4_effect_builtins
)

install(TARGETS kcm_kwindesktop  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############
install( FILES desktop.desktop DESTINATION  ${SERVICES_INSTALL_DIR} )

