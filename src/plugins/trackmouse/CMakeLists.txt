#######################################
# Effect
# Data files
install(FILES data/tm_inner.png data/tm_outer.png DESTINATION ${KDE_INSTALL_DATADIR}/${KWIN_DATADIR})

set(trackmouse_SOURCES
    main.cpp
    trackmouse.cpp
)

kconfig_add_kcfg_files(trackmouse_SOURCES
    trackmouseconfig.kcfgc
)

kwin_add_builtin_effect(trackmouse ${trackmouse_SOURCES})
target_link_libraries(trackmouse PRIVATE
    kwin

    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_trackmouse_config_SRCS trackmouse_config.cpp)
    ki18n_wrap_ui(kwin_trackmouse_config_SRCS trackmouse_config.ui)
    kconfig_add_kcfg_files(kwin_trackmouse_config_SRCS trackmouseconfig.kcfgc)

    kwin_add_effect_config(kwin_trackmouse_config ${kwin_trackmouse_config_SRCS})

    target_link_libraries(kwin_trackmouse_config
        KF6::KCMUtils
        KF6::CoreAddons
        KF6::GlobalAccel
        KF6::I18n
        KF6::XmlGui
        KWinEffectsInterface
    )
endif()
