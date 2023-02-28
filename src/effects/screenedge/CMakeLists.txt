#######################################
# Effect

# Source files
set(screenedge_SOURCES
    main.cpp
    screenedgeeffect.cpp
)

kwin4_add_effect_module(screenedge ${screenedge_SOURCES})
target_link_libraries(screenedge PRIVATE
    kwineffects
    kwinglutils

    KF6::Plasma
)
