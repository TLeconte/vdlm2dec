# VDLM2DEC
vdlm2dec is a vdl mode 2 decoder with built-in rtl_sdr or airspy front end.

It could decode up to 8 frequencies simultaneously ( but in the same 2Mhz range )

## Usage
> vdlm2dec  [-v|V] [-l logfile]  [-g gain] [-r rtldevicenumber]  Frequencies(Mhz)

 -r rtldevicenumber :	decode from rtl dongle number rtldevicenumber (don't set it for airspy)
 
 -g gain :		set preamp gain in tenth of db (ie -g 90 for +9db)

 -p ppm :		set rtl sdr ppm frequency (rtl_sdr only)

 -v :			verbose
 
 -l logfile :		output log (stderr by default)

## Examples

./vdlm2dec -r 0 136.725 136.775 136.975 

## Compilation
vdlm2dec must compile directly on any modern Linux distrib.
It has been tested on x86_64 with fedora 25, on tegra with Ubuntu 14.04.5 ,and un raspy

It depends on some external libraries :
 * librtlsdr for software radio rtl dongle input (http://sdr.osmocom.org/trac/wiki/rtl-sdr)
 * libusb

> make -f  Makefile.rtl
for rtl-sdr 

> make -f Makefile.air
for airspy

## Frequency correction for rtl-sdr
 1) In verbose mode 2 , read the ppm displayed for each message.
 2) Guess an average, round to near integer
 3) set the -p option as the opposite of this number


