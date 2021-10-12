#######################################
# Effect

set(colorpicker_SOURCES
    colorpicker.cpp
    main.cpp
)

kwin4_add_effect_module(kwin4_effect_colorpicker ${colorpicker_SOURCES})
