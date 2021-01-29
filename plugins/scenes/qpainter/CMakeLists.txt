set(SCENE_QPAINTER_SRCS scene_qpainter.cpp)

add_library(KWinSceneQPainter MODULE scene_qpainter.cpp)
set_target_properties(KWinSceneQPainter PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.kwin.scenes/")
target_link_libraries(KWinSceneQPainter
    kwin
    SceneQPainterBackend
)

install(
    TARGETS
        KWinSceneQPainter
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/org.kde.kwin.scenes/
)
