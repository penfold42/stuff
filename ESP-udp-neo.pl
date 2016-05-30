#!/usr/bin/perl
# Author penfold42
# Simple perl script to exercise a WiFi enabled LED strip driver
# tested with penfold42's ESP8266 UDP driver (ascii and binary)
# tested with hyperion and the UDP listener "effect"

use IO::Socket::INET;
use POSIX;
use Time::HiRes qw( usleep ualarm gettimeofday tv_interval nanosleep
		      clock_gettime clock_getres clock_nanosleep 
                      stat );
use Math::Trig;

## Cosmetics for the width and brightness of hands
$width_minute = 18;
$width_hour = 30;
$width_second = 7;
$hand_brightness = 64; 
$hand_effect = "gaussdot20";
$hand_effect = "linear";

$marker_brightness = 4;
#$marker_effect = "blink";
$marker_effect = "fade";
$marker_effect = "none";

# flush after every write
$| = 1;


$protocol = "binary";
$port = "2391";

$numleds = 60;

my ($socket,$data);


$sleep_speed = 10000;

# rainbow colours
@rainbow_colours = (
	[255,0,0], 
	[255,128,0], 
	[255,255,0], 
	[128,255,0], 
	[0,255,0], 
	[0,255,128], 
	[0,255,255], 
	[0,128,255], 
	[0,0,255], 
	[128,0,255], 
	[255,0,255],
	[255,0,128],

);
$rainbow_colours_sz = @rainbow_colours;

for ($i=0; $i<@ARGV ; $i++) {
#	print $ARGV[$i];
	if ($ARGV[$i] =~ /^-h/) {
		show_help();
	}
	if ($ARGV[$i] =~ /^-e/) {
		$effect = $ARGV[$i+1];
	}
	if ( ($ARGV[$i] =~ /^-s/) && ($ARGV[$i+1] =~ /^\d+$/) ) {
		$sleep_speed = $ARGV[$i+1];
	}
	if ( ($ARGV[$i] =~ /^-t/) && ($ARGV[$i+1] =~ /^\S+$/) ) {
		$target = $ARGV[$i+1];
	}
	if ( ($ARGV[$i] =~ /^-p/) && ($ARGV[$i+1] =~ /^[b|a]/) ) {
		$protocol = $ARGV[$i+1];
	}
	if ( ($ARGV[$i] =~ /^-l/) && ($ARGV[$i+1] =~ /^\d+$/) ) {
		$numleds = $ARGV[$i+1];
	}
}
#print "effect is $effect\n";
#print "sleep_speed is $sleep_speed\n";
#print "target is $target\n";


if ($target ne "") {
	($a, $b, $junk) = split(':', $target);
	if ($a ne "") { $host = $a; }	
	if ($b =~ /\d+/) { $port = $b; }	
} else {
	print ("ERROR: Missing target!\n");
	&show_help();

}

if (($host eq "") || ($port eq "")) {
	die ("invalid target, got host: $host port: $port\n");
}

#  We call IO::Socket::INET->new() to create the UDP Socket 
# and bind with the PeerAddr.
$socket = new IO::Socket::INET (
PeerAddr   => "$host:$port",
Proto        => 'udp'
) or die "ERROR in Socket Creation : $!\n";


if ($effect =~ "^c") { &clock(); }
elsif ($effect =~ "^d") { &dice(); }
elsif ($effect =~ "^p") { &phasor(); }
elsif ($effect =~ "^s") { &sinus(); }
elsif ($effect =~ "^ra") { &rainbow(); }
elsif ($effect =~ "^ri") { &rings_effect(); }
else { &clock(); }




sub sine_upshift {
	return (sin(pi*($_[0])/180)+1)/2;
}
sub sine_zeroclamp {
	$temp = sin(pi*($_[0])/180);
	if ($temp >= 0) {
		return $temp;
	} else {
		return 0;
	}
}
sub sine_180clamp {
	if ( ($_[0] >= 0) && ($_[0] < 180)) {
		return sin(pi*($_[0])/180);
	} else {
		return 0;
	}
}

@ledarray;

sub sinus {
	$step = 3;
	$offset=45;


	while (1) {
		for ($i=0; $i < $numleds*$offset; $i+=$step) {
			$angle = $i;
#print "angle is $angle\n";
			for ($led=0; $led<$numleds; $led++) {
				$bright=sine_upshift($angle+$offset*$led);
#				$bright=sine_zeroclamp($angle+$offset*$led);
#				$bright=sine_180clamp($angle-$offset*$led);
$bright=$bright/16;
				($r,$g,$b) = @{ $rainbow_colours[$led%$rainbow_colours_sz] } [0..2];
				$ledarray[$led]->[0] = $r*$bright;
				$ledarray[$led]->[1] = $g*$bright;
				$ledarray[$led]->[2] = $b*$bright;

			}
			send_ledarray();
			usleep($sleep_speed);	
		}
	}
}

sub dice {

	$dice_roll = 0;
	while (1) {
		$time_up = time()+rand(1);
		
		while (time() < $time_up) {
			usleep(100);
			$dice_roll+=rand(6);
			$dice_roll %= 6;
			if ($dice_roll >= 6) { $dice_roll = 0; }
			if ($ctr++ == 191) {
				&dice_display_pattern($dice_roll);
				$ctr = 0;
			}
		}
		&dice_display_pattern($dice_roll);
		usleep(1000000);
	}
}

sub dice_display_pattern {
	@dice_patterns = (
		[0,0,0,0,1,0,0,0,0], 
		[1,0,0,0,0,0,0,0,1], 
		[1,0,0,0,1,0,0,0,1], 
		[1,0,1,0,0,0,1,0,1], 
		[1,0,1,0,1,0,1,0,1], 
		[1,1,1,0,0,0,1,1,1], 
	);

	@pixel9_to_60 = (
		[20,21,22,25],
		[17,18,19,24,38,39,47,48],
		[14,15,16,37],
		[0,1,23,26,27,28,50,51],
		[52,53,54,55,56,57,58,59,60],
		[11,12,13,34,35,36,44,45],
		[2,3,4,29],
		[5,6,7,30,31,32,41,42],
		[8,9,10,33]
	);

	$num = $_[0];
	($r,$g,$b) = @{ $rainbow_colours[$num*2] } [0..2];
	$r /= 16; $g /= 16; $b /= 16;
	clear_ledarray();
	for ($led = 0; $led < 9; $led++) {
		if (1 ==  @{ $dice_patterns[$num] } [$led]) {

			if ($numleds == 9) {
				$ledarray[$led]->[0] = $r;
				$ledarray[$led]->[1] = $g;
				$ledarray[$led]->[2] = $b;
			} else {
				foreach (@{ $pixel9_to_60[$led] } ){
					$ledarray[$_]->[0] = $r;
					$ledarray[$_]->[1] = $g;
					$ledarray[$_]->[2] = $b;
				}
			}
		}
	}
	send_ledarray();
}


sub clock {

	$gmtoffset = 10*3600;
	while (1) {
		($seconds, $microseconds) = gettimeofday;

		$sec = $seconds%86400;
		$sec += $gmtoffset;

		$hour = ($sec/3600)%24;
		$min = ($sec/60)%60;
		$sec = $sec%60;

		
		$sec += $microseconds/1000000;
		$min += $sec/60;
		$hour += $min/60;

#		printf "hour:%2.3f  min:%2.3f sec:%2.3f\n",$hour, $min, $sec;


		clear_ledarray();


# plot the markers lights
		for ($led=0; $led < 60; $led+=5) {
			$ledarray[$led]->[0] = $marker_brightness;
			$ledarray[$led]->[1] = $marker_brightness;
			$ledarray[$led]->[2] = $marker_brightness;
		}
# plot the hour and minute hands
#		plot_hand($min*360/60, $width_minute, $hand_brightness, 0,255,0, $hand_effect);
#		plot_hand(($hour*360/12)%360, $width_hour, $hand_brightness, 0,0,255, $hand_effect);
		plot_hand((180+$min*360/60)%360, $width_minute, $hand_brightness, 0,255,0, $hand_effect);
		plot_hand((180+$hour*360/12)%360, $width_hour, $hand_brightness, 0,0,255, $hand_effect);

# clone a local copy of marker_brightness to mangle
		my $marker_brightness = $marker_brightness;
		if ($marker_effect eq "fade") {
			$marker_brightness = $marker_brightness * 2*abs(0.5-$microseconds/1000000);
		}
		if ($marker_effect eq "blink") {
			if ($microseconds < 500000 ) {
				$marker_brightness >>= 1;
			}
		}

# plot the 12oclock markers lights
		$ledarray[30]->[0] = $marker_brightness*2;
		$ledarray[30]->[1] = $marker_brightness*2;
		$ledarray[30]->[2] = $marker_brightness*0;

# plot the seconds hand
#		plot_hand($sec*360/60, $width_second, $hand_brightness, 255,0,0, $hand_effect);
		plot_hand($sec*360/60, $width_second, $hand_brightness, 255,0,0, $hand_effect, "blend");

		send_ledarray();
		usleep($sleep_speed);
	}
}


sub phasor {
	$brightness = 30;
	while (1) {
		for ($i=0; $i<3600000; $i+=3) {
			clear_ledarray();
			plot_hand(($i/2)%360, 90, $brightness, 255,0,0, "gaussdot01");
			plot_hand(($i/3)%360, 90, $brightness, 0,255,0, "gaussdot20");
#			plot_hand(359-($i/5)%360, 10, $brightness, 0,0,255, "negexp");
			send_ledarray();
			usleep($sleep_speed);	
		}
	}
}

sub plot_hand() {
(my $angle, my $width, my $brightness, $red, my $green, my $blue, my $slope, my $blend)  = @_;
	if ($slope eq "") { $slope = "linear"; }
	if ($blend eq "") { $blend = "blend"; }

	for ($led=0; $led<$numleds; $led++) {
		$diff = abs($led*6-$angle);
		if ($diff < 0) {
			$diff+=360;
		}
		if ($diff > 180) {
			$diff = 360-$diff;
		}

		if ($slope eq "linear") {
			$hwidth = $width/2;
			if ($diff < $hwidth) {
				$v=$brightness/255*($hwidth-$diff)/$hwidth;
			} else {
				$v = 0;
			}
		} elsif ($slope eq "negexp") {
			$v=$brightness/255*exp(-$diff);
		} elsif ($slope eq "gaussdot01") {
			$sd = ($width-1)/6;
			$gauss = exp(-($diff*$diff)/(2*$sd*$sd));
			if ($gauss < 0.001) {$gauss = 0;}
			$v=$brightness/255 * $gauss;
		} elsif ($slope eq "gaussdot20") {
			$sd = ($width+1)/4;
			$gauss = exp(-($diff*$diff)/(2*$sd*$sd));
			if ($gauss < 0.20) {$gauss = 0;}
			$v=$brightness/255 * $gauss;
		}

		if ($blend eq "blend") {
			if ($red>0) { $ledarray[$led]->[0] = ($v*$red + $ledarray[$led]->[0])/2; }
			if ($green>0) { $ledarray[$led]->[1] = ($v*$green + $ledarray[$led]->[1])/2; }
			if ($blue>0) { $ledarray[$led]->[2] = ($v*$blue + $ledarray[$led]->[2])/2; }
		}
		elsif ($blend eq "replace") {
			if ($v > 0) {
				if ($red>0) { $ledarray[$led]->[0] = $v*$red ; }
				if ($green>0) { $ledarray[$led]->[1] = $v*$green ; }
				if ($blue>0) { $ledarray[$led]->[2] = $v*$blue ; }
			}
		}
		elsif ($blend eq "replaceall") {
			if ($v > 0) {
				$ledarray[$led]->[0] = $v*$red ; 
				$ledarray[$led]->[1] = $v*$green ; 
				$ledarray[$led]->[2] = $v*$blue ; 
			}
		}
	}
}


sub rainbow {
	$s=1; # so S=1 makes the purest color (no white)
	$v=0.03; # Brightness V also ranges from 0 to 1, where 0 is the black.
	$offset=30;
	$step = 1;
	while (1) {
		for ($h=0; $h<360; $h+=$step) {

			for ($led=0; $led<$numleds; $led++) {
				($r,$g,$b) = hsv2rgb(($offset*$led+$h)%360,$s,$v);
				$ledarray[$led]->[0] = $r*255;
				$ledarray[$led]->[1] = $g*255;
				$ledarray[$led]->[2] = $b*255;
			}
			send_ledarray();
			usleep($sleep_speed);	
		}

	}
}

sub hsv2rgb {
    my ( $h, $s, $v ) = @_;

    if ( $s == 0 ) {
        return $v, $v, $v;
    }

    $h /= 60;
    my $i = floor( $h );
    my $f = $h - $i;
    my $p = $v * ( 1 - $s );
    my $q = $v * ( 1 - $s * $f );
    my $t = $v * ( 1 - $s * ( 1 - $f ) );

    if ( $i == 0 ) {
        return $v, $t, $p;
    }
    elsif ( $i == 1 ) {
        return $q, $v, $p;
    }
    elsif ( $i == 2 ) {
        return $p, $v, $t;
    }
    elsif ( $i == 3 ) {
        return $p, $q, $v;
    }
    elsif ( $i == 4 ) {
        return $t, $p, $v;
    }
    else {
        return $v, $p, $q;
    }
}

sub clear_ledarray() {
	for ($led=0; $led<$numleds; $led++) {
		$ledarray[$led]->[0] = 0;
		$ledarray[$led]->[1] = 0;
		$ledarray[$led]->[2] = 0;
	}
}
		


sub fill_range {
	my ($start, $end, $red, $green, $blue) = @_;
#	print ("fill_range $start, $end, $red, $green, $blue, \n");
	for (my $led=$start; $led<=$end; $led++) {
		$ledarray[$led]->[0] = $red;
		$ledarray[$led]->[1] = $green;
		$ledarray[$led]->[2] = $blue;
	}
}

sub rings_effect() {
	$s=1; # so S=1 makes the purest color (no white)
	$v=0.03; # Brightness V also ranges from 0 to 1, where 0 is the black.
	$offset=30;
	$step = 1;
	while (1) {
		for ($h=0; $h<360; $h+=$step) {

			for ($led=0; $led<5; $led++) {
				($r,$g,$b) = hsv2rgb(($offset*$led+$h)%360,$s,$v);
				fill_ring($led, $r*255, $g*255, $b*255);
			}
			send_ledarray();
			usleep($sleep_speed);	
		}

		fill_range(0,61,0,0,);
		send_ledarray();
		sleep(1);
	}
}

our @rings = (
	[0,23], 
	[24,39], 
	[40,51], 
	[52,59], 
	[60,60], 
);

sub fill_ring {
@rings = (
	[0,23], 
	[24,39], 
	[40,51], 
	[52,59], 
	[60,60], 
);
	my ($ring, $red, $green, $blue) = @_;
	
#	print ("fill_ring $ring, $red, $green, $blue, \n");
	($start,$end) = @{ $rings[$ring] } [0..1];
	fill_range($start, $end, $red, $green, $blue);
}

sub NOrings_effect() {
#1, 8, 12, 16, 24

	$bright=$bright/16;
	$bright=1/16;
	while (1) {
		for ($led = 0; $led<5; $led++) {
print "led $led\n";
			($r,$g,$b) = @{ $rainbow_colours[$led%$rainbow_colours_sz] } [0..2];
			($start,$end) = @{ $rings[$led] } [0..1];
			fill_range($start, $end, $r*$bright, $g*$bright, $b*$bright);
		}
		send_ledarray();
		usleep($sleep_speed);	
	}
}

sub send_ledarray() {
	if ($protocol =~ /^b/) {
		&send_ledarray_binary();
	} else {
		&send_ledarray_ascii();
	}
}

sub send_ledarray_binary() {
	$data = "";
	for ($led=0; $led<$numleds; $led++) {
		$data .= pack("C*",$ledarray[$led]->[0]);
		$data .= pack("C*",$ledarray[$led]->[1]);
		$data .= pack("C*",$ledarray[$led]->[2]);
	}
	$socket->send($data);
}

sub send_ledarray_ascii() {
# clear all LEDs
	my $data = "F0000000\n";
# update any non-zero LEDs
	for ($led=0; $led<$numleds; $led++) {
		if ( $ledarray[$led]->[0]+ $ledarray[$led]->[1]+ $ledarray[$led]->[2] ) {
			$data .= sprintf ("%02x%02x%02x%02x\n", $led, $ledarray[$led]->[0], $ledarray[$led]->[1], $ledarray[$led]->[2] );
		}
	}
#print $data;
	$socket->send($data);
}

sub show_help() {
print qq~ESP-udp-neo.pl

Sends UDP packets to the host to drive a WS2812 strip
	-effect clock		A clock using a ring of LEDs
	-effect phasor		2 spinning colour bands
	-effect rainbow		Can you guess?
	-effect sinus		fixed colours modulated by rotating sine brightness waves
	-effect dice		dice rolls on a 3x3 matrix
	-effect ring		pretty rainbows on a series of concentric rings
	-sleep <sleep speed>	wait time every loop
	-target <host>[:port]	target host (and port)
	-proto <b|a>		UDP protocol Binary or Ascii
	-leds <n>		assume maximum of "n" leds
~;
exit;
}

