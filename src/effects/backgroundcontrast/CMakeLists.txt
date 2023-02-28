#######################################
# Effect

# Source files
set(contrast_SOURCES
    contrast.cpp
    contrastshader.cpp
    main.cpp
)
kwin4_add_effect_module(contrast ${contrast_SOURCES})
target_link_libraries(contrast PRIVATE
    kwin
    kwineffects
    kwinglutils
)
