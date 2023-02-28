#######################################
# Effect

set(showpaint_SOURCES
    main.cpp
    showpaint.cpp
)

kwin4_add_effect_module(showpaint ${showpaint_SOURCES})
target_link_libraries(showpaint PRIVATE
    kwineffects
    kwinglutils

    KF6::GlobalAccel
    KF6::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_showpaint_config_SRCS showpaint_config.cpp)
    ki18n_wrap_ui(kwin_showpaint_config_SRCS showpaint_config.ui)

    kwin_add_effect_config(kwin_showpaint_config ${kwin_showpaint_config_SRCS})

    target_link_libraries(kwin_showpaint_config
        KF6::ConfigWidgets
        KF6::CoreAddons
        KF6::GlobalAccel
        KF6::I18n
        KF6::XmlGui
    )
endif()
