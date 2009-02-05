#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    sharpen/sharpen.cpp
    )

# .desktop files
install( FILES
    sharpen/sharpen.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# Data files
install( FILES
    sharpen/data/sharpen.frag
    sharpen/data/sharpen.vert
    DESTINATION ${DATA_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    sharpen/sharpen_config.cpp
    )

# .desktop files
install( FILES
    sharpen/sharpen_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
