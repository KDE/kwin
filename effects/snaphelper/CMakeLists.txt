#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    snaphelper/snaphelper.cpp
    )

# .desktop files
install( FILES
    snaphelper/snaphelper.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
