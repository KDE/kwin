#######################################
# Effect

# Source files
set(contrast_SOURCES
    contrast.cpp
    contrastshader.cpp
)
kwin4_add_effect_module(kwin4_effect_contrast ${contrast_SOURCES})
