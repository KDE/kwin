SUBDIRS = plastik b2 default keramik laptop modernsystem quartz redmond web
# need to be ported: kwmtheme (almost done)
# only for testing: test

# -maxdepth 2 is needed as the config directories have other .pot files :-(
# KDE4 (or 3.4): unify the .pot, at least for the clients
messages:
	$(XGETTEXT) `find . -maxdepth 2  -name \*.cpp` -o $(podir)/kwin_clients.pot
