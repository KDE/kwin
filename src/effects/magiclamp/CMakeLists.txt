#######################################
# Effect

set(magiclamp_SOURCES
    magiclamp.cpp
    main.cpp
)

kconfig_add_kcfg_files(magiclamp_SOURCES
    magiclampconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_magiclamp ${magiclamp_SOURCES})
target_link_libraries(kwin4_effect_magiclamp PRIVATE
    kwineffects

    KF5::ConfigGui
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_magiclamp_config_SRCS magiclamp_config.cpp)
    ki18n_wrap_ui(kwin_magiclamp_config_SRCS magiclamp_config.ui)
    kconfig_add_kcfg_files(kwin_magiclamp_config_SRCS magiclampconfig.kcfgc)

    kwin_add_effect_config(kwin_magiclamp_config ${kwin_magiclamp_config_SRCS})

    target_link_libraries(kwin_magiclamp_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::I18n
        Qt::DBus
        KWinEffectsInterface
    )
endif()
