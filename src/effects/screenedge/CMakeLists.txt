#######################################
# Effect

# Source files
set(screenedge_SOURCES
    main.cpp
    screenedgeeffect.cpp
)

kwin_add_builtin_effect(screenedge ${screenedge_SOURCES})
target_link_libraries(screenedge PRIVATE
    kwineffects
    kwinglutils

    KF6::Plasma
)
