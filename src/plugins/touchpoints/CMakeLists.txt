#######################################
# Effect

set(touchpoints_SOURCES
    main.cpp
    touchpoints.cpp
)

kwin_add_builtin_effect(touchpoints ${touchpoints_SOURCES})
target_link_libraries(touchpoints PRIVATE
    kwin

    KF6::GlobalAccel
)
