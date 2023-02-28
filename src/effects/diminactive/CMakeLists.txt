#######################################
# Effect

set(diminactive_SOURCES
    diminactive.cpp
    main.cpp
)

kconfig_add_kcfg_files(diminactive_SOURCES
    diminactiveconfig.kcfgc
)

kwin4_add_effect_module(diminactive ${diminactive_SOURCES})
target_link_libraries(diminactive PRIVATE
    kwineffects

    KF6::ConfigGui
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_diminactive_config_SRCS diminactive_config.cpp)
    ki18n_wrap_ui(kwin_diminactive_config_SRCS diminactive_config.ui)
    kconfig_add_kcfg_files(kwin_diminactive_config_SRCS diminactiveconfig.kcfgc)

    kwin_add_effect_config(kwin_diminactive_config ${kwin_diminactive_config_SRCS})

    target_link_libraries(kwin_diminactive_config
        KF6::ConfigWidgets
        KF6::CoreAddons
        KF6::I18n
        Qt::DBus
        KWinEffectsInterface
    )
endif()
