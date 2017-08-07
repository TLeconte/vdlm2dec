# VDLM2DEC
vdlm2dec is a vdl mode 2 decoder with built-in rtl_sdr front end.

For now, it decodes only ACARS and some XID parameters

## Usage
> vdlm2dec  [-v|V] [-l logfile]  [-g gain] [-r rtldevicenumber]  Frequency(Mhz)

 -r rtldevicenumber :	decode from rtl dongle number rtldevicenumber
 
 -g gain :		set rtl preamp gain in tenth of db (ie -g 90 for +9db)
 
 -v :			verbose
 
 -l logfile :		output log (stderr by default)

## Examples

vdlm2 -r 0 136.975 

Other known frequencies in West Europe :
 * 136.875
 * 136.725
 * 136.775

## Compilation
vdlm2dec must compile directly on any modern Linux distrib.
It has been tested on x86_64 with fedora 25, and on tegra with Ubuntu 14.04.5 

It depends on some external libraries :
 * librtlsdr for software radio rtl dongle input (http://sdr.osmocom.org/trac/wiki/rtl-sdr)
 * libusb

## TODO

 * multifrequencies reception (like acarsdec)
 * improve demodulator CPU usage
 * decode more vdlm2 protocols (looking for info)
 * send ACARS to acarsserv or planeploter (easy copy/paste from acarsdec sources: I accept commit requests :-) )
 * lots of other things
