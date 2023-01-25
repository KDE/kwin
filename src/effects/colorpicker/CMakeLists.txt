#######################################
# Effect

set(colorpicker_SOURCES
    colorpicker.cpp colorpicker.h
    main.cpp main.h
)

kwin4_add_effect_module(kwin4_effect_colorpicker ${colorpicker_SOURCES})
target_link_libraries(kwin4_effect_colorpicker PRIVATE
    kwineffects

    KF5::I18n

    Qt::DBus
)
