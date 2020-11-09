/*
The MIT License (MIT)

Copyright (c) 2016 British Broadcasting Corporation.
This software is provided by Lancaster University by arrangement with the BBC.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

/**
  * Class definition for MicroBitLightSensor.
  *
  * This is an object that interleaves light sensing with MicroBitDisplay.
  */

#include "MicroBitConfig.h"
#include "MicroBitLightSensor.h"
#include "MicroBitDisplay.h"

/**
  * After the startSensing method has been called, this method will be called
  * MICROBIT_LIGHT_SENSOR_AN_SET_TIME after.
  *
  * It will then read from the currently selected channel using the AnalogIn
  * that was configured in the startSensing method.
  */
void MicroBitLightSensor::analogReady()
{
    this->results[chan] = this->sensePin->read_u16();

    analogDisable();

    DigitalOut((PinName)(matrixMap.columnStart + chan)).write(1);

    chan++;

    chan = chan % MICROBIT_LIGHT_SENSOR_CHAN_NUM;

    update_averages();
}

/**
  * Forcibly disables the AnalogIn, otherwise it will remain in possession
  * of the GPIO channel it is using, meaning that the display will not be
  * able to use a channel (COL).
  *
  * This is required as per PAN 3, details of which can be found here:
  *
  * https://www.nordicsemi.com/eng/nordic/download_resource/24634/5/88440387
  */
void MicroBitLightSensor::analogDisable()
{
    NRF_ADC->ENABLE = ADC_ENABLE_ENABLE_Disabled;

    NRF_ADC->CONFIG = (ADC_CONFIG_RES_8bit << ADC_CONFIG_RES_Pos) |
                      (ADC_CONFIG_INPSEL_SupplyTwoThirdsPrescaling << ADC_CONFIG_INPSEL_Pos) |
                      (ADC_CONFIG_REFSEL_VBG                       << ADC_CONFIG_REFSEL_Pos) |
                      (ADC_CONFIG_PSEL_Disabled                    << ADC_CONFIG_PSEL_Pos) |
                      (ADC_CONFIG_EXTREFSEL_None                   << ADC_CONFIG_EXTREFSEL_Pos);
}

/**
  * Constructor.
  *
  * Create a representation of the light sensor.
  *
  * @param map The mapping information that relates pin inputs/outputs to physical screen coordinates.
  *            Defaults to microbitMatrixMap, defined in MicroBitMatrixMaps.h.
  */
MicroBitLightSensor::MicroBitLightSensor(const MatrixMap &map) :
    analogTrigger(),
    matrixMap(map)
{
    this->chan = 0;

    valid_average = -1;

    for(int i = 0; i < MICROBIT_LIGHT_SENSOR_CHAN_NUM; i++)
        results[i] = -1;

    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(MICROBIT_ID_DISPLAY, MICROBIT_DISPLAY_EVT_LIGHT_SENSE, this, &MicroBitLightSensor::startSensing, MESSAGE_BUS_LISTENER_IMMEDIATE);

    this->sensePin = NULL;
}

/**
  * This method updates the average of the three results, as well as the valid_average
  * if results are valid. The MICROBIT_DISPLAY_EVT_LIGHT_SENSE_READY event is sent if
  * the valid_average is updated.
  *
  * @return returns a boolean representing whether or not the results are valid.
  */
bool MicroBitLightSensor::update_averages()
{
    bool valid = true;
    int sum = 0;
    for(int i = 0; i < MICROBIT_LIGHT_SENSOR_CHAN_NUM; i++) {
        if (this->results[i] < 0 || this->results[i] > 450) {
            valid = false;
        }
        // Use zero if a result hasn't been used rather than -1.
        sum += max(this->results[i], 0);
    }
    this->average = sum / MICROBIT_LIGHT_SENSOR_CHAN_NUM;
    if (valid) {
        this->valid_average = this->average;
        MicroBitEvent(MICROBIT_ID_LIGHT_SENSOR, MICROBIT_DISPLAY_EVT_LIGHT_SENSE_READY);
    }
    return valid;
}

/**
  * This method returns a summed average of the three sections of the display.
  *
  * A section is defined as:
  *  ___________________
  * | 1 |   | 2 |   | 3 |
  * |___|___|___|___|___|
  * |   |   |   |   |   |
  * |___|___|___|___|___|
  * | 2 |   | 3 |   | 1 |
  * |___|___|___|___|___|
  * |   |   |   |   |   |
  * |___|___|___|___|___|
  * | 3 |   | 1 |   | 2 |
  * |___|___|___|___|___|
  *
  * Where each number represents a different section on the 5 x 5 matrix display.
  *
  * @param valid_only True to return -1 if full set of results not taken and to return a stale
  *            but valid reading in the case of invalid results. False to use all results.
  *            Defaults to microbitMatrixMap, defined in MicroBitMatrixMaps.h.
  *
  * @return returns a value in the range 0 - 255 where 0 is dark, and 255
  * is very bright
  */
int MicroBitLightSensor::read(bool valid_only)
{
    int average = valid_only ? this->valid_average : this->average;

    if (valid_only && average < 0)
        return -1;

    average = min(average, MICROBIT_LIGHT_SENSOR_MAX_VALUE);

    average = max(average, MICROBIT_LIGHT_SENSOR_MIN_VALUE);

    int inverted = (MICROBIT_LIGHT_SENSOR_MAX_VALUE - average) + MICROBIT_LIGHT_SENSOR_MIN_VALUE;

    int a = 0;

    int b = 255;

    int normalised = a + ((((inverted - MICROBIT_LIGHT_SENSOR_MIN_VALUE)) * (b - a))/ (MICROBIT_LIGHT_SENSOR_MAX_VALUE - MICROBIT_LIGHT_SENSOR_MIN_VALUE));

    return normalised;
}

/**
  * The method that is invoked by sending MICROBIT_DISPLAY_EVT_LIGHT_SENSE
  * using the id MICROBIT_ID_DISPLAY.
  *
  * @note this can be manually driven by calling this member function, with
  *       a MicroBitEvent using the CREATE_ONLY option of the MicroBitEvent
  *       constructor.
  */
void MicroBitLightSensor::startSensing(MicroBitEvent)
{
    for(int rowCount = 0; rowCount < matrixMap.rows; rowCount++)
        DigitalOut((PinName)(matrixMap.rowStart + rowCount)).write(0);

    PinName currentPin = (PinName)(matrixMap.columnStart + chan);

    DigitalOut(currentPin).write(1);

    DigitalIn(currentPin, PullNone).~DigitalIn();

    if(this->sensePin != NULL)
        delete this->sensePin;

    this->sensePin = new AnalogIn(currentPin);

    analogTrigger.attach_us(this, &MicroBitLightSensor::analogReady, MICROBIT_LIGHT_SENSOR_AN_SET_TIME);
}

/**
  * A destructor for MicroBitLightSensor.
  *
  * The destructor removes the listener, used by MicroBitLightSensor from the default EventModel.
  */
MicroBitLightSensor::~MicroBitLightSensor()
{
    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->ignore(MICROBIT_ID_DISPLAY, MICROBIT_DISPLAY_EVT_LIGHT_SENSE, this, &MicroBitLightSensor::startSensing);
}
