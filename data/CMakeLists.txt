add_subdirectory(icons)

########### next target ###############
add_executable( kwin5_update_default_rules update_default_rules.cpp)

target_link_libraries( kwin5_update_default_rules Qt5::Core Qt5::DBus KF5::ConfigCore )

install(TARGETS kwin5_update_default_rules DESTINATION ${LIB_INSTALL_DIR}/kconf_update_bin/  )

########### install files ###############

install(FILES org_kde_kwin.categories DESTINATION  ${KDE_INSTALL_LOGGINGCATEGORIESDIR})

