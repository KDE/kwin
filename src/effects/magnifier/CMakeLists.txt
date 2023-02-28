#######################################
# Effect

set(magnifier_SOURCES
    magnifier.cpp
    main.cpp
)

kconfig_add_kcfg_files(magnifier_SOURCES
    magnifierconfig.kcfgc
)

kwin4_add_effect_module(magnifier ${magnifier_SOURCES})
target_link_libraries(magnifier PRIVATE
    kwineffects
    kwinglutils

    KF6::ConfigGui
    KF6::ConfigWidgets
    KF6::GlobalAccel
    KF6::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_magnifier_config_SRCS magnifier_config.cpp)
    ki18n_wrap_ui(kwin_magnifier_config_SRCS magnifier_config.ui)
    kconfig_add_kcfg_files(kwin_magnifier_config_SRCS magnifierconfig.kcfgc)

    kwin_add_effect_config(kwin_magnifier_config ${kwin_magnifier_config_SRCS})

    target_link_libraries(kwin_magnifier_config
        KF6::ConfigWidgets
        KF6::CoreAddons
        KF6::GlobalAccel
        KF6::I18n
        KF6::XmlGui
        KWinEffectsInterface
    )
endif()
