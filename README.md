# Toshiba TEC TPCL CUPS Raster Driver - rastertotpcl

## Introduction

A driver to Toshiba TEC Label printers supporting the TEC Printer Command Language or TPCL,
version 2.

Converts CUPS Raster graphics along with a supported PPD file into a TPCL graphic ready to
be printed directly. As of yet, there is no support for sending text to the driver.

Conversion includes support for the TPCL TOPIX compression algorithm for reliable and fast
delivery of print jobs to the printer. Raw 8-bit graphics direct from the raster driver
are also supported but not recommended.

This repository is a fork with some minor improvements, so the driver will compile on recent
systems. It was tested on MacOS Big Sur and Debian Buster. The original source can be found
at [samlown/rastertotpcl](http://github.com/samlown/rastertotpcl).

Pull requests to this project are very welcome, as the original repository seems to be
no longer maintained.

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

Thorough testing has been performed on the B-SX4 model by the original authors. This fork was validated
against a B-EV4D model. Please get in touch if you test the drivers on other printers with success, or run
into problems.

## Installation
The CUPS development headers and ppd compiler are required before compilation. In Debian/Ubuntu, these can
be installed with:

```
sudo apt-get install build-essential libcups2-dev cups-ppdc
```

The easiest way to install from source is to run the following from the base directory:

```
make
sudo make install
```

This will install the filter and PPD files in the standard CUPS filter and PPD directories
and show them in the CUPS printer selection screens.

To remove driver and ll its components, run

```
sudo make uninstall
```

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

## Authors
rastertotpcl is based on the rastertotec driver written by Patick Kong (SKE s.a.r.l).
rastertotec is based on the rastertolabel driver included with the CUPS printing system by Easy Software Products.
Packaing of rastertotpcl and TOPIX compression was added by Sam Lown (www.samlown.com).
This fork is maintained by Mark Dornbach (yaourdt).
