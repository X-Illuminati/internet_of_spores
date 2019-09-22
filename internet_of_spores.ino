#include "project_config.h"

#include <Wire.h>
#include <ESP8266WiFi.h>
#ifdef IOTAPPSTORY
#include <IOTAppStory.h>
#else
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#endif
#include <LOLIN_HP303B.h>

#include "rtc_mem.h"
#include "sensors.h"
#include "sht30.h"


/* Global Data Structures */
#ifdef IOTAPPSTORY
IOTAppStory IAS(COMPDATE, MODEBUTTON);
#endif
LOLIN_HP303B HP303BPressureSensor;
String nodename = NODE_BASE_NAME;
uint32_t rtc_mem[RTC_MEM_MAX]; //array for accessing RTC Memory
unsigned long server_shutdown_timeout;

/* Functions */
void preinit()
{
  // Global WiFi constructors are not called yet
  // (global class instances like WiFi, Serial... are not yet initialized)..
  // No global object methods or C++ exceptions can be called in here!
  //The below is a static class method, which is similar to a function, so it's ok.
  ESP8266WiFiClass::preinitWiFiOff();
}

// helper to return the uptime in ms
uint64_t uptime(void)
{
  uint64_t uptime_ms;
  flags_time_t *timestruct = (flags_time_t*) &rtc_mem[RTC_MEM_FLAGS_TIME];

  // boot timestamp + current millis()
  uptime_ms = timestruct->millis;
  uptime_ms += millis();
  return uptime_ms;
}

// helper to format a uint64_t as a String object
String format_u64(uint64_t val)
{
  char llu[21];
  llu[20]='\0';
  sprintf(llu, "%llu", val);
  return String(llu);
}

// helpers to store pressure readings in the rtc mem ring buffer
void store_reading(sensor_type_t type, int32_t val)
{
  sensor_reading_t *reading;
  int slot;

  // determine the next free slot in the RTC memory ring buffer
  slot = rtc_mem[RTC_MEM_FIRST_READING] + rtc_mem[RTC_MEM_NUM_READINGS];
  if (slot >= NUM_STORAGE_SLOTS)
    slot -= NUM_STORAGE_SLOTS;

  // update the ring buffer metadata
  if (rtc_mem[RTC_MEM_NUM_READINGS] == NUM_STORAGE_SLOTS)
    rtc_mem[RTC_MEM_FIRST_READING]++;
  else
    rtc_mem[RTC_MEM_NUM_READINGS]++;

  if (rtc_mem[RTC_MEM_FIRST_READING] >= NUM_STORAGE_SLOTS)
    rtc_mem[RTC_MEM_FIRST_READING]=0;

  // store the sensor reading in RTC memory at the given slot
  reading = (sensor_reading_t*) &rtc_mem[RTC_MEM_DATA+slot*NUM_WORDS(sensor_reading_t)];
  reading->type = type;
  reading->reading = val;
  reading->timestamp = uptime();
}

// helper to reset the rtc mem ring buffer
void clear_readings(void)
{
  // determine the next free slot in the RTC memory ring buffer
  rtc_mem[RTC_MEM_FIRST_READING]=0;
  rtc_mem[RTC_MEM_NUM_READINGS]=0;
}

// helper to print the stored pressure readings from the rtc mem ring buffer
void dump_readings(void)
{
#if (EXTRA_DEBUG != 0)
  const char typestrings[][11] = {
    "UNKNOWN",
    "TEMP (C)",
    "HUMI (%)",
    "PRES (kPa)",
    "PART (?)",
    "BATT (V)",
  };
  char formatted[47];
  formatted[46]=0;

  Serial.println("slot |     timestamp | type       |    value");
  Serial.println("-----+---------------+------------+---------");


  for (unsigned i=0; i < rtc_mem[RTC_MEM_NUM_READINGS]; i++) {
    sensor_reading_t *reading;
    const char* type;
    int slot;

    // find the slot indexed into the ring buffer
    slot = rtc_mem[RTC_MEM_FIRST_READING] + i;
    if (slot >= NUM_STORAGE_SLOTS)
      slot -= NUM_STORAGE_SLOTS;

    reading = (sensor_reading_t*) &rtc_mem[RTC_MEM_DATA+slot*NUM_WORDS(sensor_reading_t)];

    // determine the sensor type
    switch(reading->type) {
      case SENSOR_TEMPERATURE:
        type=typestrings[1];
      break;

      case SENSOR_HUMIDITY:
        type=typestrings[2];
      break;

      case SENSOR_PRESSURE:
        type=typestrings[3];
      break;

      case SENSOR_PARTICLE:
        type=typestrings[4];
      break;

      case SENSOR_BATTERY_VOLTAGE:
        type=typestrings[5];
      break;

      case SENSOR_UNKNOWN:
      default:
        type=typestrings[0];
      break;
    }

    // format an output row with the data, type, and timestamp
    snprintf(formatted, 45, "%4u | %13llu | %-10s | %+8.3f", i, reading->timestamp, type, reading->reading/1000.0);
    Serial.println(formatted);
  }
  Serial.println("[" + format_u64(uptime()) + "] dump complete");
#endif
}

// helper to transmit the readings formatted as a json string
bool transmit_readings(WiFiClient& client)
{
  unsigned result;
  String json;

  //format preamble
  json  = "{\"version\":1,\"timestamp\":";
  json += format_u64(uptime());
  json += ",\"node\":\""+nodename+"\",";
  json += "\"measurements\":[";

  //format measurements
  {
    const char typestrings[][12] = {
      "unknown",
      "temperature",
      "humidity",
      "pressure",
      "particles",
      "battery",
    };

    for (unsigned i=0; i < rtc_mem[RTC_MEM_NUM_READINGS]; i++) {
      sensor_reading_t *reading;
      const char* type;
      int slot;
  
      // find the slot indexed into the ring buffer
      slot = rtc_mem[RTC_MEM_FIRST_READING] + i;
      if (slot >= NUM_STORAGE_SLOTS)
        slot -= NUM_STORAGE_SLOTS;
  
      reading = (sensor_reading_t*) &rtc_mem[RTC_MEM_DATA+slot*NUM_WORDS(sensor_reading_t)];
  
      // determine the sensor type
      switch(reading->type) {
        case SENSOR_TEMPERATURE:
          type=typestrings[1];
        break;
  
        case SENSOR_HUMIDITY:
          type=typestrings[2];
        break;
  
        case SENSOR_PRESSURE:
          type=typestrings[3];
        break;
  
        case SENSOR_PARTICLE:
          type=typestrings[4];
        break;
  
        case SENSOR_BATTERY_VOLTAGE:
          type=typestrings[5];
        break;
  
        case SENSOR_UNKNOWN:
        default:
          type=typestrings[0];
        break;
      }
      json += "{\"timestamp\":";
      json += format_u64(reading->timestamp);
      json +=",\"type\":\"";
      json += type;
      json += "\",\"value\":";
      json += reading->reading/1000.0;
      json += "}";
      if ((i+1) < rtc_mem[RTC_MEM_NUM_READINGS])
        json += ",";
    }
  }

  //terminate the json objects
  json += "]}";

#if (EXTRA_DEBUG != 0)
  Serial.println("Transmitting to report server:");
  Serial.println(json);
#endif

  //transmit the results and verify the number of bytes written
  result = client.print(json);
  return (result == json.length());
}

// helper to manage uploading the readings to the report server
void upload_readings(void)
{
  WiFiClient client;
  String response;
  unsigned long timeout;

  Serial.print("Connecting to report server ");
  Serial.print(REPORT_HOST_NAME);
  Serial.print(":");
  Serial.println(REPORT_HOST_PORT);
  if (!client.connect(REPORT_HOST_NAME, REPORT_HOST_PORT)) {
    Serial.println("Connection Failed");
  } else {
    // This will send a string to the server
    Serial.println("Sending data to report server");
    if (client.connected()) {
      if (transmit_readings(client)) {
        // wait for response to be available
        timeout = millis();
        while (client.available() == 0) {
          if (millis() - timeout > REPORT_RESPONSE_TIMEOUT) {
            Serial.println("Timeout waiting for response!");
            break;
          }
          yield();
        }

        if (client.available()) {
          // Read response from the report server
          response = String("");
          while (client.available()) {
            char ch = static_cast<char>(client.read());
            response += ch;
          }
    
          Serial.print("Response from report server: ");
          Serial.println(response);
          if (response == "OK")
            clear_readings();
        }
      }
    }
  }

  client.stop();
  delay(10);
}

// helper for saving rtc memory before entering deep sleep
void deep_sleep(uint64_t time_us)
{
  flags_time_t *timestruct = (flags_time_t*) &rtc_mem[RTC_MEM_FLAGS_TIME];

  // update the stored millis including some overhead for the write, suspend, and wake
  timestruct->millis += millis() + time_us/1000 + SLEEP_OVERHEAD_MS;

  // update the header checksum
  rtc_mem[RTC_MEM_CHECK] = PREINIT_MAGIC - rtc_mem[RTC_MEM_BOOT_COUNT];

  // store the array to RTC memory and enter suspend
  ESP.rtcUserMemoryWrite(0, rtc_mem, sizeof(rtc_mem));
  ESP.deepSleep(time_us, RF_DISABLED);
}

// helper to read and store values from the SHT30 temperature and humidity sensor
bool read_sht30(void)
{
  sht30_data_t data;
  int ret;
  ret = sht30_get(SHT30_ADDR, SHT30_RPT_HIGH, &data);
  Serial.println();
  if (ret != 0) {
    Serial.print("Error Reading SHT30 ret=");
    Serial.println(ret);
    return false;
  } else {
    store_reading(SENSOR_TEMPERATURE, sht30_parse_temp_c(data)*1000.0 + 0.5);
    store_reading(SENSOR_HUMIDITY, sht30_parse_humidity(data)*1000.0 + 0.5);
#if (EXTRA_DEBUG != 0)
    Serial.print("Temperature: ");
    Serial.print(sht30_parse_temp_c(data), 3);
    Serial.print("°C (");
    Serial.print(sht30_parse_temp_f(data), 3);
    Serial.println("°F)");
    Serial.print("Humidity: ");
    Serial.println(sht30_parse_humidity(data), 3);
#endif
  }

  return true;
}

// helper to read and store values from the HP303B barametric pressure sensor
void read_hp303b(bool measure_temp)
{
  //OVERSAMPLING TABLE
  // Val | Time (ms) | Noise (counts)
  //   0 |      15.6 | 12
  //   1 |      17.6 | 6
  //   2 |      20.6 | 4
  //   3 |      25.5 | 3
  //   4 |      39.6 | 2
  //   5 |      65.6 | 2
  //   6 |     116.6 | 2
  //   7 |     218.7 | 1
  int16_t oversampling = 4;
  int16_t ret;
  int32_t pressure;

  // Call begin to initialize HP303BPressureSensor
  // The default address is 0x77 and does not need to be given.
  HP303BPressureSensor.begin();

  if (measure_temp) {
    int32_t temperature;
    //lets the HP303B perform a Single temperature measurement with the last (or standard) configuration
    //The result will be written to the paramerter temperature
    //ret = HP303BPressureSensor.measureTempOnce(temperature);
    //the commented line below does exactly the same as the one above, but you can also config the precision
    //oversampling can be a value from 0 to 7
    //the HP303B will perform 2^oversampling internal temperature measurements and combine them to one result with higher precision
    //measurements with higher precision take more time, consult datasheet for more information
    ret = HP303BPressureSensor.measureTempOnce(temperature, oversampling);

    if (ret != 0) {
      //Something went wrong.
      //Look at the library code for more information about return codes
      Serial.print("Error Reading HP303B (temperature) ret=");
      Serial.println(ret);
    } else {
      store_reading(SENSOR_TEMPERATURE, temperature*1000);
#if (EXTRA_DEBUG != 0)
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.print(" °C (");
      Serial.print(((float)temperature * 9/5)+32.0);
      Serial.println(" °F)");
#endif
    }
  }

  //Pressure measurement behaves like temperature measurement
  //ret = HP303BPressureSensor.measurePressureOnce(pressure);
  ret = HP303BPressureSensor.measurePressureOnce(pressure, oversampling);
  if (ret != 0) {
    //Something went wrong.
    //Look at the library code for more information about return codes
    Serial.print("Error Reading HP303B (pressure) ret=");
    Serial.println(ret);
  } else {
    store_reading(SENSOR_PRESSURE, pressure);
#if (EXTRA_DEBUG != 0)
    Serial.print("Pressure: ");
    Serial.print(pressure/1000.0, 3);
    Serial.print(" kPa (");
    Serial.print((float)pressure/3386.39);
    Serial.println(" in Hg)");
#endif
  }
}

// helper to read the ESP VCC voltage level
void read_vcc(void)
{
  uint16_t val;
  val = ESP.getVcc();

  if (val > 37000)
    Serial.println("Error reading VCC value");
  else
    store_reading(SENSOR_BATTERY_VOLTAGE, val);

#if (EXTRA_DEBUG != 0)
  Serial.print("Battery voltage: ");
  Serial.print(val/1000.0, 3);
  Serial.println("V");
#endif
}

// Read from all of the attached sensors and store the readings
void take_readings(void)
{
  bool sht30_ok;

  //read temp/humidity from SHT30
  sht30_ok = read_sht30();

  //read temp/humidity from HP303B
  read_hp303b(!sht30_ok);

  read_vcc();
}

// helper to clear/reinitialize rtc memory
void invalidate_rtc(void)
{
  memset(rtc_mem, 0, sizeof(rtc_mem));
  ESP.rtcUserMemoryWrite(0, rtc_mem, sizeof(rtc_mem));
}

// helper to run the WiFi configuration mode
void enter_config_mode(void)
{
#ifdef IOTAPPSTORY
    invalidate_rtc(); //invalidate existing RTC memory, IAS will overwrite it...
    IAS.begin('P');
    IAS.runConfigServer();
#else
    WiFiManager wifi_manager;
    WiFiManagerParameter custom_report_host("report_host", "report server hostname/IP", "", 40, "><label for=\"report_host\">Custom Report Server</label");
    WiFiManagerParameter custom_report_port("report_port", "report server port number", "2880", 40, "><label for=\"report_port\">Custom Report Server Port Number</label");
    WiFiManagerParameter clock_drift_adj("clock_drift", "clock drift (ms)", String(SLEEP_OVERHEAD_MS).c_str(), 6, "><label for=\"clock_drift\">Clock Drift Compensation</label");
    WiFiManagerParameter temp_adj("temp_adj", "temperature calibration (°C)", "0.0", 10, "><label for=\"temp_adj\">Temperatue Offset Calibration</label");
    WiFiManagerParameter humidity_adj("humidity_adj", "humidity calibration (%)", "0.0", 10, "><label for=\"humidity_adj\">Humidity Offset Calibration</label");
    WiFiManagerParameter pressure_adj("pressure_adj", "pressure calibration (kPa)", "0.0", 10, "><label for=\"pressure_adj\">Pressure Offset Calibration</label");

    wifi_manager.addParameter(&custom_report_host);
    wifi_manager.addParameter(&custom_report_port);
    wifi_manager.addParameter(&clock_drift_adj);
    wifi_manager.addParameter(&temp_adj);
    wifi_manager.addParameter(&humidity_adj);
    wifi_manager.addParameter(&pressure_adj);
    wifi_manager.setConfigPortalTimeout(CONFIG_SERVER_MAX_TIME);

    Serial.println("Starting config server");
    Serial.print("Connect to AP ");
    Serial.println(nodename);
    if (wifi_manager.startConfigPortal(nodename.c_str())) {
      // TODO save config settings
    }
#endif
}

// helper to connect to the WiFi and return the status
wl_status_t connect_wifi(void)
{
  wl_status_t retval;

#ifdef IOTAPPSTORY
  IAS.begin('P');
#else
  Serial.println("Connecting to AP");
  WiFiManager wifi_manager;
  wifi_manager.setConfigPortalTimeout(1);
  wifi_manager.autoConnect();
#endif

  retval = WiFi.status();
#if (EXTRA_DEBUG != 0)
  Serial.print("WiFi status ");
  switch(retval) {
    case WL_NO_SHIELD: Serial.println("WL_NO_SHIELD"); break;
    case WL_IDLE_STATUS: Serial.println("WL_IDLE_STATUS"); break;
    case WL_NO_SSID_AVAIL: Serial.println("WL_NO_SSID_AVAIL"); break;
    case WL_SCAN_COMPLETED: Serial.println("WL_SCAN_COMPLETED"); break;
    case WL_CONNECTED: Serial.println("WL_CONNECTED"); break;
    case WL_CONNECT_FAILED: Serial.println("WL_CONNECT_FAILED"); break;
    case WL_CONNECTION_LOST: Serial.println("WL_CONNECTION_LOST"); break;
    case WL_DISCONNECTED: Serial.println("WL_DISCONNECTED"); break;
    default: Serial.println(retval); break;
  }
#endif

  return retval;
}

// helper to read rtc user memory and print some details to serial
// increments the boot count and returns true if the RTC memory was OK
bool load_rtc_memory(void)
{
  bool retval = true;

  ESP.rtcUserMemoryRead(0, rtc_mem, sizeof(rtc_mem));
  if (rtc_mem[RTC_MEM_CHECK] + rtc_mem[RTC_MEM_BOOT_COUNT] != PREINIT_MAGIC) {
    Serial.println("Preinit magic doesn't compute, reinitializing");
    invalidate_rtc();
    retval = false;
  }
  rtc_mem[RTC_MEM_BOOT_COUNT]++;

  Serial.print("[" + format_u64(uptime()) + "] ");
  Serial.print("setup: reset reason=");
  Serial.print(ESP.getResetReason());
  Serial.print(", boot count=");
  Serial.print(rtc_mem[RTC_MEM_BOOT_COUNT]);
#if (EXTRA_DEBUG != 0)
  flags_time_t *flags = (flags_time_t*) &rtc_mem[RTC_MEM_FLAGS_TIME];
  Serial.print(", flags=0x");
  Serial.print((uint8_t)flags->flags, HEX);
#endif

  Serial.print(", num readings=");
  Serial.println(rtc_mem[RTC_MEM_NUM_READINGS]);

  if (RTC_MEM_MAX > 128)
    Serial.println("*************************\nWARNING RTC_MEM_MAX > 128\n*************************");

  return retval;
}

void setup(void)
{
  bool rtc_config_valid = false;

  pinMode(LED_BUILTIN, INPUT);

  Wire.begin();
  Serial.begin(SERIAL_SPEED);
  delay(2);
  Serial.println();

  nodename += String(ESP.getChipId());

  rtc_config_valid = load_rtc_memory();

#ifdef IOTAPPSTORY
  IAS.preSetDeviceName(nodename);
  IAS.preSetAutoUpdate(false);
  IAS.setCallHome(false);
#endif

  // We can detect a "double press" of the reset button as a regular Ext Reset
  // This is because we spend most of our time asleep and a single press will
  // generally appear as a deep-sleep wakeup.
  // We will use this to enter the WiFi config mode.
  // However, we must ignore it on the first boot after reprogramming or inserting
  // the battery -- so only pay attention if the RTC memory checksum is OK.
  if ((ESP.getResetInfoPtr()->reason == REASON_EXT_SYS_RST) && rtc_config_valid) {
    enter_config_mode();
  } else {
    take_readings();
    dump_readings();

    if (rtc_mem[RTC_MEM_NUM_READINGS] >= HIGH_WATER_SLOT) {
      // time to connect and upload our readings
      if (WL_CONNECTED == connect_wifi())
        upload_readings();
    }
  }

  WiFi.mode(WIFI_OFF);
  deep_sleep(SLEEP_TIME_US);
}

void loop(void)
{
  //should never get here
  WiFi.mode(WIFI_OFF);
  deep_sleep(SLEEP_TIME_US);
}
