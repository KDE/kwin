systemd-run --user --wait --collect -P -p Restart=on-failure -p FileDescriptorStoreMax=20 kwin_wayland $@
