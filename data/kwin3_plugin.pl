#!/usr/bin/perl
foreach (<>) {
    if(/^PluginLib=kwin_(.*)$/) {
        print "PluginLib=kwin3_$1\n"; 
        next;
    }
    print $_;
}
