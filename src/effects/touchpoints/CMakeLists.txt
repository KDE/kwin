#######################################
# Effect

set(touchpoints_SOURCES
    main.cpp
    touchpoints.cpp
)

kwin4_add_effect_module(touchpoints ${touchpoints_SOURCES})
target_link_libraries(touchpoints PRIVATE
    kwineffects
    kwinglutils

    KF6::GlobalAccel
)
