#!/usr/bin/perl
#
# Script for generating the color name to RGB component maps in a big
# json string. Run from the root pennmush directory, not utils.
#
# Gets xterm color data from utils/xterm256.json and other color
# schemes from Graphics::ColorNames
#
# Requires a fairly modern perl and the File::Slurper,
# Graphics::ColorNames and JSON::XS packages. Install off of CPAN or
# your package manager (libfile-slurper-perl,
# libgraphics-colornames-perl, libjson-xs-perl on Debian/Ubuntu/etc.,
# perl-file-slurper, perl-graphics-colornames, perl-json-xs on arch)
#

use strict;
use warnings;
use autodie;
use feature qw/say/;
use integer;
use Graphics::ColorNames 2.0, qw/all_schemes/;
use File::Slurper qw/read_binary/;
use JSON::XS;

# spork's color map
my @map_16 = qw/
  0: x    1: r    2: g    3: y    4: b    5: m    6: c    7: w    8: xh
  9: rh  10: gh  11: yh  12: bh  13: mh  14: ch  15: wh  16: x   17: b 
 18: b   19: b   20: hb  21: hb  22: g   23: c   24: b   25: hb  26: hb
 27: hb  28: hg  29: g   30: c   31: hb  32: hb  33: hb  34: hg  35: hg
 36: hc  37: hc  38: hb  39: hb  40: hg  41: hg  42: hg  43: hc  44: hc
 45: hc  46: hg  47: hg  48: hg  49: hc  50: hc  51: hc  52: r   53: m 
 54: m   55: bh  56: hb  57: hb  58: g   59: g   60: c   61: hb  62: hb
 63: hb  64: g   65: g   66: g   67: c   68: c   69: hb  70: hg  71: g 
 72: g   73: c   74: hc  75: hc  76: hg  77: hg  78: hg  79: hc  80: hc
 81: hc  82: hg  83: hg  84: hg  85: hg  86: hc  87: hc  88: r   89: r 
 90: m   91: m   92: hm  93: hm  94: g   95: r   96: m   97: m   98: hm
 99: hm 100: g  101: g  102: g  103: c  104: hb 105: hb 106: g  107: g 
108: g  109: c  110: hc 111: hc 112: hg 113: hg 114: hg 115: hg 116: hc
117: hc 118: hg 119: hg 120: hg 121: hg 122: hw 123: hw 124: r  125: m 
126: m  127: hm 128: hm 129: hm 130: hr 131: hr 132: hr 133: hm 134: hm
135: hm 136: y  137: y  138: hm 139: hm 140: hm 141: hm 142: y  143: y 
144: y  145: hm 146: hm 147: hm 148: hy 149: hy 150: hy 151: hw 152: hw
153: hw 154: hy 155: hy 156: hy 157: hy 158: hw 159: hw 160: r  161: hr
162: hm 163: hm 164: hm 165: hm 166: hr 167: hr 168: hr 169: hm 170: hm
171: hm 172: y  173: y  174: y  175: hm 176: hm 177: hm 178: y  179: y 
180: hy 181: hm 182: hm 183: hm 184: y  185: y  186: hy 187: hw 188: hw
189: hw 190: hy 191: hy 192: hy 193: hw 194: hw 195: hw 196: hr 197: hr
198: hr 199: hm 200: hm 201: hm 202: hr 203: hr 204: hr 205: hm 206: hm
207: hm 208: hr 209: hr 210: hm 211: hm 212: hm 213: hm 214: hy 215: hy
216: hw 217: hw 218: hw 219: hw 220: hy 221: hy 222: hy 223: hw 224: hw
225: hw 226: hy 227: hy 228: hy 229: hy 230: hw 231: hw 232: x  233: x 
234: xh 235: xh 236: xh 237: xh 238: xh 239: xh 240: xh 241: w  242: w 
243: w  244: w  245: w  246: w  247: w  248: hw 249: hw 250: hw 251: hw
252: hw 253: hw 254: hw 255: hw
    /;	

my %ansicodes = (
    x => 0,
    r => 1,
    g => 2,
    y => 3,
    b => 4,
    m => 5,
    c => 6,
    w => 7
    );

@map_16 = map {
    my $hilite = s/h//;
    my $color = $ansicodes{$_};
    $color |= 0x0100 if $hilite;
    $color
} grep(/^[xrgybmcwh]+$/, @map_16);


my $xterm256_raw = read_binary "utils/xterm256.json";
my $xterm256colors = decode_json $xterm256_raw;

my @schemes = ("X"); #all_schemes;
say "Using the following color name schemes: XTerm256 @schemes";

tie my %x11colors, 'Graphics::ColorNames', @schemes;

sub color_diff {
    my ($a, $b) = @_;
    return ($a - $b) * ($a - $b);
}

sub hex_difference {
    my ($a, $b) = @_;
    return color_diff($a & 0xFF, $b & 0xFF)
	+ color_diff(($a >> 8) & 0xFF, ($b >> 8) & 0xFF)
	+ color_diff(($a >> 16) & 0xFF, ($b >> 16) & 0xFF);
}

my %seen;
my %xterm256_rgb;

sub map_to_256 {
    my $rgb = shift;
    my $diff = 0x0FFFFFFF;
    my $best = 0;
    if (exists $xterm256_rgb{$rgb}) {
	return $xterm256_rgb{$rgb};
    }
    $rgb = hex $rgb;
    for (my $n = 0; $n < 256; $n += 1) {
	my $xtermrgb = $seen{"xterm$n"};
	my $cdiff = hex_difference $xtermrgb, $rgb;
	if ($cdiff < $diff) {
	    $best = $n;
	    $diff = $cdiff;
	}
    }
    return $best;
}

my @allcolors;
my $counter = 0;

foreach my $color (@$xterm256colors) {
    my $rgb = $$color{"hexString"};
    my $num = $$color{"colorId"};
    my $ansi = $map_16[$num];
    $rgb =~ s/^\#//;
    
    $counter += 1;
    $seen{"xterm$num"} = hex $rgb;
    $xterm256_rgb{$rgb} = $num;
    push @allcolors, {name => "xterm$num", rgb => "0x$rgb", xterm => int $num,
		      ansi => int $ansi};
}

while (my ($name, $rgb) = each %x11colors) {
    $name =~ s/\s+//;
    $name = lc $name;
    next if exists $seen{$name};
    $counter += 1;
    $seen{$name} = hex $rgb;
    my $xnum = map_to_256 $rgb;
    my $ansi = $map_16[$xnum];
    push @allcolors, {name => $name, rgb => "0x$rgb", xterm => int $xnum,
		      ansi => int $ansi};
}

foreach my $color (@$xterm256colors) {
    my $rgb = $$color{"hexString"};
    my $num = $$color{"colorId"};
    my $name = lc $$color{"name"};
    my $ansi = $map_16[$num];
    $rgb =~ s/^\#//;

    # Add any missing color names    
    next if exists $seen{$name};
    $counter += 1;
    $seen{$name} = hex $rgb;
    push @allcolors, {name => $name, rgb => "0x$rgb", xterm => int $num,
		      ansi => int $ansi};
}

@allcolors = sort { $a->{name} cmp $b->{name} } @allcolors;

open my $output, ">", "game/txt/colors.json";
my $printer = JSON::XS->new->utf8->pretty->canonical;
print $output $printer->encode(\@allcolors);
close $output;
say "Outputted $counter colors.";
