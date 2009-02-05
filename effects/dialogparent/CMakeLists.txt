#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    dialogparent/dialogparent.cpp
    )

# .desktop files
install( FILES
    dialogparent/dialogparent.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
