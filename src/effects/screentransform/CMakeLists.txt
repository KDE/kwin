#######################################
# Effect

set(screentransform_SOURCES
    main.cpp
    screentransform.cpp
    screentransform.qrc
)

kwin4_add_effect_module(screentransform ${screentransform_SOURCES})
target_link_libraries(screentransform PRIVATE
    kwineffects
    kwinglutils
)
