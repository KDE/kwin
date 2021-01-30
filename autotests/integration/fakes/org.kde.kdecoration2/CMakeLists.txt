########################################################
# FakeDecoWithShadows
########################################################
add_library(fakedecoshadows MODULE fakedecoration_with_shadows.cpp)
set_target_properties(fakedecoshadows PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/fakes/org.kde.kdecoration2")
target_link_libraries(fakedecoshadows
    PUBLIC
        Qt::Core
        Qt::Gui
    PRIVATE
        KDecoration2::KDecoration
        KF5::CoreAddons)

