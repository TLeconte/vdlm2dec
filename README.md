# VDLM2DEC
vdlm2dec is a vdl mode 2 decoder with built-in rtl_sdr or airspy front end.

It could decode up to 8 frequencies simultaneously ( but in the same 2Mhz range )

## Usage
> vdlm2dec  [-l logfile]  [-g gain] [-r rtldevicenumber]  Frequencies(Mhz)

 -r rtldevicenumber :	decode from rtl dongle number rtldevicenumber (don't set it for airspy)
 
 -g gain :		set preamp gain in tenth of db (ie -g 90 for +9db)

 -p ppm :		set rtl sdr ppm frequency (rtl_sdr only)

 -q :			quiet
 
 -l logfile :		output log (stderr by default)

 -J :			json output

 -j addr:port		send to addr:port UDP packets in json that could be read by acarsserv

 
## Examples

> ./vdlm2dec -r 0 136.725 136.775 136.975 

    [#3 (F:136.975 P:-4.9) 26/01/2018 21:37:08.327 --------------------------------
    Response from Aircraft: 440030 to 1098CA 
    Frame-S: Nr:5 SREJ

    [#3 (F:136.975 P:-4.0) 26/01/2018 21:37:08.575 --------------------------------
    Command from Aircraft: 400D8D to 1098AA 
    Frame-U: XID
    Connection management: HO|VDA|ESN
    XID sequencing 0:0
    Specific options: GDA:ESN:IHS:BHN:BCN
    Destination airport EGMC
    Aircraft Position 45.8  -0.3 alt: 38000

    [#3 (F:136.975 P:-7.2) 26/01/2018 21:39:31.155 --------------------------------
    Command from Aircraft: 440575 to 213C97 
    Frame-I: Ns:3 Nr:7
    ACARS
    Aircraft reg: OE-IAP Flight id: 3V0015
    Mode: 2 Msg. label: 85
    Block id: 1 Ack: !
    Msg. no: M29A
    Message :
    3C01 POSWX0015/180126/LEZL/EBLG/OE-IAP
    /POS46.36917/0.1911/OVR2139
    /ALT33592/FOB0019/MCH770.000000/SAT        


## Compilation
vdlm2dec must compile directly on any modern Linux distrib.
It has been tested on x86_64 with fedora 27, on tegra with Ubuntu 14.04.5 

It depends on some external libraries :
 * librtlsdr for software radio rtl dongle input (http://sdr.osmocom.org/trac/wiki/rtl-sdr)
 * libusb

> make -f  Makefile.rtl
for rtl-sdr 

> make -f Makefile.air
for airspy

## Frequency correction for rtl-sdr
 1) when receiving a message ex :
     [#1 (F:136.725 P:-3.7) 26/01/2018 21:34:57.633 --------------------------------
  read the ppm displayed in P: field.
 2) Guess an average, round to near integer
 3) set the -p option as the opposite of this number (ex : -p 4 )

# Acarsserv

acarsserv is a companion program for vdlm2dec. It listens to acars messages on UDP coming from one or more acarsdec processes and stores them in a sqlite database.

See : [acarsserv](https://github.com/TLeconte/acarsserv)

Note : vdlm2dec could only send packet over the network on json format. It does not support the acarsdec old binary format.

## Copyrights 
vdlm2dec and acarsserv are Copyright Thierry Leconte 2015-2018

These code are free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License version 2
published by the Free Software Foundation.

They include [cJSON](https://github.com/DaveGamble/cJSON) Copyright (c) 2009-2017 Dave Gamble a
nd cJSON contributors

