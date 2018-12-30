# VDLM2DEC
vdlm2dec is a vdl mode 2 decoder with built-in rtl_sdr or airspy front end.

It could decode up to 8 frequencies simultaneously ( but in the same 2Mhz range )

It decodes ARINC-622 ATS applications (ADS-C, CPDLC) via [libacars](https://github.com/szpajder/libacars) library

## Usage
> vdlm2dec  [-l logfile]  [-g gain] [-i stid] [-v] [-q] [-J] [-l logfile] [-r rtldevicenumber]  Frequencies(Mhz)

 -r rtldevicenumber :	decode from rtl dongle number rtldevicenumber (don't set it for airspy)
 
 -g gain :		set preamp gain in tenth of db (ie -g 90 for +9db) (rtl-sdr)

 -g gain :		set linearity gain (0 to 21).By default use maximum gain (airspy)

 -p ppm :		set rtl sdr ppm frequency (rtl-sdr only)

 -i stid :		local receiver station id

 -b filter:             filter output by label (ex: -b "H1:Q0" : only output messages  with label H1 or Q0"

 -v :			verbose output

 -q :			quiet output

 -J :			json output

 -R :			Flights & Aircrafts registration json format output

 -a :			aircraft registration csv format output

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
    
JSON out :
> ./vdlm2dec -J -r 0 136.725 136.775 136.875 136.975

    {"timestamp":1543675373.6302309,"channel":2,"freq":136.875,"icao":4221787,"toaddr":2499139,"mode":"2","label":"10","block_id":"4","ack":"!","tail":"G-GATL","flight":"BA030T","msgno":"M16A","text":"MET01LPPR   "}
    {"timestamp":1543675375.78301,"channel":2,"freq":136.875,"icao":4221787,"toaddr":2499139,"mode":"2","label":"_d","block_id":"5","ack":"E","tail":"G-GATL","flight":"BA030T","msgno":"S57A"}
    {"timestamp":1543675376.9893639,"channel":2,"freq":136.875,"icao":5024073,"toaddr":2199779,"dsta":"GMAD"}
    {"timestamp":1543675377.651469,"channel":3,"freq":136.975,"icao":4921892,"toaddr":1087690,"mode":"2","label":"H2","block_id":"5","ack":"!","tail":"HB-JXK","flight":"DS51QH","msgno":"M69E","text":"33297M517308091G    "}


Flights & Aircraft registration JSON output :
> ./vdlm2dec -R -r 0 136.725 136.775 136.875 136.975

    {"timestamp":1546187157.8686321,"icao":"3C656D","tail":"D-AIKM"}
    {"timestamp":1546187160.7268431,"icao":"020095","tail":"CN-ROU"}
    {"timestamp":1546187164.189714,"flight":"BA092F","depa":"EGKK","dsta":"LPFR","icao":"406B84","tail":"G-GATS"}
    {"timestamp":1546187173.913542,"icao":"440395","tail":"OE-IZO"}
    {"timestamp":1546187215.3877859,"flight":"CJ8483","depa":"EGLC","dsta":"LEPA"}

Aircraft registration csv output:
> ./vdlm2dec -a -r 0 136.725 136.775 136.875 136.975
    
    4D201F,9H-AEI
    4CA2C9,EI-DEP
    49514B,CS-TJK
    400D8B,G-EZAA

## Compilation
vdlm2dec must compile directly on any modern Linux distrib.
It has been tested on x86_64 with fedora 27, on tegra TK1,TX1 with Ubuntu  

It depends on some external libraries :
 * librtlsdr for software radio rtl dongle input (http://sdr.osmocom.org/trac/wiki/rtl-sdr)
 * libairspy for airspy (https://github.com/airspy/airspyone_host)
 * optionaly libacars for decoding ATS applications (https://github.com/szpajder/libacars)

### Compile

#### For rtl_sdr :

    mkdir build
    cd build
    cmake .. -Drtl=ON
    make
    sudo make install


#### For airspy :

    mkdir build
    cd build
    cmake .. -Dairspy=ON
    make
    sudo make install

#### For airspy mini
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

then compile as for the airspy (see above)

#### For raspberry Pi and others ARM machines :

The gcc compile option -march=native could not be working, so modify the add_compile_options in CMakeLists.txt to set the correct options for your platform.


## Frequency correction for rtl-sdr
In message header, the P field give an estimation of frequency drift in ppm :

   [#4 (F:136.975 P:+03.1) 28/09/2018 09:36:17.739 --------------------------------

This drift could come from transmitter, doppler effect and receiver.
To try to estimate and compensate receiver frequency drift, first run vdlm2dec for a while logging the received messages :

> ./vdlm2dec -l logmessage -r 0 136.725 136.775 136.875 136.975 

then extract frequency drifts :

> grep " P:" logmessage | cut -c 18-22 > ppmlog

from ppmlog estimate an average, and use the opposite as -p correction parameter. Ie: if the average is +5.2 use :

> ./vdlm2dec -p -5 -r 0 136.725 136.775 136.875 136.975 

# Acarsserv

acarsserv is a companion program for vdlm2dec. It listens to acars messages on UDP coming from one or more vdlm2dec or [acarsdec](https://github.com/TLeconte/acarsdec) processes and stores them in a sqlite database.

See : [acarsserv](https://github.com/TLeconte/acarsserv)

Note : vdlm2dec could only send packet over the network on json format. It does not support the acarsdec old binary format.

## Copyrights 
vdlm2dec and acarsserv are Copyright Thierry Leconte 2015-2018

These code are free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License version 2
published by the Free Software Foundation.

They include [cJSON](https://github.com/DaveGamble/cJSON) Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

