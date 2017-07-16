#include <IRsend.h>

const uint16_t khz = 38;

IRsend irsend(4);

char temperatures[16][4] = {
  "F00",
  "708",
  "B04",
  "30C",
  "D02",
  "50A",
  "906",
  "10E",
  "E01",
  "609",
  "A05",
  "20D",
  "C03",
  "40B",
  "807",
  "00F"
};

#define MODE_COOL 'B'
#define MODE_HEAT 'E'
#define MODE_AUTO 'F'

#define CMMD_UP_TEMP "BF40"
#define CMMD_CHANGE_MODE "7F80"
#define CMMD_CHANGE_STATE "FF00"

#define TURN_ON "A956"
#define TURN_OFF "E916"

#define SPEED_ONE "A956"
#define SPEED_TWO "AD52"

int lastTemperature = 25;
char* lastSpeed = SPEED_ONE;
char lastMode = MODE_COOL;

struct List {
  uint16_t data[197];
  uint16_t counter = 0;
};

void executeCommand(char* commandType);

struct KomecoController {
  void addToList(List& data, unsigned int value) {
    data.data[data.counter++] = value;
  }

  byte hexToByte(char hex) {
    if (hex >= '0' && hex <= '9') {
      return hex - '0';
    }
    return hex - 'A' + 10;
  }

  unsigned int highEndRawData[2] = {500, 1570};

  void byteToRawData(byte bytee, List& data) {
    for (int i = 3; i >= 0; --i) {
      addToList(data, highEndRawData[0]);
      addToList(data, highEndRawData[bitRead(bytee, i)]);
    }
  }

  void addBytesToData(const char* bytes, size_t count, List& data) {
    for (int i = 0; i < count; ++i) {
      byteToRawData(hexToByte(bytes[i]), data);
    }
  }

  void addHeaderToData(List& data) {
    addToList(data, 6234);
    addToList(data, 7302);

    addBytesToData("FF00FF00", 8, data);
  }

  void addCommandToData(char* command, List& data) {
    addBytesToData(command, 4, data);
  }

  void addParameterToData(const char* parameter, List& data) {
    addBytesToData(parameter, 4, data);
  }

  void addTemperatureToData(int temp, char mode, List& data) {
    unsigned int realTempIndex = temp - 16;
    char* temperature = temperatures[realTempIndex];

    temperature[1] = mode;
    addBytesToData(temperature, 3, data);
  }

  void addModeToData(char* mode, List& data) {
    addBytesToData(mode, 1, data);
  }

  void addChecksumToData(char* checksum, List& data) {
    addBytesToData(checksum, 2, data);
  }

  void addFooterToData(List& data) {
    addBytesToData("4AB", 3, data);

    addToList(data, 608);
    addToList(data, 7372);
    addToList(data, 616);
  }

  void turnOff() {
    List data;
    addHeaderToData(data);
    addCommandToData(CMMD_CHANGE_STATE, data);

    addParameterToData(TURN_OFF, data);
    addTemperatureToData(lastTemperature, lastMode, data);
    addChecksumToData("45", data);
    addFooterToData(data);

    irsend.sendRaw(data.data, data.counter, khz);
  }

  void setTemperatureTo(int temperature) {
    lastTemperature = temperature;
    executeCommand(CMMD_UP_TEMP);
  }

  void setModeToCool() {
    lastMode = MODE_COOL;
    executeCommand(CMMD_CHANGE_MODE);
  }

  void setModeToHeat() {
    lastMode = MODE_HEAT;
    executeCommand(CMMD_CHANGE_MODE);
  }

  void setModeToAuto() {
    lastMode = MODE_AUTO;
    executeCommand(CMMD_CHANGE_MODE);
  }

  void setSpeedTo(int speed) {
    if (speed == 1) {
      lastSpeed = SPEED_ONE;
    } else if (speed == 2) {
      lastSpeed = SPEED_TWO;
    }
    executeCommand(CMMD_CHANGE_MODE);
  }

  void executeCommand(char* commandType) {
    List data;
    addHeaderToData(data);
    addCommandToData(commandType, data);

    addParameterToData(lastSpeed, data);
    addTemperatureToData(lastTemperature, lastMode, data);
    addChecksumToData("45", data);
    addFooterToData(data);

    irsend.sendRaw(data.data, data.counter, khz);
  }

  void setup() {
    irsend.begin();
  }
};
