# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwindecoration\")

add_subdirectory(declarative-plugin)

set(kcm_kwindecoration_PART_SRCS
    kcm.cpp
    decorationmodel.cpp
    declarative-plugin/buttonsmodel.cpp
)

ki18n_wrap_ui(kcm_kwindecoration_PART_SRCS
    kcm.ui
)

add_library(kcm_kwindecoration MODULE ${kcm_kwindecoration_PART_SRCS})
target_link_libraries(kcm_kwindecoration
    KDecoration2::KDecoration
    Qt5::DBus
    Qt5::Quick
    Qt5::QuickWidgets
    Qt5::UiTools
    KF5::Completion
    KF5::ConfigWidgets
    KF5::Declarative
    KF5::I18n
    KF5::NewStuff
    KF5::WindowSystem
    KF5::Service
)
install(TARGETS kcm_kwindecoration  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############

install( FILES kwindecoration.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES
    qml/main.qml
    qml/Buttons.qml
    qml/ButtonGroup.qml
    qml/Previews.qml
    DESTINATION ${DATA_INSTALL_DIR}/kwin/kcm_kwindecoration)
