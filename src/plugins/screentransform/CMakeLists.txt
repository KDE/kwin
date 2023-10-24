#######################################
# Effect

set(screentransform_SOURCES
    main.cpp
    screentransform.cpp
    screentransform.qrc
)

kwin_add_builtin_effect(screentransform ${screentransform_SOURCES})
target_link_libraries(screentransform PRIVATE
    kwin
)
