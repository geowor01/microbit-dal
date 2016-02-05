/**
  * Class definition for the custom MicroBit Temperature Service.
  * Provides a BLE service to remotely read the state of the temperature, and configure its behaviour.
  */

#include "MicroBit.h"
#include "ble/UUID.h"

#include "MicroBitEddyStoneService.h"

/**
  * Constructor.
  * Create a representation of the TemperatureService
  * @param _ble The instance of a BLE device that we're running on.
  */
MicroBitEddyStoneService::MicroBitEddyStoneService(BLEDevice &_ble, ManagedString url, ManagedString namespaceID, ManagedString instanceID) :
        ble(_ble),
        uidFrame(namespaceID, instanceID),
        urlFrame(url, uidFrame.getEncodedUID()),
        tlmFrame(uidFrame.getEncodedUID())
{
    uBit.serial.printf("%s %s %s\r\n",url.toCharArray(),namespaceID.toCharArray(), instanceID.toCharArray());

    this->namespaceID = namespaceID;
    this->instance = instanceID;
    currentFrame = 0;

    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_NON_CONNECTABLE_UNDIRECTED);
    ble.gap().setAdvertisingInterval(100);

    updateAdvertisementPacket();


    /* Start advertising */
    ble.gap().startAdvertising();
}

void MicroBitEddyStoneService::updateAdvertisementPacket()
{
    currentFrame = (currentFrame + 1) % EDDYSTONE_NUM_EDDYSTONE_FRAMES;

    uint8_t* data;
    int len;

    switch(currentFrame)
    {
        case 0:
            len = uidFrame.length();
            data = (uint8_t*) malloc(len);
            uidFrame.getFrame(data);
            break;

        case 1:
            len = urlFrame.length();
            data = (uint8_t*) malloc(len);
            urlFrame.getFrame(data);
            break;

        case 2:
            len = tlmFrame.length();
            data = (uint8_t*) malloc(len);
            tlmFrame.getFrame(data);
            break;
    }

    uint16_t encodedUID = uidFrame.getEncodedUID();

    uint8_t uid[2] =
    {
        (uint8_t)(encodedUID & 0x0F),
        (uint8_t)(encodedUID >> 8)
    };

#if CONFIG_ENABLED(MICROBIT_DBG)

    uBit.serial.printf("frame: %d\r\n",currentFrame);

    uBit.serial.printf("raw: ");
    for(int i = 0; i < len; i++)
        uBit.serial.printf("%d", data[i]);
    uBit.serial.printf("\r\n");

    uBit.serial.printf("uid: ");

    for(int i = 0; i < 2; i++)
        uBit.serial.printf("%d", uid[i]);

    uBit.serial.printf("\r\n");
#endif



    ble.gap().clearAdvertisingPayload();
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, uid, sizeof(uid));
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::SERVICE_DATA, data, len);

    free(data);
}
