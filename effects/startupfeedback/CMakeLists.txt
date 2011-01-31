#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    startupfeedback/startupfeedback.cpp
    )

# .desktop files
install( FILES
    startupfeedback/startupfeedback.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# Data files
install( FILES
    startupfeedback/data/blinking-startup-fragment.glsl
    DESTINATION ${DATA_INSTALL_DIR}/kwin )

#######################################
# Config
