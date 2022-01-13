#######################################
# Effect

set(invert_SOURCES
    invert.cpp
    invert.qrc
    main.cpp
)

kwin4_add_effect_module(kwin4_effect_invert ${invert_SOURCES})

#######################################
# Config
set(kwin_invert_config_SRCS invert_config.cpp)

kwin_add_effect_config(kwin_invert_config ${kwin_invert_config_SRCS})

target_link_libraries(kwin_invert_config
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    KWinEffectsInterface
)
