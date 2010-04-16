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
    decorationdelegate.cpp
    decorationmodel.cpp
    ${AURORAE_SOURCE_DIR}/auroraebutton.cpp
    ${AURORAE_SOURCE_DIR}/auroraescene.cpp
    ${AURORAE_SOURCE_DIR}/auroraetab.cpp
    ${AURORAE_SOURCE_DIR}/auroraetheme.cpp
    ${AURORAE_SOURCE_DIR}/themeconfig.cpp
)

kde4_add_ui_files(kcm_kwindecoration_PART_SRCS buttons.ui config.ui decoration.ui)

kde4_add_plugin(kcm_kwindecoration ${kcm_kwindecoration_PART_SRCS})
target_link_libraries(kcm_kwindecoration ${KDE4_PLASMA_LIBS} ${KDE4_KNEWSTUFF3_LIBS} kdecorations ${X11_LIBRARIES})
install(TARGETS kcm_kwindecoration  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############

install( FILES kwindecoration.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
