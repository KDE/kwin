#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    slidingpopups/slidingpopups.cpp
    )

# .desktop files
install( FILES
    slidingpopups/slidingpopups.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
