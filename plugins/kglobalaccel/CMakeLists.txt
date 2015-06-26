set(kglobalaccel_plugin_SRCS
    kglobalaccel_plugin.cpp
)

add_library(KF5GlobalAccelPrivateKWin MODULE ${kglobalaccel_plugin_SRCS})
target_link_libraries(KF5GlobalAccelPrivateKWin
    KF5::GlobalAccelPrivate
    kwin
)

install(
    TARGETS
        KF5GlobalAccelPrivateKWin
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kglobalaccel5.platforms/
)
