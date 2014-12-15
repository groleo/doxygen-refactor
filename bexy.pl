#!/usr/bin/perl
use strict;
use warnings;

my $it_decl;


while (<>) {              # Read input into default variable $_
   s/\@B(.*)\@E/$1/gi; # Subs 'four', 'for' or 'floor' for 4
   if ($1) {
        $it_decl=$1;
   }
   if (m/\@X(.*),(.*)\@Y/) {
        s/\@X(.*),(.*)\@Y/$2!=${it_decl}end() && $1=*$2/gi;
   }
   print $_;
}
