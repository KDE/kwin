#! /usr/bin/perl

# Convert strings as in `Placement::policyToString()` from `placement.cpp`
# to correponding values of enum `Placement::Policy` defined in `placement.h`

use strict;

while (<>)
{
    chomp;
    s/placement=NoPlacement/placement=0/;
    s/placement=Default/placement=1/;
    s/placement=Random/placement=3/;
    s/placement=Smart/placement=4/;
    s/placement=Cascade/placement=5/;
    s/placement=Centered/placement=6/;
    s/placement=ZeroCornered/placement=7/;
    s/placement=UnderMouse/placement=8/;
    s/placement=OnMainWindow/placement=9/;
    s/placement=Maximizing/placement=10/;
    print "$_\n";
}
