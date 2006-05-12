


########### next target ###############
set(kwin_update_window_settings_SRCS update_window_settings.cpp)

kde4_automoc(${kwin_update_window_settings_SRCS})

kde4_add_executable( kwin_update_window_settings ${kwin_update_window_settings_SRCS})

target_link_libraries( kwin_update_window_settings ${KDE4_KDECORE_LIBS} )

install_targets(/lib/kconf_update_bin/ kwin_update_window_settings )



########### next target ###############
set(kwin_update_default_rules_SRCS update_default_rules.cpp)

kde4_automoc(${kwin_update_default_rules})

kde4_add_executable( kwin_update_default_rules ${kwin_update_default_rules_SRCS})

target_link_libraries( kwin_update_default_rules ${KDE4_KDECORE_LIBS} )

install_targets(/lib/kconf_update_bin/ kwin_update_default_rules )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin/default_rules FILES fsp_workarounds_1 )
install_files( ${SOUND_INSTALL_DIR} FILES pop.wav )
install_files( ${KCONF_UPDATE_INSTALL_DIR} FILES kwin.upd kwinsticky.upd kwiniconify.upd kwin3_plugin.upd kwin_focus1.upd          kwinupdatewindowsettings.upd kwin_focus2.upd kwin_fsp_workarounds_1.upd )
install_files( ${KCONF_UPDATE_INSTALL_DIR} FILES pluginlibFix.pl kwin3_plugin.pl kwin_focus1.sh kwin_focus2.sh )




#original Makefile.am contents follow:

#kconf_PROGRAMS = kwin_update_window_settings kwin_update_default_rules
#kconfdir = $(libdir)/kconf_update_bin
#
#kwin_update_window_settings_SOURCES = update_window_settings.cpp
#kwin_update_window_settings_LDADD   = $(LIB_KDECORE) $(KDE_RPATH)
#kwin_update_window_settings_LDFLAGS = $(all_libraries)
#
#kwin_update_default_rules_SOURCES = update_default_rules.cpp
#kwin_update_default_rules_LDADD   = $(LIB_KDECORE) $(KDE_RPATH)
#kwin_update_default_rules_LDFLAGS = $(all_libraries)
#
#INCLUDES = $(all_includes)
#
#METASOURCES = AUTO
#
#sounddir= $(kde_sounddir)
#sound_DATA = pop.wav
#
#update_DATA = kwin.upd kwinsticky.upd kwiniconify.upd kwin3_plugin.upd kwin_focus1.upd \
#        kwinupdatewindowsettings.upd kwin_focus2.upd kwin_fsp_workarounds_1.upd
#update_SCRIPTS = pluginlibFix.pl kwin3_plugin.pl kwin_focus1.sh kwin_focus2.sh
#updatedir = $(kde_datadir)/kconf_update
#
#kwin_default_rules_DATA = fsp_workarounds_1
#kwin_default_rulesdir = $(kde_datadir)/kwin/default_rules
