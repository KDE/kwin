[Unit]
Description=KDE Window Manager
Wants=kcminit.service

[Service]
ExecStart=@CMAKE_INSTALL_FULL_BINDIR@/kwin_wayland
BusName=org.kde.KWin
KillMode=none

[Install]
Alias=window-manager.service
