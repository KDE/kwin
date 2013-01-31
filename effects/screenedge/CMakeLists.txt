#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    screenedge/screenedgeeffect.cpp
    )

# .desktop files
install( FILES
    screenedge/screenedgeeffect.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
