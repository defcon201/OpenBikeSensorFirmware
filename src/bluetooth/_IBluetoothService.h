#ifndef OBS_BLUETOOTH_IBLUETOOTHSERVICE_H
#define OBS_BLUETOOTH_IBLUETOOTHSERVICE_H

#include <Arduino.h>
#include <BLEService.h>
#include <list>
#include "globals.h"

/**
 * This class interface defines how a bluetooth service should work.
 * Any child class should override every public method.
 */
class IBluetoothService {
  public:
    /**
     * This method initializes the class and expects the instance to create its
     * own BLEService and its characteristics.
     * @param pServer pointer to the bluetooth server
     */
    virtual void setup(BLEServer *pServer) = 0;

    /**
     * Whether this bluetooth service should be advertised or not.
     */
    virtual bool shouldAdvertise() = 0;

    /**
     * Gets the only BLEService that was created in setup().
     */
    virtual BLEService* getService() = 0;

    /**
     * Processes new sensor values from both sides.
     * @param leftValue sensor value of the left side (MAX_SENSOR_VALUE for no reading)
     * @param rightValues sensor value of the right side (MAX_SENSOR_VALUE for no reading)
     */
    virtual void newSensorValues(uint16_t leftValue, uint16_t rightValue) = 0;

    /**
     * Processes the event that the push button was just triggered.
     */
    virtual void buttonPressed() = 0;

  protected:
    /**
     * Joins a list of integers by placing the `glue` string between the members.
     */
    static String joinList(const std::list<uint16_t>& list, const String& glue) {
      String s;

      boolean firstValue = true;
      for (auto value : list) {
        if (!firstValue) {
          s += glue;
        }
        s += String(value);
        firstValue = false;
      }

      return s;
    }

    static String valueAsString(uint16_t value) {
      return String(value);
    }
};

#endif