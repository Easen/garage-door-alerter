#include "BLEDeviceScanner.h"

void BLEDeviceScanner::setup(int scanDuration, int maxRSSI)
{
    this->scanDuration = scanDuration;
    this->maxRSSI = maxRSSI;

    BLEDevice::init("");
    this->pBLEScan = BLEDevice::getScan(); // create new scan
    this->pBLEScan->setActiveScan(true);   // active scan uses more power, but get results faster
    this->pBLEScan->setInterval(100);
    this->pBLEScan->setWindow(99); // less or equal setInterval value
}

bool BLEDeviceScanner::isBLEDeviceNearby(std::vector<String> bleDeviceNames)
{
    bool found = false;
    BLEScanResults foundDevices = this->pBLEScan->start(this->scanDuration, false);

    for (int i = 0; i < foundDevices.getCount(); i++)
    {
        auto device = foundDevices.getDevice(i);
        if (this->maxRSSI > device.getRSSI())
        {
            continue;
        }
        for (String bleDeviceName : bleDeviceNames)
        {
            if (device.getName() == bleDeviceName.c_str())
            {
                found = true;
                break;
            }
        }
    }
    pBLEScan->clearResults();
    return found;
}