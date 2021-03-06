#ifndef DeviceConfiguration_h
#define DeviceConfiguration_h

#include "Arduino.h"

#define DEVICE_CONF_ARRAY_LENGHT 70

class DeviceConfiguration
{
  public:
    DeviceConfiguration() {};
    int confirmation;
    char broker[DEVICE_CONF_ARRAY_LENGHT]; //= "casa-granzotto.ddns.net"; //MQTT broker
    char topic[DEVICE_CONF_ARRAY_LENGHT]; //= "home/bedroom/outlet/1"; //MQTT topic
    char mqttUser[DEVICE_CONF_ARRAY_LENGHT]; // = "osmc";
    char mqttPassword[DEVICE_CONF_ARRAY_LENGHT]; // = "password1234"; (actually not this one)
    char wifiSsid[DEVICE_CONF_ARRAY_LENGHT];
    char wifiPass[DEVICE_CONF_ARRAY_LENGHT];
};

#endif
