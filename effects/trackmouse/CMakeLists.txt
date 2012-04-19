#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    trackmouse/trackmouse.cpp
    )

# .desktop files
install( FILES
    trackmouse/trackmouse.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# Data files
install( FILES
    trackmouse/data/tm_inner.png
    trackmouse/data/tm_outer.png
    DESTINATION ${DATA_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    trackmouse/trackmouse_config.cpp
    trackmouse/trackmouse_config.ui
    )

# .desktop files
install( FILES
    trackmouse/trackmouse_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
