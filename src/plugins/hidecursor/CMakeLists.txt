kwin_add_builtin_effect(hidecursor)

target_sources(hidecursor PRIVATE
    hidecursor.cpp
    main.cpp
)

kconfig_add_kcfg_files(hidecursor
    hidecursorconfig.kcfgc
)

target_link_libraries(hidecursor PRIVATE
    kwin

    KF6::ConfigGui
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_hidecursor_config_SRCS hidecursor_config.cpp)
    ki18n_wrap_ui(kwin_hidecursor_config_SRCS hidecursor_config.ui)
    kconfig_add_kcfg_files(kwin_hidecursor_config_SRCS hidecursorconfig.kcfgc)

    kwin_add_effect_config(kwin_hidecursor_config ${kwin_hidecursor_config_SRCS})

    target_link_libraries(kwin_hidecursor_config
        KF6::KCMUtils
        KF6::CoreAddons
        KF6::I18n
        KF6::XmlGui
        KWinEffectsInterface
    )
endif()
