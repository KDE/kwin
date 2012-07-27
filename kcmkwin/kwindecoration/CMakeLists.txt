# need a header file from Aurorae sources
include_directories(
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/clients/aurorae/src/lib
)

set(AURORAE_SOURCE_DIR ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/clients/aurorae/src/lib/)

set(kcm_kwindecoration_PART_SRCS
    kwindecoration.cpp
    buttons.cpp
    buttonsconfigdialog.cpp
    configdialog.cpp
    preview.cpp
    decorationmodel.cpp
    ${AURORAE_SOURCE_DIR}/auroraetheme.cpp
    ${AURORAE_SOURCE_DIR}/themeconfig.cpp
)

kde4_add_ui_files(kcm_kwindecoration_PART_SRCS
    auroraeconfig.ui
    buttons.ui
    config.ui
    decoration.ui
)

kde4_add_plugin(kcm_kwindecoration ${kcm_kwindecoration_PART_SRCS})
target_link_libraries(kcm_kwindecoration ${KDE4_KNEWSTUFF3_LIBS} kdecorations ${QT_QTDECLARATIVE_LIBRARY} ${X11_LIBRARIES})
install(TARGETS kcm_kwindecoration  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############

install( FILES kwindecoration.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES
    qml/main.qml
    qml/AuroraeDecoration.qml
    qml/AuroraePreview.qml
    qml/DecorationPreview.qml
    DESTINATION ${DATA_INSTALL_DIR}/kwin/kcm_kwindecoration)
