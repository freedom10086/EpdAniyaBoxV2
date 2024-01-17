#ifndef H_MY_BLE_UUID_
#define H_MY_BLE_UUID_


#define BLE_SVC_BATTERY 0x180f
// read notify
#define BLE_UUID_CHAR_BATTERY_LEVEL 0x2A19


#define BLE_SVC_CSC_UUID 0x1816
// noytify
#define BLE_UUID_CHAR_CSC_MSRMT 0x2A5B
// read
#define BLE_UUID_CHAR_CSC_FEATURE 0x2A5C
// read
#define BLE_UUID_CHAR_SENSOR_LOCATION 0x2A5D
// write indicate
#define BLE_UUID_CHAR_SC_CONTROL_POINT 0x2A55


#define BLE_SVC_DEVICE_INFORMATION_UUID 0x180A
//all read
#define BLE_UUID_CHAR_Manufacturer_Name 0x2A29
#define BLE_UUID_CHAR_Model_Number 0x2A24
#define BLE_UUID_CHAR_Serial_Number 0x2A25
#define BLE_UUID_CHAR_Hardware_Revision 0x2A27
#define BLE_UUID_CHAR_Firmware_Revision 0x2A26
#define BLE_UUID_CHAR_Software_Revision 0x2A28
#define BLE_UUID_CHAR_System_ID 0x2A23
#define BLE_UUID_CHAR_PnP_ID  0x2A50


#define BLE_SVC_HRM_UUID 0x180D
// Notify
#define BLE_UUID_CHAR_Heart_Rate_Measurement 0x2A37
#define BLE_UUID_CHAR_Body_Sensor_Location 0x2A38

#define BLE_SVC_ENVIRONMENTAL_SENSING_UUID 0x181A
// all read and notify
#define BLE_UUID_CHAR_TEMPERATURE 0x2A6E
#define BLE_UUID_CHAR_HUMIDITY 0x2A6F

#define BLE_UUID_PRIMARY_SERVICE 0x2800
#define BLE_UUID_SECONDARY_SERVICE 0x2801

// 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
static const ble_uuid128_t BLE_SVC_UART_UUID =
        BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E);
static const ble_uuid128_t BLE_UUID_CHAR_UART_TX =
        BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E);
static const ble_uuid128_t BLE_UUID_CHAR_UART_RX =
        BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E);


#define BLE_UUID_END 0x0000

#endif