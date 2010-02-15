#!/usr/bin/perl

use warnings;
use strict;

use utf8;
use open qw(:std :utf8);

use POSIX;

unless (@ARGV) {
    print "Usage $0 object1 object2\n";
}

print "Test $ARGV[0] R_OK...";
if (POSIX::access($ARGV[0], &POSIX::R_OK)) {
    print " ok\n";
} else {
    print " fail\n";
}

print "Test $ARGV[0] W_OK...";
if (POSIX::access($ARGV[0], &POSIX::W_OK)) {
    print " ok\n";
} else {
    print " fail\n";
}

print "Test $ARGV[0] X_OK...";
if (POSIX::access($ARGV[0], &POSIX::X_OK)) {
    print " ok\n";
} else {
    print " fail\n";
}
