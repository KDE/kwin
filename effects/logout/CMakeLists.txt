#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    logout/logout.cpp
    )

# .desktop files
install( FILES
    logout/logout.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
