kconf_PROGRAMS = kwin_update_window_settings kwin_update_default_rules
kconfdir = $(libdir)/kconf_update_bin

kwin_update_window_settings_SOURCES = update_window_settings.cpp
kwin_update_window_settings_LDADD   = $(LIB_KDECORE) $(KDE_RPATH)
kwin_update_window_settings_LDFLAGS = $(all_libraries)

kwin_update_default_rules_SOURCES = update_default_rules.cpp
kwin_update_default_rules_LDADD   = $(LIB_KDECORE) $(KDE_RPATH)
kwin_update_default_rules_LDFLAGS = $(all_libraries)

INCLUDES = $(all_includes)

METASOURCES = AUTO

sounddir= $(kde_sounddir)
sound_DATA = pop.wav

update_DATA = kwin.upd kwinsticky.upd kwiniconify.upd kwin3_plugin.upd kwin_focus1.upd \
        kwinupdatewindowsettings.upd kwin_focus2.upd kwin_fsp_workarounds_1.upd
update_SCRIPTS = pluginlibFix.pl kwin3_plugin.pl kwin_focus1.sh kwin_focus2.sh
updatedir = $(kde_datadir)/kconf_update

kwin_default_rules_DATA = fsp_workarounds_1
kwin_default_rulesdir = $(kde_datadir)/kwin/default_rules
