########### next target ###############
set(kwin_update_default_rules_SRCS update_default_rules.cpp)


add_executable( kwin_update_default_rules ${kwin_update_default_rules_SRCS})

target_link_libraries( kwin_update_default_rules Qt5::Core Qt5::DBus KF5::ConfigCore )

install(TARGETS kwin_update_default_rules DESTINATION ${LIB_INSTALL_DIR}/kconf_update_bin/  )

########### install files ###############

install( FILES stripTitle.js  DESTINATION  ${DATA_INSTALL_DIR}/kwin )

