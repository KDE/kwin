#######################################
# Effect

kwin_add_builtin_effect(slideback)
target_sources(slideback PRIVATE
    main.cpp
    motionmanager.cpp
    slideback.cpp
)
target_link_libraries(slideback PRIVATE
    kwin
)
