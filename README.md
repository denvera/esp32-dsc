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
- [x] Connect to DSC server (TCP)
- [x] MQTT support for reading and writing
- [ ] Configuration via either Espressif Smartconfig, BT or AP captive portal


#### Compilation / SDK notes
It appears a change was made recently to how select works ([3c457afca5561a04a84ced04b94f947801fe8bf0](https://github.com/espressif/esp-idf/commit/3c457afca5561a04a84ced04b94f947801fe8bf0)), so components might need their `select` calls changed to `lwip_select`'s. Ths change to ESP-IDF seems to ensure that only one task can call `select` at a time which can lead to strange problems and deadlocks when using multiple components or tasks that try to `select` simultaneously.
