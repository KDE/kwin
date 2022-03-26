#######################################
# Effect

set(magnifier_SOURCES
    magnifier.cpp
    main.cpp
)

kconfig_add_kcfg_files(magnifier_SOURCES
    magnifierconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_magnifier ${magnifier_SOURCES})
target_link_libraries(kwin4_effect_magnifier PRIVATE
    kwineffects
    kwinglutils

    KF5::ConfigGui
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_magnifier_config_SRCS magnifier_config.cpp)
    ki18n_wrap_ui(kwin_magnifier_config_SRCS magnifier_config.ui)
    kconfig_add_kcfg_files(kwin_magnifier_config_SRCS magnifierconfig.kcfgc)

    kwin_add_effect_config(kwin_magnifier_config ${kwin_magnifier_config_SRCS})

    target_link_libraries(kwin_magnifier_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::GlobalAccel
        KF5::I18n
        KF5::XmlGui
        KWinEffectsInterface
    )
endif()
