#######################################
# Effect

set(screentransform_SOURCES
    main.cpp
    screentransform.cpp
)

kwin4_add_effect_module(kwin4_effect_screentransform ${screentransform_SOURCES})
