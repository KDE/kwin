# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcmkwinrules\")
add_definitions(-DKCMRULES)
########### next target ###############

include_directories(../../)
set(kwinrules_MOC_HDRS yesnobox.h ../../cursor.h ../../plugins/platforms/x11/standalone/x11cursor.h)
qt5_wrap_cpp(kwinrules_MOC_SRCS ${kwinrules_MOC_HDRS})
set(kwinrules_SRCS ruleswidget.cpp ruleslist.cpp kwinsrc.cpp detectwidget.cpp ${kwinrules_MOC_SRCS})

ki18n_wrap_ui(kwinrules_SRCS ruleslist.ui detectwidget.ui editshortcut.ui ruleswidgetbase.ui)

set(kwin_rules_dialog_KDEINIT_SRCS main.cpp ${kwinrules_SRCS})

kf5_add_kdeinit_executable(kwin_rules_dialog ${kwin_rules_dialog_KDEINIT_SRCS})

set(kwin_kcm_rules_XCB_LIBS
    XCB::CURSOR
    XCB::XCB
    XCB::XFIXES
)

set(kcm_libs
    Qt5::Concurrent
    Qt5::X11Extras

    KF5::Completion
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
    KF5::WindowSystem
    KF5::XmlGui
)

if (KWIN_BUILD_ACTIVITIES)
    set(kcm_libs ${kcm_libs} KF5::Activities)
endif()

target_link_libraries(kdeinit_kwin_rules_dialog ${kcm_libs} ${kwin_kcm_rules_XCB_LIBS})

install(TARGETS kdeinit_kwin_rules_dialog ${INSTALL_TARGETS_DEFAULT_ARGS})
install(TARGETS kwin_rules_dialog DESTINATION ${LIBEXEC_INSTALL_DIR})

########### next target ###############

set(kcm_kwinrules_PART_SRCS kcm.cpp ${kwinrules_SRCS})

add_library(kcm_kwinrules MODULE ${kcm_kwinrules_PART_SRCS})

target_link_libraries(kcm_kwinrules ${kcm_libs} ${kwin_kcm_rules_XCB_LIBS})

install(TARGETS kcm_kwinrules DESTINATION ${PLUGIN_INSTALL_DIR})

########### install files ###############

install(FILES kwinrules.desktop DESTINATION ${SERVICES_INSTALL_DIR})
