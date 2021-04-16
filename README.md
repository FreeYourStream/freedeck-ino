# freedeck-ino

## For the newest (and sometimes unstable) features [CLICK HERE](https://github.com/koriwi/freedeck-ino/tree/develop) </br>(Feedback greatly appreciated)

## [Discord Community](https://discord.gg/sEt2Rrd)

## The Arduino files for you arduino pro micro to build your own freedeck
## Libraries needed
### SdFat
Be careful to install 1.x and **not** 2.x version of sdfat. it's not compatible in our case and i did not update yet. (pull request appreciated ;))
### HID-Project
Install HID-Project in version 2.x
## Serial API

### Let's you speak to the FreeDeck to automate things

All commands and parameters have to be followed by a newline (`\n`/`\0xa`/`\10`). Carriage returns are ignored (`\r`/`\0xd`/`\13`)

E.g. changing the displayed page to page 5 (linux):

```sh
$ echo -ne '\x3\n\x31\n5\n' > /dev/ttyACM0
```

| Binary(LE) |    Command     |                                                             Description |
| :--------: | :------------: | ----------------------------------------------------------------------: |
|  0x3 (3)   |     Begin      |                                        Begin of Serial API Transmission |
| 0x10 (16)  | Get FW_Version |                                            Returns the firmware version |
| 0x20 (32)  |   Get Config   |                                                        Dumps the config |
| 0x21 (33)  |  Write Config  | Expects filesize as parameter in ascii followed by the config in binary |
| 0x30 (48)  |    Get Page    |                          Return the currently displayed page (in ascii) |
| 0x31 (49)  |  Change page   |                         Expects the targeted page as parameter in ascii |
=======

## BIG thank you to [bitbank2 and his oled_turbo](https://github.com/bitbank2/oled_turbo)
