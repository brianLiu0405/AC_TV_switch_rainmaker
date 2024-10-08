#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"
#include "AppInsights.h"
#include <IRremote.hpp>
#include "esp_gap_ble_api.h"

const int irSendPin = 20;
#define DEFAULT_AC_POWER_MODE true
#define DEFAULT_TV_POWER_MODE false
// const char *service_name = "MY_TV_AND_AC";
// const char *pop = "89459043";
const char *service_name = "PROV_2067";
const char *pop = "89459043";

// GPIO for push button
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
static int gpio_0 = 9;
static int gpio_switch = 7;
#else
// GPIO for virtual device
static int gpio_0 = 0;
static int gpio_switch_TV = 16;
static int gpio_1 = 1;
static int gpio_switch_AC = 17;
#endif

/* Variable for reading pin status*/
bool switch_state_TV = true;
bool switch_state_AC = true;

// The framework provides some standard device types like switch, lightbulb,
// fan, temperaturesensor.
static Switch *my_TV = NULL;
static Switch *my_AC = NULL;

// WARNING: sysProvEvent is called from a separate FreeRTOS task (thread)!
void sysProvEvent(arduino_event_t *sys_event) {
  switch (sys_event->event_id) {
    case ARDUINO_EVENT_PROV_START:
#if CONFIG_IDF_TARGET_ESP32S2
      Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n", service_name, pop);
      WiFiProv.printQR(service_name, pop, "softap");
#else
      Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
      WiFiProv.printQR(service_name, pop, "ble");
#endif
      break;
    case ARDUINO_EVENT_PROV_INIT:         WiFiProv.disableAutoStop(10000); break;
    case ARDUINO_EVENT_PROV_CRED_SUCCESS: WiFiProv.endProvision(); break;
    default:                              ;
  }
}

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx) {
  const char *device_name = device->getDeviceName();
  const char *param_name = param->getParamName();

  if (strcmp(param_name, "Power") == 0) {
      Serial.printf("Received value = %s for %s - %s\n", val.val.b ? "true" : "false", device_name, param_name);
      if(strcmp(device_name, "Switch_TV") == 0){
        switch_state_TV = val.val.b;
        (switch_state_TV == false) ? digitalWrite(gpio_switch_TV, LOW) : digitalWrite(gpio_switch_TV, HIGH);
        TV_1_0();
      }
      else if(strcmp(device_name, "Switch_AC") == 0){
        switch_state_AC = val.val.b;
        (switch_state_AC == false) ? digitalWrite(gpio_switch_AC, LOW) : digitalWrite(gpio_switch_AC, HIGH);
        AC_1_0();
      }
      param->updateAndReport(val);
  }
}

void setup() {
  Serial.begin(9600);
  // IrReceiver.begin(irReceiverPin);  
  IrSender.begin(irSendPin, 0, 48);
  pinMode(gpio_0, INPUT);
  pinMode(gpio_switch_TV, OUTPUT);
  pinMode(gpio_1, INPUT);
  pinMode(gpio_switch_AC, OUTPUT);
  digitalWrite(gpio_switch_TV, DEFAULT_TV_POWER_MODE);
  digitalWrite(gpio_switch_AC, DEFAULT_AC_POWER_MODE);

  Node my_node;
  my_node = RMaker.initNode("ESP RainMaker Node");

  // Initialize switch device
  my_TV = new Switch("Switch_TV", &gpio_switch_TV);
  my_AC = new Switch("Switch_AC", &gpio_switch_AC);
  if (!my_TV) {
    return;
  }
  if (!my_AC) {
    return;
  }
  // Standard switch device
  my_TV->addCb(write_callback);
  my_AC->addCb(write_callback);

  // Add switch device to the node
  my_node.addDevice(*my_TV);
  my_node.addDevice(*my_AC);

  esp_ble_gap_config_local_privacy(true);
  // This is optional
  RMaker.enableOTA(OTA_USING_TOPICS);
  // If you want to enable scheduling, set time zone for your region using
  // setTimeZone(). The list of available values are provided here
  // https://rainmaker.espressif.com/docs/time-service.html
  //  RMaker.setTimeZone("Asia/Shanghai");
  //  Alternatively, enable the Timezone service and let the phone apps set the
  //  appropriate timezone
  RMaker.enableTZService();

  RMaker.enableSchedule();

  RMaker.enableScenes();
  // Enable ESP Insights. Insteads of using the default http transport, this function will
  // reuse the existing MQTT connection of Rainmaker, thereby saving memory space.
  initAppInsights();

  RMaker.enableSystemService(SYSTEM_SERV_FLAGS_ALL, 2, 2, 2);

  RMaker.start();

  WiFi.onEvent(sysProvEvent);  // Will call sysProvEvent() from another thread.
#if CONFIG_IDF_TARGET_ESP32S2
  WiFiProv.beginProvision(NETWORK_PROV_SCHEME_SOFTAP, NETWORK_PROV_SCHEME_HANDLER_NONE, NETWORK_PROV_SECURITY_1, pop, service_name);
#else
  WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM, NETWORK_PROV_SECURITY_1, pop, service_name);
#endif
}

void loop() {
}


void TV_1_0(){
  IrSender.sendPulseDistanceWidth(38, 9050, 4450, 600, 1650, 600, 500, 0xA05FAC03, 32, PROTOCOL_IS_LSB_FIRST, 0, 0);

}

void AC_1_0(){
  uint64_t IORawData[]={0x27160F8AB2754D, 0x1808000000};
  IrSender.sendPulseDistanceWidthFromArray(38, 3500, 3350, 400, 1250, 400, 400, &IORawData[0], 104, PROTOCOL_IS_LSB_FIRST, 0, 0);
}

  // pinMode(gpio_switch_AC, OUTPUT);
  // digitalWrite(gpio_switch_AC, DEFAULT_POWER_MODE);
  // pinMode(gpio_swing_AC, OUTPUT);
  // digitalWrite(gpio_swing_AC, DEFAULT_SWING);
  // pinMode(gpio_tem_AC, OUTPUT);
  // analogWrite(gpio_tem_AC, DEFAULT_TEM);

  // /*AC*/
  // my_AC->addNameParam();
  // my_AC->addPowerParam(DEFAULT_POWER_MODE);
  // my_AC->assignPrimaryParam(my_AC->getParamByName(ESP_RMAKER_DEF_POWER_NAME));

  // Param swing("Swing", ESP_RMAKER_PARAM_TOGGLE, value(DEFAULT_SWING), PROP_FLAG_READ | PROP_FLAG_WRITE);
  // swing.addUIType(ESP_RMAKER_UI_TOGGLE);
  // my_AC->addParam(swing);

  // Param tem("Tem", ESP_RMAKER_PARAM_RANGE, value(DEFAULT_TEM), PROP_FLAG_READ | PROP_FLAG_WRITE);
  // tem.addUIType(ESP_RMAKER_UI_SLIDER);
  // tem.addBounds(value(20), value(30), value(1));
  // my_AC->addParam(tem);
  // /*AC*/


// void loop() {
// }


// void TV_1_0(){
//   IrSender.sendPulseDistanceWidth(38, 9050, 4450, 600, 1650, 600, 500, 0xA05FAC03, 32, PROTOCOL_IS_LSB_FIRST, 0, 0);

// }

// void AC_1_0(){
//   uint64_t IORawData[]={0x27160F8AB2754D, 0x1808000000};
//   IrSender.sendPulseDistanceWidthFromArray(38, 3500, 3350, 400, 1250, 400, 400, &IORawData[0], 104, PROTOCOL_IS_LSB_FIRST, 0, 0);
// }

// void AC_swing(){
//   uint64_t swingRawData[]={0x27160F8AB2754D, 0x1220000000};
//   IrSender.sendPulseDistanceWidthFromArray(38, 3450, 3350, 400, 1250, 400, 400, &swingRawData[0], 104, PROTOCOL_IS_LSB_FIRST, 0, 0);
// }

// void AC_tem(int tem){
//   /*
//   25. 0x25160F8AB2754D, 0xE00000000
//   26. 0x27160F8AB2754D, 0x1000000000
//   27. 0x29160F8AB2754D, 0x1200000000
//   28. 0x2B160F8AB2754D, 0x1400000000
//   29. 0x2D160F8AB2754D, 0x1600000000
//   */
//   uint64_t first = ((2 * tem) - 36) << 32;
//   uint64_t second = (((2 * tem) - 13) << 48) | 0x160F8AB2754D;
//   uint64_t temRawData[]={first, second};
//   IrSender.sendPulseDistanceWidthFromArray(38, 3500, 3350, 400, 1250, 400, 400, &temRawData[0], 104, PROTOCOL_IS_LSB_FIRST, 0, 0);
// }



