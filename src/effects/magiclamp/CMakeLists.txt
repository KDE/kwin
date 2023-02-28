#######################################
# Effect

set(magiclamp_SOURCES
    magiclamp.cpp
    main.cpp
)

kconfig_add_kcfg_files(magiclamp_SOURCES
    magiclampconfig.kcfgc
)

kwin4_add_effect_module(magiclamp ${magiclamp_SOURCES})
target_link_libraries(magiclamp PRIVATE
    kwineffects

    KF6::ConfigGui
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_magiclamp_config_SRCS magiclamp_config.cpp)
    ki18n_wrap_ui(kwin_magiclamp_config_SRCS magiclamp_config.ui)
    kconfig_add_kcfg_files(kwin_magiclamp_config_SRCS magiclampconfig.kcfgc)

    kwin_add_effect_config(kwin_magiclamp_config ${kwin_magiclamp_config_SRCS})

    target_link_libraries(kwin_magiclamp_config
        KF6::ConfigWidgets
        KF6::CoreAddons
        KF6::I18n
        Qt::DBus
        KWinEffectsInterface
    )
endif()
