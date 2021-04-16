#define OK F("ok")
#define ERROR F("err")

unsigned long readSerialAscii();
unsigned long readSerialBinary();
void SerialOK();
void SerialError();
void handleAPI();