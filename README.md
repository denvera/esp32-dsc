# ESP32 based DSC interface module
### Overview
ESP32 project to read/write DSC KeyBus using GPIO pins.

### Console commands
Enter any of the following at the `esp32-dsc>` prompt (supports tab completion):

- ```mon``` - Enable "monitor" mode, print keybus and peripheral messages to console_task
- ```write <0xHH or DD> ...``` - write 0xHH hex or DDD decimal to keybus. Eg: ```write 0x1 0x2 0x3``` or ```write 100 20 300```
- ```reset``` - reboot ESP32
- ```version``` - Show firmware version

#### Status suffixes for messages
- ``[OK]``  - Message CRC valid for Panel messages (or no checksum), Keypress checksum valid for Peripheral messages.
- ``[||]``  - Message not checksummed for peripheral messages
- ``[BAD]`` - Checksum failed (also possibly a non-CRC message which would always give bad CRC)

#### To do:
- [x] Interpret KeyBus Protocol
- [x] Write to KeyBus
- [x] Sniff peripheral messages
- [x] Basic serial console
- [x] Debug print to Serial
- [ ] Connect to DSC server
- [ ] Configuration via either Espressif Smartconfig, BT or AP captive portal
