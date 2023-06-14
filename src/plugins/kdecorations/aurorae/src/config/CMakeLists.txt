kcoreaddons_add_plugin(kcm_auroraedecoration SOURCES auroraeconfig.cpp INSTALL_NAMESPACE "${KDECORATION_KCM_PLUGIN_DIR}")
target_link_libraries(kcm_auroraedecoration
    KF6::ConfigGui
    KF6::KCMUtils
    KF6::I18n
    KDecoration2::KDecoration
    Qt::DBus
    Qt::UiTools
    Qt::Widgets
    KWinEffectsInterface
)
