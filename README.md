# VDLM2DEC
vdlm2dec is a vdl mode 2 decoder with built-in rtl_sdr front end.

It could decode up to 8 frequencies simultaneously ( but in the same 2Mhz range )

## Usage
> vdlm2dec  [-v|V] [-l logfile]  [-g gain] [-r rtldevicenumber]  Frequencies(Mhz)

 -r rtldevicenumber :	decode from rtl dongle number rtldevicenumber
 
 -g gain :		set rtl preamp gain in tenth of db (ie -g 90 for +9db)

 -p ppm :		set rtl sdr ppm frequency 

 -v :			verbose
 
 -l logfile :		output log (stderr by default)

## Examples

vdlm2 -r 0 136.725 136.775 136.975 

## Compilation
vdlm2dec must compile directly on any modern Linux distrib.
It has been tested on x86_64 with fedora 25, and on tegra with Ubuntu 14.04.5 

It depends on some external libraries :
 * librtlsdr for software radio rtl dongle input (http://sdr.osmocom.org/trac/wiki/rtl-sdr)
 * libusb

## Frequency correction
 1) In verbose mode, read the ppm displayed for each message.
 2) Guess an average
 3) set the -p option as the opposite of this average

## TODO

 * improve demodulator CPU usage
 * decode more vdlm2 protocols (looking for info)
 * send ACARS to acarsserv or planeploter (easy copy/paste from acarsdec sources: I accept commit requests :-) )
 * lots of other things
