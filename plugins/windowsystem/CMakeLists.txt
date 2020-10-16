set(kwindowsystem_plugin_SRCS
    plugin.cpp
    windoweffects.cpp
    windowshadow.cpp
    windowsystem.cpp
)

add_library(KF5WindowSystemKWinPrivatePlugin MODULE ${kwindowsystem_plugin_SRCS})
set_target_properties(KF5WindowSystemKWinPrivatePlugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf5/kwindowsystem/")
target_link_libraries(KF5WindowSystemKWinPrivatePlugin kwin)

install(
    TARGETS
        KF5WindowSystemKWinPrivatePlugin
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kf5/kwindowsystem/
)
