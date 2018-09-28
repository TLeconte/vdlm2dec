# VDLM2DEC
vdlm2dec is a vdl mode 2 decoder with built-in rtl_sdr or airspy front end.

It could decode up to 8 frequencies simultaneously ( but in the same 2Mhz range )

## Usage
> vdlm2dec  [-l logfile]  [-g gain] [-i stid] [-v] [-q] [-J] [-l logfile] [-r rtldevicenumber]  Frequencies(Mhz)

 -r rtldevicenumber :	decode from rtl dongle number rtldevicenumber (don't set it for airspy)
 
 -g gain :		set preamp gain in tenth of db (ie -g 90 for +9db) (rtl-sdr)

 -g gain :		set linearity gain (0 to 21).By default use maximum gain (airspy)

 -p ppm :		set rtl sdr ppm frequency (rtl-sdr only)

 -i stid :		local receiver station id

 -v :			verbose output

 -q :			quiet output

 -J :			json logfile output

 -l logfile :		output log (stderr by default)

 -j addr:port		send to addr:port UDP packets in json that could be read by acarsserv

 -G :			output messages from ground station

 -E :			output empty messages

 -U :			output undecoded messages

 
## Examples
For rtl-sdr :
> ./vdlm2dec -r 0 136.725 136.775 136.875 136.975 

For airspy :
> ./vdlm2dec 136.725 136.775 136.875 136.975 

    [#4 (F:136.975 P:-2.1) 11/03/2018 15:52:27.259 --------------------------------
    Command from Aircraft:3986E5 (airborne) to GroundD:2198B7 
    Frame-I: Ns:0 Nr:0
    ACARS
    Aircraft reg: F-HBXF Flight id: YS7656
    Mode: 2 Msg. label: Q0
    Block id: 1 Ack: !
    Msg. no: M36A
    
    [#4 (F:136.975 P:-6.3) 11/03/2018 15:52:27.831 --------------------------------
    Command from Aircraft:3946EC (airborne) to GroundD:2190F7 
    Frame-I: Ns:1 Nr:0
    unknown data
    
    [#1 (F:136.725 P:-3.5) 11/03/2018 15:52:28.240 --------------------------------
    Response from Aircraft:406321 (airborne) to GroundD:1191EA 
    Frame-S: Nr:4 RR
    
    [#3 (F:136.875 P:-5.8) 11/03/2018 15:52:29.009 --------------------------------
    Command from Aircraft:398517 (airborne) to GroundD:27CB21 
    Frame-I: Ns:5 Nr:6
    unknown data
    
    [#4 (F:136.975 P:-5.0) 11/03/2018 15:52:29.006 --------------------------------
    Command from Aircraft:440176 (airborne) to GroundD:1098CA 
    Frame-I: Ns:5 Nr:6
    ACARS
    Aircraft reg: OE-IJF Flight id: U287HY
    Mode: 2 Msg. label: H2
    Block id: 5 Ack: !
    Msg. no: M60A
    Message :
    02A111552LFRSLPPRN47097W001372111532  85P112     131P107205018G     185P097209026G     239P087215031G     292P072214037G     339P065220041G     378P052221044G     406P042226047G     427P035222046G     448P03022
    Block End
    
    [#2 (F:136.775 P:-6.6) 11/03/2018 15:52:29.357 --------------------------------
    Command from Aircraft:461FA1 (airborne) to GroundD:280645 
    Frame-I: Ns:4 Nr:7
    unknown data

    [#1 (F:136.725 P:-2.5) 11/03/2018 15:52:29.359 --------------------------------
    Command from Aircraft:440630 (airborne) to GroundD:11989A 
    Frame-I: Ns:1 Nr:4
    unknown data

## Compilation
vdlm2dec must compile directly on any modern Linux distrib.
It has been tested on x86_64 with fedora 27, on tegra TK1,TX1 with Ubuntu  

It depends on some external libraries :
 * libusb
 * librtlsdr for software radio rtl dongle input (http://sdr.osmocom.org/trac/wiki/rtl-sdr)
 * libairspy for airspy (https://github.com/airspy/airspyone_host)

### Make
#### for rtl-sdr 

> make -f  Makefile.rtl

#### for airspy R2

> make -f Makefile.air

#### for airspy mini
In vdlm2.h change :

    #ifdef WITH_AIR
    #define SDRINRATE 5000000
    #define SDRCLK  1250
    #endif

into 

    #ifdef WITH_AIR
    #define SDRINRATE 6000000
    #define SDRCLK  1500
    #endif

comment the following lines in air.c :

    //airspy_r820t_write(device, 10, 0xB0 | (15-j));
    //airspy_r820t_write(device, 11, 0xE0 | (15-i));

then 

> make -f Makefile.air


## Frequency correction for rtl-sdr
 1) when receiving a message ex :
     [#1 (F:136.725 P:-3.7) 26/01/2018 21:34:57.633 --------------------------------
  read the ppm displayed in P: field.
 2) Guess an average, round to near integer
 3) set the -p option as the opposite of this number (ex : -p 4 )

# Acarsserv

acarsserv is a companion program for vdlm2dec. It listens to acars messages on UDP coming from one or more vdlm2dec processes and stores them in a sqlite database.

See : [acarsserv](https://github.com/TLeconte/acarsserv)

Note : vdlm2dec could only send packet over the network on json format. It does not support the acarsdec old binary format.

## Copyrights 
vdlm2dec and acarsserv are Copyright Thierry Leconte 2015-2018

These code are free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License version 2
published by the Free Software Foundation.

They include [cJSON](https://github.com/DaveGamble/cJSON) Copyright (c) 2009-2017 Dave Gamble a
nd cJSON contributors

