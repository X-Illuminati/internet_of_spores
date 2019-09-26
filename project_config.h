#ifndef _PROJECT_CONFIG_H_
#define _PROJECT_CONFIG_H_

/* Global Configurations */
#ifdef IOTAPPSTORY
#define COMPDATE                (__DATE__ __TIME__)
#define MODEBUTTON              (0) /* unused */
#define CONFIG_SERVER_MAX_TIME  (600 /* total seconds */)
#else
#define SERIAL_SPEED            (115200)
#define CONFIG_SERVER_MAX_TIME  (120 /* seconds without client */)
#endif
#define EXTRA_DEBUG             (0)
#define SLEEP_TIME_US           (15000000)
#define SLEEP_OVERHEAD_MS       (DEFAULT_SLEEP_CLOCK_ADJ) /* guess at the overhead time for entering and exitting sleep */
#define BUILD_UNIQUE_ID         (__TIME__[3]*1000+__TIME__[4]*100+__TIME__[6]*10+__TIME__[7])
#define PREINIT_MAGIC           (0xAA559876 ^ BUILD_UNIQUE_ID)
#define NUM_STORAGE_SLOTS       (59)
#define HIGH_WATER_SLOT         (NUM_STORAGE_SLOTS-12)
#define SHT30_ADDR              (0x45)
#define REPORT_RESPONSE_TIMEOUT (2000)

//persistent storage file names
#define PERSISTENT_NODE_NAME        "node_name"
#define PERSISTENT_REPORT_HOST_NAME "report_host_name"
#define PERSISTENT_REPORT_HOST_PORT "report_host_port"
#define PERSISTENT_CLOCK_CALIB      "clock_drift_calibration"
#define PERSISTENT_TEMP_CALIB       "temperature_calibration"
#define PERSISTENT_HUMIDITY_CALIB   "humidity_calibration"
#define PERSISTENT_PRESSURE_CALIB   "pressure_calibration"

#define DEFAULT_NODE_BASE_NAME      "spores-"
#define DEFAULT_REPORT_HOST_NAME    "192.168.0.1"
#define DEFAULT_REPORT_HOST_PORT    (2880)
#define DEFAULT_SLEEP_CLOCK_ADJ     (120)
#define DEFAULT_TEMP_CALIB          (0.0)
#define DEFAULT_HUMIDITY_CALIB      (0.0)
#define DEFAULT_PRESSURE_CALIB      (0.0)

#endif /* _PROJECT_CONFIG_H_ */