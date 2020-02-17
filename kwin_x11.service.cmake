[Unit]
Description=KDE Window Manager
Wants=kcminit.service

[Service]
ExecStart=@CMAKE_INSTALL_FULL_BINDIR@/kwin_x11 --replace
BusName=org.kde.KWin
KillMode=none

[Install]
Alias=window-manager.service
