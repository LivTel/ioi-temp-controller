ioi-temp-controller
=============

# Overview

![alt text](https://raw.githubusercontent.com/LivTel/ioi-temp-controller/master/img/ioi_panel2.jpg "In situ.")

This package contains the electronic schematics and arduino code to drive a 
10 microamp excitation current required by the DT-400 series temperature 
sensor diodes inside IO:I's cryostat. This facility was originally provided 
by the Lakeshore 331 temperature controller. 

# Commands

The mac, ip, gateway and subnet addresses need to be configured first.

On upload to an ethernet arduino, a socket server is instantiated to serve a limited command set previously 
used by IO:I's ICS. The four commands that are currently available are:

| Command        | Description                        |
| :------------- | :----------------                  | 
| KRDG?A         | Read temperature from sensor A (K) |
| KRDG?B         | Read temperature from sensor B (K) |
| VRDG?A         | Read voltage from sensor A (V)     |
| VRDG?B         | Read voltage from sensor B (V)     |

The socket server is blocking, and must not be held open for prolonged periods by a single machine.

## Schematic

The schematic is as follows.

![alt text](https://raw.githubusercontent.com/LivTel/ioi-temp-controller/master/sch/current_source.png "Schematic")
 
The LM134 IC provides a temperature dependent current source. The magnitude of this 
current source is modulated by the two digital pots R1 & R2 (MCP41100). The 
temperature dependence is largely removed by diode D1 (1N4933). 

Note that the ETHCS pin is not used for clock selects!

# I/O Configuration

Definitions for I/O should not need to be changed if the wiring follows the schematic above *and* the 
temperature sensor wiring follows:

| Pin            | Description       |
| :------------- | :---------------- | 
| 2              | T2 V-             |
| 3              | T2 V+             |
| 4              | T1 V-             |
| 5              | T1 V+             |

# Current Magnitude and Temperature Calibration Coefficients

The set current [**SET\_CURRENT**] must be set so that it lies within the capability defined by the relative 
resistances of the digital pots. See p8 of the LM234 component manual in `man/` for a description of how the 
current magnitude is defined using the two pots.

The temperature sensor calibration is defined by the coefficients [**DIODE\_TEMPERATURE\_COEFFS**] and 
a corresponding reference voltage [**REFERENCE\_VOLTAGE**]. This has been set (as of 19/10/15) for 
a DT471 series diode.

# Known Issues

A 2.5V reference voltage is fed to the reference pin to increase the accuracy of the A2D conversion whilst still
remaining in range of the full output voltage of the diode at RT down to 80K. Currently, with the 10bit conversion 
and a diode temperature coefficient of -2.5mV/degC, the system is only accurate to [2.5/(2^10)]/(2.5*10^-3) 
degC == 1 degC. This could be improved by either i) addition of a more accurate A2D circuit, or ii) reducing the range 
over which the sensor can be read.

