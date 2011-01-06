#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    invert/invert.cpp
    )

# .desktop files
install( FILES
    invert/invert.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# Data files
install( FILES
    invert/data/invert.frag
    DESTINATION ${DATA_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    invert/invert_config.cpp
    )

# .desktop files
install( FILES
    invert/invert_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
