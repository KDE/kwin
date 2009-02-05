#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    snow/snow.cpp
    )

# .desktop files
install( FILES
    snow/snow.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# Data files
install( FILES
    snow/data/snow.frag
    snow/data/snow.vert
    snow/data/snowflake.png
    DESTINATION ${DATA_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    snow/snow_config.cpp
    snow/snow_config.ui
    )

# .desktop files
install( FILES
    snow/snow_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
