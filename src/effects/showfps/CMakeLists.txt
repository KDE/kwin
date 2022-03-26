#######################################
# Effect

set(showfps_SOURCES
    main.cpp
    showfps.cpp
)

kconfig_add_kcfg_files(showfps_SOURCES
    showfpsconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_showfps ${showfps_SOURCES})
target_link_libraries(kwin4_effect_showfps PRIVATE
    kwineffects
    kwinglutils

    KF5::ConfigGui
    KF5::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_showfps_config_SRCS showfps_config.cpp)
    ki18n_wrap_ui(kwin_showfps_config_SRCS showfps_config.ui)
    kconfig_add_kcfg_files(kwin_showfps_config_SRCS showfpsconfig.kcfgc)

    kwin_add_effect_config(kwin_showfps_config ${kwin_showfps_config_SRCS})

    target_link_libraries(kwin_showfps_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::I18n
        Qt::DBus
        KWinEffectsInterface
    )
endif()
