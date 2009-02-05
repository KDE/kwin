#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    translucency/translucency.cpp
    )

# .desktop files
install( FILES
    translucency/translucency.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    translucency/translucency_config.cpp
    translucency/translucency_config.ui
    )

# .desktop files
install( FILES
    translucency/translucency_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
