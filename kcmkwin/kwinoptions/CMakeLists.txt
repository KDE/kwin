########### next target ###############
# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcmkwm\")

set(kcm_kwinoptions_PART_SRCS
    ${KWin_SOURCE_DIR}/effects/effect_builtins.cpp
    main.cpp
    mouse.cpp
    windows.cpp
)

ki18n_wrap_ui(kcm_kwinoptions_PART_SRCS
    actions.ui
    advanced.ui
    focus.ui
    mouse.ui
    moving.ui
)

kconfig_add_kcfg_files(kcm_kwinoptions_PART_SRCS kwinoptions_settings.kcfgc GENERATE_MOC)
kconfig_add_kcfg_files(kcm_kwinoptions_PART_SRCS kwinoptions_kdeglobals_settings.kcfgc GENERATE_MOC)

qt5_add_dbus_interface(kcm_kwinoptions_PART_SRCS ${KWin_SOURCE_DIR}/org.kde.kwin.Effects.xml kwin_effects_interface)
add_library(kcm_kwinoptions MODULE ${kcm_kwinoptions_PART_SRCS})
target_link_libraries(kcm_kwinoptions kwin Qt5::DBus KF5::Completion KF5::I18n KF5::ConfigWidgets KF5::Service KF5::WindowSystem)
install(TARGETS kcm_kwinoptions DESTINATION ${PLUGIN_INSTALL_DIR})

########### install files ###############

install(
    FILES
        kwinactions.desktop
        kwinadvanced.desktop
        kwinfocus.desktop
        kwinmoving.desktop
        kwinoptions.desktop
    DESTINATION
        ${SERVICES_INSTALL_DIR}
)
