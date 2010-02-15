#!/usr/bin/perl

use warnings;
use strict;

use utf8;
use open qw(:std :utf8);

use POSIX;

unless (@ARGV) {
    print "Usage $0 object1 object2\n";
}


for (@ARGV) {
    print "Test $_ R_OK...";
    if (POSIX::access($_, &POSIX::R_OK)) {
        print " ok\n";
    } else {
        print " fail\n";
    }

    print "Test $_ W_OK...";
    if (POSIX::access($_, &POSIX::W_OK)) {
        print " ok\n";
    } else {
        print " fail\n";
    }

    print "Test $_ X_OK...";
    if (POSIX::access($_, &POSIX::X_OK)) {
        print " ok\n";
    } else {
        print " fail\n";
    }

    print "\n";
}
