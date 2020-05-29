install(FILES kwin.upd
        DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})
install(PROGRAMS kwin-5.16-auto-bordersize.sh
        DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})
install(PROGRAMS kwin-5.18-move-animspeed.py
        DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})

install(FILES kwinrules.upd
        DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})
install(PROGRAMS kwinrules-5.19-placement.pl
        DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})
