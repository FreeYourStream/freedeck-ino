#define OK F("ok")
#define ERROR F("err")

void _dumpConfigFileOverSerial();
void _renameTempFileToConfigFile(char const *path);
void _openTempFile();
long _getSerialFileSize();
void _saveNewConfigFileFromSerial();
void handleAPI();
void handleSerial();
unsigned long int readSerialAscii();
unsigned long int readSerialBinary();