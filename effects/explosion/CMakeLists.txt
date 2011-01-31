#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    explosion/explosion.cpp
    )

# .desktop files
install( FILES
    explosion/explosion.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# Data files
install( FILES
    explosion/data/explosion-end.png
    explosion/data/explosion-start.png
    explosion/data/explosion.frag
    DESTINATION ${DATA_INSTALL_DIR}/kwin )
