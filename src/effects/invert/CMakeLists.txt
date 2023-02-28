#######################################
# Effect

set(invert_SOURCES
    invert.cpp
    invert.qrc
    main.cpp
)

kwin4_add_effect_module(invert ${invert_SOURCES})
target_link_libraries(invert PRIVATE
    kwineffects
    kwinglutils

    KF6::GlobalAccel
    KF6::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_invert_config_SRCS invert_config.cpp)

    kwin_add_effect_config(kwin_invert_config ${kwin_invert_config_SRCS})

    target_link_libraries(kwin_invert_config
        KF6::ConfigWidgets
        KF6::CoreAddons
        KF6::GlobalAccel
        KF6::I18n
        KF6::XmlGui
        KWinEffectsInterface
    )
endif()
