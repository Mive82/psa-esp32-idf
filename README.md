# ESP32 Firmware for the Peugeot Infotainment Project

This is a rewrite of the firmware in esp-idf. Currently it has the exact same features (or lack thereof) as the arduino
version. It's compatible with the new v1.0 revision of the PCB.

This firmware aims to be a bare-bones Type-A display emulator that works with the factory RD3 head-unit.

The main purpose for this is to replace the existing display with a custom Raspberry Pi implementation.

My car doesn't have automatic AC, satnav or the CD changer, so those features will not be implemented.

## Credits

This uses code for the `TSS463C` and `VAN_RMT_RX` from [Peter Pinter](https://github.com/morcibacsi), but heavily modified
to suit my needs.
