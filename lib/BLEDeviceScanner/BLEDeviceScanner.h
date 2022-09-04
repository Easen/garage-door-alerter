#ifndef BLEDeviceScanner_h
#define BLEDeviceScanner_h

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>

class BLEDeviceScanner
{
public:
    void setup(int scanDuration, int maxRSSI);
    bool isBLEDeviceNearby(std::vector<String> bleDeviceNames);

private:
    BLEScan *pBLEScan;
    int scanDuration;
    int maxRSSI;
};

#endif