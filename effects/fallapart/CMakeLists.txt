#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    fallapart/fallapart.cpp
    )

# .desktop files
install( FILES
    fallapart/fallapart.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
