#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    cube/cube.cpp
    cube/cubeslide.cpp
    cube/cylinder.cpp
    cube/sphere.cpp
    )

# .desktop files
install( FILES
    cube/cube.desktop
    cube/cubeslide.desktop
    cube/cylinder.desktop
    cube/sphere.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# Data files
install( FILES
    cube/data/cubecap.png
    cube/data/cylinder.frag
    cube/data/cylinder.vert
    cube/data/sphere.vert
    DESTINATION ${DATA_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    cube/cube_config.cpp
    cube/cube_config.ui
    cube/cubeslide_config.cpp
    cube/cubeslide_config.ui
    cube/cylinder_config.cpp
    cube/cylinder_config.ui
    cube/sphere_config.cpp
    cube/sphere_config.ui
    )

# .desktop files
install( FILES
    cube/cube_config.desktop
    cube/cubeslide_config.desktop
    cube/cylinder_config.desktop
    cube/sphere_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
