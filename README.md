# AskSin++ HM-ES-TX-WM SML

This repository contains the source code for the AskSin++ HomeMatic HM-ES-TX-WM ES-IEC replica with **SML protocol support**.  
A version with support for ES-Fer, ES-LED and ES-Gas sensors can be found at [pa-pa](https://github.com/pa-pa/AskSinPP/tree/master/examples/HM-ES-TX-WM).

**Contents:**

- [Compilation](#compilation)
- [Flashing](#flashing)
- [Features](#features)
- [Requirements](#requirements)
- [Limitations](#limitations)
- [Pin Configuration](#pin-configuration)
- [Debug Output](#debug-output)
- [Supported Hardware](#supported-hardware)
- [Sample Board](#sample-board)
- [SML Specification](#sml-specification)
- [License](#license)

## Compilation

```bash
pio run
```

## Flashing

USB-TTL adapter required.

```bash
pio run --target upload
````

## Features

- OBIS code 1-0:1.8.0 - Positive active energy (A+) total \[kWh\]
- OBIS code 1-0:16.7.0 - Sum active instantaneous power (A+ - A-) \[kW\]

## Requirements

- Unlocked smart meter with SML protocol support.

## Limitations

The library does not parse the whole SML document, but searches for the OBIS codes 1-0:1.8.0 and 1-0:16.7.0 by pattern matching. The SML subtree of the pattern matched OBIS code is then parsed according to the SML specification.

This means that the implementation already contains methods for parsing SML trees and thus the entire SML document can be parsed with little additional effort if required.

Device settings set via a CCU are ignored.

The sensor was **not** designed for battery operation.

## Pin Configuration

- LED: Pin 4
- Config Button: Pin 5
- IR RX: Pin 7

## Debug Output

The debug output is written to the default TX pin of AskSin++.

## Supported Hardware

- ATmega328P
- Serial/TTL IR reader

## Sample Board

[HB-UNI-Mini-X](https://github.com/wolwin/WW-myPCB/tree/master/PCB_HB-UNI-Mini-X) with [Hichi IR v1.1](https://www.photovoltaikforum.com/thread/141332-neue-lesekopf-baus%C3%A4tze-ohne-smd-l%C3%B6ten/) IR-Reader.

## SML Specification

[www.bsi.bund.de](https://www.bsi.bund.de/SharedDocs/Downloads/DE/BSI/Publikationen/TechnischeRichtlinien/TR03109/TR-03109-1_Anlage_Feinspezifikation_Drahtgebundene_LMN-Schnittstelle_Teilb.pdf?__blob=publicationFile)

## License

This work is licensed under a [Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License](https://creativecommons.org/licenses/by-nc-sa/3.0/).
