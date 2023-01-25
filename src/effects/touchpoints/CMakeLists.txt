#######################################
# Effect

set(touchpoints_SOURCES
    main.cpp main.h
    touchpoints.cpp touchpoints.h
)

kwin4_add_effect_module(kwin4_effect_touchpoints ${touchpoints_SOURCES})
target_link_libraries(kwin4_effect_touchpoints PRIVATE
    kwineffects
    kwinglutils

    KF5::GlobalAccel
)
