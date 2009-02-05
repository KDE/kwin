#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    shadow/shadow.cpp
    )

# .desktop files
install( FILES
    shadow/shadow.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# Data files
install( FILES
    shadow/data/shadow-texture.png
    DESTINATION ${DATA_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    shadow/shadow_config.cpp
    shadow/shadow_config.ui
    )

# .desktop files
install( FILES
    shadow/shadow_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
