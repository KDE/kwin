#######################################
# Effect

set(showpaint_SOURCES
    main.cpp
    showpaint.cpp
)

kwin_add_builtin_effect(showpaint ${showpaint_SOURCES})
target_link_libraries(showpaint PRIVATE
    kwin
)
