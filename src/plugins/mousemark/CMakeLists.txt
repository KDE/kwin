#######################################
# Effect

set(mousemark_SOURCES
    main.cpp
    mousemark.cpp
)

kconfig_add_kcfg_files(mousemark_SOURCES
    mousemarkconfig.kcfgc
)

kwin_add_builtin_effect(mousemark ${mousemark_SOURCES})
target_link_libraries(mousemark PRIVATE
    kwin

    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n
)

ecm_qt_declare_logging_category(mousemark
        HEADER mousemarklogging.h
        IDENTIFIER KWIN_MOUSEMARK
        CATEGORY_NAME kwin_effect_mousemark
        DEFAULT_SEVERITY Warning
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_mousemark_config_SRCS mousemark_config.cpp)
    ki18n_wrap_ui(kwin_mousemark_config_SRCS mousemark_config.ui)
    kconfig_add_kcfg_files(kwin_mousemark_config_SRCS mousemarkconfig.kcfgc)

    kwin_add_effect_config(kwin_mousemark_config ${kwin_mousemark_config_SRCS})

    target_link_libraries(kwin_mousemark_config
        KF6::KCMUtils
        KF6::CoreAddons
        KF6::GlobalAccel
        KF6::I18n
        KF6::XmlGui
        KWinEffectsInterface
    )
endif()
