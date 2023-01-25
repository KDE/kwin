#######################################
# Effect

set(showpaint_SOURCES
    main.cpp main.h
    showpaint.cpp showpaint.h
)

kwin4_add_effect_module(kwin4_effect_showpaint ${showpaint_SOURCES})
target_link_libraries(kwin4_effect_showpaint PRIVATE
    kwineffects
    kwinglutils

    KF5::GlobalAccel
    KF5::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_showpaint_config_SRCS showpaint_config.cpp showpaint_config.h)
    ki18n_wrap_ui(kwin_showpaint_config_SRCS showpaint_config.ui)

    kwin_add_effect_config(kwin_showpaint_config ${kwin_showpaint_config_SRCS})

    target_link_libraries(kwin_showpaint_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::GlobalAccel
        KF5::I18n
        KF5::XmlGui
    )
endif()
