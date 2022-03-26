#######################################
# Effect

set(diminactive_SOURCES
    diminactive.cpp
    main.cpp
)

kconfig_add_kcfg_files(diminactive_SOURCES
    diminactiveconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_diminactive ${diminactive_SOURCES})
target_link_libraries(kwin4_effect_diminactive PRIVATE
    kwineffects

    KF5::ConfigGui
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_diminactive_config_SRCS diminactive_config.cpp)
    ki18n_wrap_ui(kwin_diminactive_config_SRCS diminactive_config.ui)
    kconfig_add_kcfg_files(kwin_diminactive_config_SRCS diminactiveconfig.kcfgc)

    kwin_add_effect_config(kwin_diminactive_config ${kwin_diminactive_config_SRCS})

    target_link_libraries(kwin_diminactive_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::I18n
        Qt::DBus
        KWinEffectsInterface
    )
endif()
