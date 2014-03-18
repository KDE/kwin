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

qt5_wrap_ui(kcm_kwindecoration_PART_SRCS
    auroraeconfig.ui
    buttons.ui
    config.ui
    decoration.ui
)

add_library(kcm_kwindecoration MODULE ${kcm_kwindecoration_PART_SRCS})
target_link_libraries(kcm_kwindecoration
    kdecorations
    Qt5::DBus
    Qt5::Quick
    Qt5::UiTools
    KF5::Completion
    KF5::ConfigWidgets
    KF5::I18n
    KF5::NewStuff
)
install(TARGETS kcm_kwindecoration  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############

install( FILES kwindecoration.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES
    qml/main.qml
    qml/AuroraeDecoration.qml
    qml/AuroraePreview.qml
    qml/DecorationPreview.qml
    DESTINATION ${DATA_INSTALL_DIR}/kwin/kcm_kwindecoration)
