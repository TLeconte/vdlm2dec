# VDLM2DEC
vdlm2dec is a vdl mode 2 decoder with built-in rtl_sdr or airspy front end.

It could decode up to 8 frequencies simultaneously ( but in the same 2Mhz range for rtl_sdr )

It decodes ARINC-622 ATS applications (ADS-C, CPDLC) via [libacars](https://github.com/szpajder/libacars) library

## Usage

For RTL-SDR:

> vdlm2dec  [-l logfile]  [-g gain] [-i stid] [-v] [-q] [-J] [-l logfile] -r rtldevicenumber  Frequencies(Mhz)

For Airspy R2 / Mini:

> vdlm2dec  [-l logfile]  [-g gain] [-i stid] [-v] [-q] [-J] [-l logfile] [-k airspy_serial ] Frequencies(Mhz)

 -p ppm :		set rtl sdr ppm frequency (rtl-sdr only)

 -i stid :		local receiver station id

 -b filter:  filter output by label (ex: -b "H1:Q0" : only output messages  with label H1 or Q0"

 -v :			verbose output

 -q :			quiet output

 -J :			json output

 -R :			Flights & Aircrafts registration json format output

 -a :			Aircraft registration csv format output

 -l logfile :		output log (default : stdout)

 -j addr:port		send to addr:port UDP packets in json that could be read by acarsserv or other online aggregator 

 -s addr:port :		send received position in sbs format output to addr:port

 -G :			output messages from ground station

 -E :			output empty messages (not really useful)

 -U :			output undecoded messages  (not really useful)

for the RTLSDR device

 -r rtldevicenumber :	decode from rtl dongle number rtldevicenumber or s/n
 
 -g gain :		set preamp gain in tenth of db (ie -g 90 for +9db)

for the AirSpy device

 -g gain :		set linearity gain (0 to 21).By default use maximum gain (airspy)

 -k airspy_serial_number :  decode from airspy device with supplied serial number specified in hex, ie 0xA74068C82F591693

 
## Examples
#### For rtl-sdr :
> ./vdlm2dec -r 0 136.725 136.775 136.875 136.975 

#### For airspy :
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
    
#### JSON out :
> ./vdlm2dec -J -r 0 136.725 136.775 136.875 136.975

    {"timestamp":1675586802.2075,"station_id":"TL-LFRN-VDL2","freq":136.775,"hex":"02007D","icao":131197,"toaddr":2178181,"app":    {"name":"vdlm2dec","ver":"2.1"},"mode":"2","label":"H1","block_id":"2","ack":"!","tail":"CN-ROR","flight":"ATRAM6","msgno":"D65B","text":"#DFB0400000627932100\r\nD2010000031421170\r\nF1+0304+99+6702981040+034+366+00XXXX+02-00\r\nF2+0305+99+6702981036+034+369-00XXXX+02-00\r\nG1-3920308781084308430887\r\nG2-3920308781084308430888\r\nK100111011111X110101100\r\nK2","end":true}
    {"timestamp":1675586807.2109711,"station_id":"TL-LFRN-VDL2","freq":136.725,"hex":"407795","icao":4224917,"toaddr":1153178,"app":{"name":"vdlm2dec","ver":"2.1"},"dsta":"GCRR","lat":47.4,"lon":-3.8,"epu":6,"alt":36000}
    {"timestamp":1675586807.290592,"station_id":"TL-LFRN-VDL2","freq":136.775,"hex":"02007D","icao":131197,"toaddr":2178181,"app":       {"name":"vdlm2dec","ver":"2.1"},"mode":"2","label":"H1","block_id":"3","ack":"!","tail":"CN-ROR","flight":"ATRAM6","msgno":"D65C","text":"#DFB00111011111-110101100\r\nL1018CE000000000939A8501000100007F0920000040847\r\nL2018CE000000000939B0501000200007F092000004084A\r\n"}


#### Flights & Aircraft registration JSON output :
> ./vdlm2dec -R -r 0 136.725 136.775 136.875 136.975

    {"timestamp":1546187157.8686321,"icao":"3C656D","tail":"D-AIKM"}
    {"timestamp":1546187160.7268431,"icao":"020095","tail":"CN-ROU"}
    {"timestamp":1546187164.189714,"flight":"BA092F","depa":"EGKK","dsta":"LPFR","icao":"406B84","tail":"G-GATS"}
    {"timestamp":1546187173.913542,"icao":"440395","tail":"OE-IZO"}
    {"timestamp":1546187215.3877859,"flight":"CJ8483","depa":"EGLC","dsta":"LEPA"}

#### Aircraft registration csv output:
> ./vdlm2dec -a -r 0 136.725 136.775 136.875 136.975
    
    4D201F,9H-AEI
    4CA2C9,EI-DEP
    49514B,CS-TJK
    400D8B,G-EZAA

#### Send to a remote aggregator:
> ./vdlm2dec -q -i XX-YYYYZ -j feed.acars.io:5555 -r 0 136.725 136.775 136.825 136.875 136.975

#### Send positions in sbs format to a local readsb:
> ./vdlm2dec -q -s 127.0.0.1:37000 -r 0 136.725 136.775 136.825 136.875 136.975

Notes : 
 * Add --net-sbs-jaero-in-port=37000 to readsb command line to receive sbs packets
 * You could use -s and -j together :
 > ./vdlm2dec -q -i XX-YYYYZ -s 127.0.0.1:37000 -j feed.acars.io:5555 -r 0 136.725 136.775 136.825 136.875 136.975

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

#### For raspberry Pi and others ARM machines :

The gcc compile option -march=native could not be working, so modify the add_compile_options in CMakeLists.txt to set the correct options for your platform.

## Copyrights 
vdlm2dec and acarsserv are Copyright Thierry Leconte 2015-2023

These code are free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License version 2
published by the Free Software Foundation.

They include [cJSON](https://github.com/DaveGamble/cJSON) Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

