
# RasterToTPCL CUPS Raster Driver

## Introduction

A driver to Toshiba TEC Label printers supporting the TEC Printer Command Language or TPCL,
version 2.

Converts CUPS Raster graphics along with a supported PPD file into a TPCL graphic ready to
be printed directly. As of yet, there is no support for sending text to the driver.

Conversion includes support for the TPCL TOPIX compression algorithm for reliable and fast
delivery of print jobs to the printer. Raw 8-bit graphics direct from the raster driver
are also supported but not recommended.

This document and source for the driver can be found at:

http://github.com/samlown/rastertotpcl

Any issues or code you'd like to contribute can be performed there.

## License

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.


## History

### 2010-07-10 - PPD Fixes

PPDC compilation errors discovered in Ubuntu 10.10 and resolution setting
was being ignored.


### 2010-05-27 - Initial release
 
Converted original restertotec file into rastertotpcl.
Added support for TOPIX compression.
Now using CUPS raster header 2 for finer control of page sizes.
Refactoring.

## Supported Printers

Support for the following printers is included by the PPD files:

 * Toshiba TEC B-SA4G/T     (tecbas4.ppd)
 * Toshiba TEC B-SX4        (tecbsx4.ppd)
 * Toshiba TEC B-SX5        (tecbsx5.ppd)
 * Toshiba TEC B-SX6        (tecbsx6.ppd)
 * Toshiba TEC B-SX8        (tecbsx8.ppd)
 * Toshiba TEC B-852R       (tecb852r.ppd)
 * Toshiba TEC B-SV4D       (tecbsv4d.ppd)
 * Toshiba TEC B-SV4T       (tecbsv4t.ppd)
 * Toshiba TEC B-EV4D-GS14  (tecbev4d.ppd)
 * Toshiba TEC B-EV4T-GS14  (tecbev4t.ppd)

There is little variation between these printers other than resolutions, speeds, and accepted media types,
so new printer models can be tested or added easily.

As of May 2010, thorough testing has only been performed on the B-SX4 model. Please get in touch if you test
the drivers on other printers with success.


## Instalation

The CUPS image development headers are required before compilation. In Ubuntu, these can be installed with:

    sudo apt-get install libcupsimage2-dev

The easiest way to install from source is to run the following from the base directory:

    make
    sudo make install

This will install the filter and PPD files in the standard CUPS filter and PPD directories
and show them in the CUPS printer selection screens.


## TODO

* Add support for RFID.

## Authors

rastertotpcl is based on the rastertotec driver written by Patick Kong (SKE s.a.r.l).
rastertotec is based on the rastertolabel driver included with the CUPS printing system by Easy Software Products.
Packaing of rastertotpcl and TOPIX compression was added by Sam Lown (www.samlown.com).

