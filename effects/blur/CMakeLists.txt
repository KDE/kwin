#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    blur/blur.cpp
    )

# .desktop files
install( FILES
    blur/blur.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# Data files
install( FILES
    blur/data/blur.frag
    blur/data/blur.vert
    blur/data/blur-render.frag
    blur/data/blur-render.vert
    DESTINATION  ${DATA_INSTALL_DIR}/kwin )
