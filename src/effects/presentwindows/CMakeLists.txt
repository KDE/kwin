#######################################
# Effect
install(FILES main.qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/presentwindows/)

set(presentwindows_SOURCES
    main.cpp
    presentwindows.cpp
    presentwindows_proxy.cpp
)

kconfig_add_kcfg_files(presentwindows_SOURCES
    presentwindowsconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_presentwindows ${presentwindows_SOURCES})

#######################################
# Config
set(kwin_presentwindows_config_SRCS presentwindows_config.cpp)
ki18n_wrap_ui(kwin_presentwindows_config_SRCS presentwindows_config.ui)
kconfig_add_kcfg_files(kwin_presentwindows_config_SRCS presentwindowsconfig.kcfgc)

kwin_add_effect_config(kwin_presentwindows_config ${kwin_presentwindows_config_SRCS})

target_link_libraries(kwin_presentwindows_config
    KF5::Completion
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    KWinEffectsInterface
)
