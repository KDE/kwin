#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    startupfeedback/startupfeedback.cpp
    )

# Data files
install( FILES
    startupfeedback/data/blinking-startup-fragment.glsl
    DESTINATION ${DATA_INSTALL_DIR}/kwin )

#######################################
# Config
