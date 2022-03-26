#######################################
# Effect
# Data files
install(FILES data/tm_inner.png data/tm_outer.png DESTINATION ${KDE_INSTALL_DATADIR}/kwin)

set(trackmouse_SOURCES
    main.cpp
    trackmouse.cpp
)

kconfig_add_kcfg_files(trackmouse_SOURCES
    trackmouseconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_trackmouse ${trackmouse_SOURCES})
target_link_libraries(kwin4_effect_trackmouse PRIVATE
    kwineffects
    kwinglutils

    KF5::ConfigGui
    KF5::GlobalAccel
    KF5::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_trackmouse_config_SRCS trackmouse_config.cpp)
    ki18n_wrap_ui(kwin_trackmouse_config_SRCS trackmouse_config.ui)
    kconfig_add_kcfg_files(kwin_trackmouse_config_SRCS trackmouseconfig.kcfgc)

    kwin_add_effect_config(kwin_trackmouse_config ${kwin_trackmouse_config_SRCS})

    target_link_libraries(kwin_trackmouse_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::GlobalAccel
        KF5::I18n
        KF5::XmlGui
        KWinEffectsInterface
    )
endif()
