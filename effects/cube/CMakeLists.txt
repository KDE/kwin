#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    cube/cube.cpp
    cube/cube_proxy.cpp
    cube/cubeslide.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources
    cube/cubeslideconfig.kcfgc
    cube/cubeconfig.kcfgc
    )

# .desktop files
install( FILES
    cube/cube.desktop
    cube/cubeslide.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# Data files
install( FILES
    cube/data/cube-cap.glsl
    cube/data/cube-reflection.glsl
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
    )

kde4_add_kcfg_files(kwin4_effect_builtins_config_sources
    cube/cubeslideconfig.kcfgc
    cube/cubeconfig.kcfgc
    )

# .desktop files
install( FILES
    cube/cube_config.desktop
    cube/cubeslide_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
