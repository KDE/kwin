#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
     dashboard/dashboard.cpp
     dashboard/dashboard.cpp )

# .desktop files
install( FILES
         dashboard/dashboard.desktop
         DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
     dashboard/dashboard_config.cpp
     dashboard/dashboard_config.ui )

install( FILES
         dashboard/dashboard_config.desktop
         DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
