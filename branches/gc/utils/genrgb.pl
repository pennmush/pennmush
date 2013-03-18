#!/usr/bin/perl
#
# Script for generating the color name to RGB component maps in
# hdrs/rgb.h and src/rgbtab.gperf (Which is itself processed by gperf
# to create the name to color lookup table).
#
# Uses the standard X and XTerm colors by default. Invoke with
# --rgb=/path/to/rgb.txt to add your own custom color names (The only
# reason you should ever need to run this script).  Same format as the
# standard X one: Lines consisting of
#
# R G B Name of color
#
#
# Requires a fairly modern perl and the Graphics::ColorNames
# package. Install off of CPAN or your package manager
# (libgraphics-colornames-perl on Debian/Ubuntu/etc.,
# perl-graphics-colornames on arch)

use strict;
use warnings;
use autodie;
use feature qw/say state/;
use integer;
use Getopt::Long;
use Graphics::ColorNames;
use Module::Loaded;

our @rgbfiles = ();
GetOptions("rgb=s" => \@rgbfiles);
@rgbfiles = split /,/, join(',', @rgbfiles);

# XTerm Colors
package Graphics::ColorNames::XTerm {
    use integer;
    sub NamesRgbTable() {
	return
	{
	    "xterm0" => 0x000000,
	    "xterm1" => 0x800000,
	    "xterm2" => 0x008000,
	    "xterm3" => 0x808000,
	    "xterm4" => 0x000080,
	    "xterm5" => 0x800080,
	    "xterm6" => 0x008080,
	    "xterm7" => 0xc0c0c0,
	    "xterm8" => 0x808080,
	    "xterm9" => 0xff0000,
	    "xterm10" => 0x00ff00,
	    "xterm11" => 0xffff00,
	    "xterm12" => 0x0000ff,
	    "xterm13" => 0xff00ff,
	    "xterm14" => 0x00ffff,
	    "xterm15" => 0xffffff,
	    "xterm16" => 0x000000,
	    "xterm17" => 0x00005f,
	    "xterm18" => 0x000087,
	    "xterm19" => 0x0000af,
	    "xterm20" => 0x0000d7,
	    "xterm21" => 0x0000ff,
	    "xterm22" => 0x005f00,
	    "xterm23" => 0x005f5f,
	    "xterm24" => 0x005f87,
	    "xterm25" => 0x005faf,
	    "xterm26" => 0x005fd7,
	    "xterm27" => 0x005fff,
	    "xterm28" => 0x008700,
	    "xterm29" => 0x00875f,
	    "xterm30" => 0x008787,
	    "xterm31" => 0x0087af,
	    "xterm32" => 0x0087d7,
	    "xterm33" => 0x0087ff,
	    "xterm34" => 0x00af00,
	    "xterm35" => 0x00af5f,
	    "xterm36" => 0x00af87,
	    "xterm37" => 0x00afaf,
	    "xterm38" => 0x00afd7,
	    "xterm39" => 0x00afff,
	    "xterm40" => 0x00d700,
	    "xterm41" => 0x00d75f,
	    "xterm42" => 0x00d787,
	    "xterm43" => 0x00d7af,
	    "xterm44" => 0x00d7d7,
	    "xterm45" => 0x00d7ff,
	    "xterm46" => 0x00ff00,
	    "xterm47" => 0x00ff5f,
	    "xterm48" => 0x00ff87,
	    "xterm49" => 0x00ffaf,
	    "xterm50" => 0x00ffd7,
	    "xterm51" => 0x00ffff,
	    "xterm52" => 0x5f0000,
	    "xterm53" => 0x5f005f,
	    "xterm54" => 0x5f0087,
	    "xterm55" => 0x5f00af,
	    "xterm56" => 0x5f00d7,
	    "xterm57" => 0x5f00ff,
	    "xterm58" => 0x5f5f00,
	    "xterm59" => 0x5f5f5f,
	    "xterm60" => 0x5f5f87,
	    "xterm61" => 0x5f5faf,
	    "xterm62" => 0x5f5fd7,
	    "xterm63" => 0x5f5fff,
	    "xterm64" => 0x5f8700,
	    "xterm65" => 0x5f875f,
	    "xterm66" => 0x5f8787,
	    "xterm67" => 0x5f87af,
	    "xterm68" => 0x5f87d7,
	    "xterm69" => 0x5f87ff,
	    "xterm70" => 0x5faf00,
	    "xterm71" => 0x5faf5f,
	    "xterm72" => 0x5faf87,
	    "xterm73" => 0x5fafaf,
	    "xterm74" => 0x5fafd7,
	    "xterm75" => 0x5fafff,
	    "xterm76" => 0x5fd700,
	    "xterm77" => 0x5fd75f,
	    "xterm78" => 0x5fd787,
	    "xterm79" => 0x5fd7af,
	    "xterm80" => 0x5fd7d7,
	    "xterm81" => 0x5fd7ff,
	    "xterm82" => 0x5fff00,
	    "xterm83" => 0x5fff5f,
	    "xterm84" => 0x5fff87,
	    "xterm85" => 0x5fffaf,
	    "xterm86" => 0x5fffd7,
	    "xterm87" => 0x5fffff,
	    "xterm88" => 0x870000,
	    "xterm89" => 0x87005f,
	    "xterm90" => 0x870087,
	    "xterm91" => 0x8700af,
	    "xterm92" => 0x8700d7,
	    "xterm93" => 0x8700ff,
	    "xterm94" => 0x875f00,
	    "xterm95" => 0x875f5f,
	    "xterm96" => 0x875f87,
	    "xterm97" => 0x875faf,
	    "xterm98" => 0x875fd7,
	    "xterm99" => 0x875fff,
	    "xterm100" => 0x878700,
	    "xterm101" => 0x87875f,
	    "xterm102" => 0x878787,
	    "xterm103" => 0x8787af,
	    "xterm104" => 0x8787d7,
	    "xterm105" => 0x8787ff,
	    "xterm106" => 0x87af00,
	    "xterm107" => 0x87af5f,
	    "xterm108" => 0x87af87,
	    "xterm109" => 0x87afaf,
	    "xterm110" => 0x87afd7,
	    "xterm111" => 0x87afff,
	    "xterm112" => 0x87d700,
	    "xterm113" => 0x87d75f,
	    "xterm114" => 0x87d787,
	    "xterm115" => 0x87d7af,
	    "xterm116" => 0x87d7d7,
	    "xterm117" => 0x87d7ff,
	    "xterm118" => 0x87ff00,
	    "xterm119" => 0x87ff5f,
	    "xterm120" => 0x87ff87,
	    "xterm121" => 0x87ffaf,
	    "xterm122" => 0x87ffd7,
	    "xterm123" => 0x87ffff,
	    "xterm124" => 0xaf0000,
	    "xterm125" => 0xaf005f,
	    "xterm126" => 0xaf0087,
	    "xterm127" => 0xaf00af,
	    "xterm128" => 0xaf00d7,
	    "xterm129" => 0xaf00ff,
	    "xterm130" => 0xaf5f00,
	    "xterm131" => 0xaf5f5f,
	    "xterm132" => 0xaf5f87,
	    "xterm133" => 0xaf5faf,
	    "xterm134" => 0xaf5fd7,
	    "xterm135" => 0xaf5fff,
	    "xterm136" => 0xaf8700,
	    "xterm137" => 0xaf875f,
	    "xterm138" => 0xaf8787,
	    "xterm139" => 0xaf87af,
	    "xterm140" => 0xaf87d7,
	    "xterm141" => 0xaf87ff,
	    "xterm142" => 0xafaf00,
	    "xterm143" => 0xafaf5f,
	    "xterm144" => 0xafaf87,
	    "xterm145" => 0xafafaf,
	    "xterm146" => 0xafafd7,
	    "xterm147" => 0xafafff,
	    "xterm148" => 0xafd700,
	    "xterm149" => 0xafd75f,
	    "xterm150" => 0xafd787,
	    "xterm151" => 0xafd7af,
	    "xterm152" => 0xafd7d7,
	    "xterm153" => 0xafd7ff,
	    "xterm154" => 0xafff00,
	    "xterm155" => 0xafff5f,
	    "xterm156" => 0xafff87,
	    "xterm157" => 0xafffaf,
	    "xterm158" => 0xafffd7,
	    "xterm159" => 0xafffff,
	    "xterm160" => 0xd70000,
	    "xterm161" => 0xd7005f,
	    "xterm162" => 0xd70087,
	    "xterm163" => 0xd700af,
	    "xterm164" => 0xd700d7,
	    "xterm165" => 0xd700ff,
	    "xterm166" => 0xd75f00,
	    "xterm167" => 0xd75f5f,
	    "xterm168" => 0xd75f87,
	    "xterm169" => 0xd75faf,
	    "xterm170" => 0xd75fd7,
	    "xterm171" => 0xd75fff,
	    "xterm172" => 0xd78700,
	    "xterm173" => 0xd7875f,
	    "xterm174" => 0xd78787,
	    "xterm175" => 0xd787af,
	    "xterm176" => 0xd787d7,
	    "xterm177" => 0xd787ff,
	    "xterm178" => 0xd7af00,
	    "xterm179" => 0xd7af5f,
	    "xterm180" => 0xd7af87,
	    "xterm181" => 0xd7afaf,
	    "xterm182" => 0xd7afd7,
	    "xterm183" => 0xd7afff,
	    "xterm184" => 0xd7d700,
	    "xterm185" => 0xd7d75f,
	    "xterm186" => 0xd7d787,
	    "xterm187" => 0xd7d7af,
	    "xterm188" => 0xd7d7d7,
	    "xterm189" => 0xd7d7ff,
	    "xterm190" => 0xd7ff00,
	    "xterm191" => 0xd7ff5f,
	    "xterm192" => 0xd7ff87,
	    "xterm193" => 0xd7ffaf,
	    "xterm194" => 0xd7ffd7,
	    "xterm195" => 0xd7ffff,
	    "xterm196" => 0xff0000,
	    "xterm197" => 0xff005f,
	    "xterm198" => 0xff0087,
	    "xterm199" => 0xff00af,
	    "xterm200" => 0xff00d7,
	    "xterm201" => 0xff00ff,
	    "xterm202" => 0xff5f00,
	    "xterm203" => 0xff5f5f,
	    "xterm204" => 0xff5f87,
	    "xterm205" => 0xff5faf,
	    "xterm206" => 0xff5fd7,
	    "xterm207" => 0xff5fff,
	    "xterm208" => 0xff8700,
	    "xterm209" => 0xff875f,
	    "xterm210" => 0xff8787,
	    "xterm211" => 0xff87af,
	    "xterm212" => 0xff87d7,
	    "xterm213" => 0xff87ff,
	    "xterm214" => 0xffaf00,
	    "xterm215" => 0xffaf5f,
	    "xterm216" => 0xffaf87,
	    "xterm217" => 0xffafaf,
	    "xterm218" => 0xffafd7,
	    "xterm219" => 0xffafff,
	    "xterm220" => 0xffd700,
	    "xterm221" => 0xffd75f,
	    "xterm222" => 0xffd787,
	    "xterm223" => 0xffd7af,
	    "xterm224" => 0xffd7d7,
	    "xterm225" => 0xffd7ff,
	    "xterm226" => 0xffff00,
	    "xterm227" => 0xffff5f,
	    "xterm228" => 0xffff87,
	    "xterm229" => 0xffffaf,
	    "xterm230" => 0xffffd7,
	    "xterm231" => 0xffffff,
	    "xterm232" => 0x080808,
	    "xterm233" => 0x121212,
	    "xterm234" => 0x1c1c1c,
	    "xterm235" => 0x262626,
	    "xterm236" => 0x303030,
	    "xterm237" => 0x3a3a3a,
	    "xterm238" => 0x444444,
	    "xterm239" => 0x4e4e4e,
	    "xterm240" => 0x585858,
	    "xterm241" => 0x626262,
	    "xterm242" => 0x6c6c6c,
	    "xterm243" => 0x767676,
	    "xterm244" => 0x808080,
	    "xterm245" => 0x8a8a8a,
	    "xterm246" => 0x949494,
	    "xterm247" => 0x9e9e9e,
	    "xterm248" => 0xa8a8a8,
	    "xterm249" => 0xb2b2b2,
	    "xterm250" => 0xbcbcbc,
	    "xterm251" => 0xc6c6c6,
	    "xterm252" => 0xd0d0d0,
	    "xterm253" => 0xdadada,
	    "xterm254" => 0xe4e4e4,
	    "xterm255" => 0xeeeeee,
	};
    }
    # spork's color map
    our @map_16 = qw/
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
};
mark_as_loaded "Graphics::ColorNames::XTerm";

open RGBH, ">", "hdrs/rgb.h";
open GPERF, ">", "src/rgbtab.gperf";

# Print out stock header information for the autogenerated files.
print RGBH <<'EOC';
/* rgb.h
 *
 * Generated by utils/genrgb.pl, this file contains mappings of color names to RGB values.
 * Used by markup.c for ansi() color.
 */

/** Holds info on a named color */
struct RGB_COLORMAP {
  char *name; /**< Color name */
  uint32_t hex; /**< Hex value of color */
  int as_xterm; /**< XTERM color code (0-255) */
  int as_ansi; /**< Color (0-7), +256 for highlight. Add 30 for FG color or 40 for BG color */
};

struct RGB_COLORMAP allColors[] = {
EOC

print GPERF <<'EOC';
/* Gperf data file for color name lookup. Created by utils/genrgb.pl */
%language=ANSI-C
%define hash-function-name colorname_hash
%define lookup-function-name colorname_lookup
%enum
%readonly-tables
%struct-type
%define initializer-suffix ,0,0,0

/*

I looked into using these options to generate a global array of
color pairs to replace the one in hdrs/rgb.h, but gperf reorders the
list and puts lots of null entries in as needed when building its
static hash tables. Makes things that need to scan through the entire
list (Like colors(*foo*)) a pain, and having all the xterm colors in
one block is handy for the mapping code, so those would have to be
duplicated anyways. Not worth the bother.

* %global-table
* %define word-array-name allColors
*/

struct RGB_COLORMAP;
%%
EOC

our (%xcolors, %colors);

tie %xcolors, 'Graphics::ColorNames', 'XTerm';
tie %colors, 'Graphics::ColorNames', 'X', @rgbfiles;

sub xtnum {   
    state $re = qr/^xterm(\d+)/;
    $_[0] =~ m/$re/;
    return $1;
}

sub byxterm {
    my $an = xtnum $a;
    my $bn = xtnum $b;
    return $an <=> $bn;
}

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


our @xterm = sort byxterm keys %xcolors;
our @xterm2 = @xterm;
splice @xterm2, 0, 16;

sub map_to_256 {
    my $rgb = hex shift;
    my $diff = 0x0FFFFFFF;
    my $best = "xterm0";
    foreach my $xterm (@xterm2) {
	my $cdiff = hex_difference hex $xcolors{$xterm}, $rgb;
	if ($cdiff < $diff) {
	    $best = $xterm;
	    $diff = $cdiff;
	}
    }
    return xtnum $best;
}

sub map_to_ansi {
    my $xnum = shift;
    return $Graphics::ColorNames::XTerm::map_16[$xnum];
}

foreach my $xterm (@xterm) {
    my $rgb = $xcolors{$xterm};
    my $xnum = xtnum $xterm;
    my $ansi = map_to_ansi $xnum;
    say RGBH "  {\"$xterm\", 0x$rgb, $xnum, $ansi},";
    say GPERF "$xterm, 0x$rgb, $xnum, $ansi";
}

while (my ($name, $rgb) = each %colors) {
    state %seen;
    $name =~ s/\s+//;
    $name = lc $name;
    next if exists $seen{$name};
    $seen{$name} = 1;
    my $xnum = map_to_256 $rgb;
    my $ansi = map_to_ansi $xnum;
    say RGBH "  {\"$name\", 0x$rgb, $xnum, $ansi},";
    say GPERF "$name, 0x$rgb, $xnum, $ansi";
}

# Print end of file data
print RGBH <<'EOC';
  {NULL, 0, 0, 0}
};

/** Info on a color from the 16-color ANSI palette */
struct COLORMAP_16 {
  int id; /**< Code for this color (0-7)
  char desc; /**< Lowercase char representing color */
  uint8_t hilite; /**< Is this char highlighted? */
  uint32_t hex; /**< Hex code for this color */
};

/* Taken from the xterm color chart on wikipedia */
struct COLORMAP_16 colormap_16[] = {
  /* normal colors */
  {0, 'x', 0, 0x000000},
  {1, 'r', 0, 0xcd0000},
  {2, 'g', 0, 0x00cd00},
  {3, 'y', 0, 0xcdcd00},
  {4, 'b', 0, 0x0000ee},
  {5, 'm', 0, 0xcd00cd},
  {6, 'c', 0, 0x00cdcd},
  {7, 'w', 0, 0xe5e5e5},

  /* Hilite colors */
  {0, 'x', 1, 0x7f7f7f},
  {1, 'r', 1, 0xff0000},
  {2, 'g', 1, 0x00ff00},
  {3, 'y', 1, 0xffff00},
  {4, 'b', 1, 0x5c5cff},
  {5, 'm', 1, 0xff00ff},
  {6, 'c', 1, 0x00ffff},
  {7, 'w', 1, 0xffffff},

  {-1, 0, 0, 0}
};
EOC

say GPERF '%%';

untie %colors;
close RGBH;
close GPERF;
