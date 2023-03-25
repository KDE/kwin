#######################################
# Effect

# Source files
set(slideback_SOURCES
    main.cpp
    slideback.cpp
)

kwin_add_builtin_effect(slideback ${slideback_SOURCES})
target_link_libraries(slideback PRIVATE
    kwineffects
)
