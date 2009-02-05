#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    dimscreen/dimscreen.cpp
    )

# .desktop files
install( FILES
    dimscreen/dimscreen.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
