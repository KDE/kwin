#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    scalein/scalein.cpp
    )

# .desktop files
install( FILES
    scalein/scalein.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
