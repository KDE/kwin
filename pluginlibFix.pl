#!/usr/bin/perl
foreach (<>) {
    if(/^PluginLib=libkwin(.*)$/) {
        print "PluginLib=kwin_$1\n"; 
        next;
    }
    print $_;
}
