#include <ArduinoBLE.h>
#include <karate_inferencing.h>
#include <Arduino_BMI270_BMM150.h>
#include <Arduino_LPS22HB.h>
#include <Arduino_HS300x.h>
#include <Arduino_APDS9960.h>

// === BLE UART Service (Nordic UART) ===
BLEService uartService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLECharacteristic txCharacteristic("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLERead | BLENotify, 512);
BLECharacteristic rxCharacteristic("6E400002-B5A3-F393-E0A9-E50E24DCCA9E", BLEWriteWithoutResponse, 512);

// === Sensor Fusion Setup ===
enum sensor_status { NOT_USED = -1, NOT_INIT, INIT, SAMPLED };

typedef struct {
    const char *name;
    float *value;
    uint8_t (*poll_sensor)(void);
    bool (*init_sensor)(void);
    sensor_status status;
} eiSensors;

#define CONVERT_G_TO_MS2    9.80665f
#define MAX_ACCEPTED_RANGE  2.0f
#define N_SENSORS 18
static float data[N_SENSORS];
static int8_t fusion_sensors[N_SENSORS];
static int fusion_ix = 0;
static const bool debug_nn = false;

// === Function Prototypes ===
bool init_IMU(void);
bool init_HTS(void);
bool init_BARO(void);
bool init_APDS(void);

uint8_t poll_acc(void);
uint8_t poll_gyr(void);
uint8_t poll_mag(void);
uint8_t poll_HTS(void);
uint8_t poll_BARO(void);
uint8_t poll_APDS_color(void);
uint8_t poll_APDS_proximity(void);
uint8_t poll_APDS_gesture(void);

void blePrintf(const char *fmt, ...);

// === Sensor Mapping ===
eiSensors sensors[] = {
    "accX", &data[0], &poll_acc, &init_IMU, NOT_USED,
    "accY", &data[1], &poll_acc, &init_IMU, NOT_USED,
    "accZ", &data[2], &poll_acc, &init_IMU, NOT_USED,
    "gyrX", &data[3], &poll_gyr, &init_IMU, NOT_USED,
    "gyrY", &data[4], &poll_gyr, &init_IMU, NOT_USED,
    "gyrZ", &data[5], &poll_gyr, &init_IMU, NOT_USED,
    "magX", &data[6], &poll_mag, &init_IMU, NOT_USED,
    "magY", &data[7], &poll_mag, &init_IMU, NOT_USED,
    "magZ", &data[8], &poll_mag, &init_IMU, NOT_USED,
    "temperature", &data[9], &poll_HTS, &init_HTS, NOT_USED,
    "humidity", &data[10], &poll_HTS, &init_HTS, NOT_USED,
    "pressure", &data[11], &poll_BARO, &init_BARO, NOT_USED,
    "red", &data[12], &poll_APDS_color, &init_APDS, NOT_USED,
    "green", &data[13], &poll_APDS_color, &init_APDS, NOT_USED,
    "blue", &data[14], &poll_APDS_color, &init_APDS, NOT_USED,
    "brightness", &data[15], &poll_APDS_color, &init_APDS, NOT_USED,
    "proximity", &data[16], &poll_APDS_proximity, &init_APDS, NOT_USED,
    "gesture", &data[17], &poll_APDS_gesture, &init_APDS, NOT_USED,
};

void setup() {
    Serial.begin(115200);
    delay(100);

    // Don't wait forever for Serial!
    if (Serial) Serial.println("Starting...");

    // === BLE Setup ===
    if (!BLE.begin()) {
        Serial.println("BLE init failed!");
        while (1);
    }

    BLE.setLocalName("Nano33BLE");
    BLE.setAdvertisedService(uartService);
    uartService.addCharacteristic(txCharacteristic);
    uartService.addCharacteristic(rxCharacteristic);
    BLE.addService(uartService);
    BLE.advertise();

    blePrintf("BLE UART started.\r\n");

    // === Sensor Fusion Init ===
    if (!ei_connect_fusion_list(EI_CLASSIFIER_FUSION_AXES_STRING)) {
        blePrintf("ERR: Sensor list mismatch.\r\n");
        return;
    }

    for (int i = 0; i < fusion_ix; i++) {
        if (sensors[fusion_sensors[i]].status == NOT_INIT) {
            sensors[fusion_sensors[i]].status = (sensor_status)sensors[fusion_sensors[i]].init_sensor();
            if (!sensors[fusion_sensors[i]].status) {
                blePrintf("%s init failed.\r\n", sensors[fusion_sensors[i]].name);
            } else {
                blePrintf("%s init OK.\r\n", sensors[fusion_sensors[i]].name);
            }
        }
    }
}

void loop() {
    BLEDevice central = BLE.central();

    if (central) {
        blePrintf("Connected: %s\r\n", central.address().c_str());

        while (central.connected()) {
            blePrintf("Starting inferencing in 2s...\r\n");
            delay(2000);

            if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != fusion_ix) {
                blePrintf("ERR: Fusion mismatch. Need: %s\r\n", EI_CLASSIFIER_FUSION_AXES_STRING);
                return;
            }

            float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = { 0 };

            for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) {
                int64_t next_tick = (int64_t)micros() + ((int64_t)EI_CLASSIFIER_INTERVAL_MS * 1000);

                for (int i = 0; i < fusion_ix; i++) {
                    if (sensors[fusion_sensors[i]].status == INIT) {
                        sensors[fusion_sensors[i]].poll_sensor();
                        sensors[fusion_sensors[i]].status = SAMPLED;
                    }
                    if (sensors[fusion_sensors[i]].status == SAMPLED) {
                        buffer[ix + i] = *sensors[fusion_sensors[i]].value;
                        sensors[fusion_sensors[i]].status = INIT;
                    }
                }

                int64_t wait_time = next_tick - (int64_t)micros();
                if (wait_time > 0) delayMicroseconds(wait_time);
            }

            signal_t signal;
            if (numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal) != 0) return;

            ei_impulse_result_t result = { 0 };
            EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
            if (err != EI_IMPULSE_OK) {
                blePrintf("Classifier error: %d\r\n", err);
                return;
            }

            // === Output Result ===
            blePrintf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.):\r\n",
                      result.timing.dsp, result.timing.classification, result.timing.anomaly);
            for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
                blePrintf("%s: %.5f\r\n", result.classification[ix].label, result.classification[ix].value);
            }

#if EI_CLASSIFIER_HAS_ANOMALY == 1
            blePrintf("Anomaly score: %.3f\r\n", result.anomaly);
#endif
        }

        blePrintf("Disconnected.\r\n");
    }
}

// === BLE Print Function ===
void blePrintf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Serial.print(buf);  // optional debug

    int len = strlen(buf);
    for (int i = 0; i < len; i += 20) {
        String chunk = String(buf).substring(i, i + 20);
        txCharacteristic.writeValue((const uint8_t *)chunk.c_str(), chunk.length());
        delay(10);
    }
}

// === Sensor Functions ===
float ei_get_sign(float number) { return (number >= 0.0) ? 1.0 : -1.0; }

bool init_IMU() {
    static bool status = false;
    if (!status) status = IMU.begin();
    return status;
}

bool init_HTS() {
    static bool status = false;
    if (!status) status = HS300x.begin();
    return status;
}

bool init_BARO() {
    static bool status = false;
    if (!status) status = BARO.begin();
    return status;
}

bool init_APDS() {
    static bool status = false;
    if (!status) status = APDS.begin();
    return status;
}

uint8_t poll_acc() {
    if (IMU.accelerationAvailable()) {
        IMU.readAcceleration(data[0], data[1], data[2]);
        for (int i = 0; i < 3; i++) {
            if (fabs(data[i]) > MAX_ACCEPTED_RANGE)
                data[i] = ei_get_sign(data[i]) * MAX_ACCEPTED_RANGE;
        }
        data[0] *= CONVERT_G_TO_MS2;
        data[1] *= CONVERT_G_TO_MS2;
        data[2] *= CONVERT_G_TO_MS2;
    }
    return 0;
}
uint8_t poll_gyr() { if (IMU.gyroscopeAvailable()) IMU.readGyroscope(data[3], data[4], data[5]); return 0; }
uint8_t poll_mag() { if (IMU.magneticFieldAvailable()) IMU.readMagneticField(data[6], data[7], data[8]); return 0; }
uint8_t poll_HTS() { data[9] = HS300x.readTemperature(); data[10] = HS300x.readHumidity(); return 0; }
uint8_t poll_BARO() { data[11] = BARO.readPressure(); return 0; }

uint8_t poll_APDS_color() {
    int rgb[4];
    if (APDS.colorAvailable()) {
        APDS.readColor(rgb[0], rgb[1], rgb[2], rgb[3]);
        data[12] = rgb[0]; data[13] = rgb[1]; data[14] = rgb[2]; data[15] = rgb[3];
    }
    return 0;
}
uint8_t poll_APDS_proximity() { if (APDS.proximityAvailable()) data[16] = APDS.readProximity(); return 0; }
uint8_t poll_APDS_gesture() { if (APDS.gestureAvailable()) data[17] = APDS.readGesture(); return 0; }

static int8_t ei_find_axis(char *axis_name) {
    for (int i = 0; i < N_SENSORS; i++) {
        if (strstr(axis_name, sensors[i].name)) return i;
    }
    return -1;
}
static bool ei_connect_fusion_list(const char *input_list) {
    char *input_string = (char *)ei_malloc(strlen(input_list) + 1);
    if (!input_string) return false;
    strncpy(input_string, input_list, strlen(input_list) + 1);

    memset(fusion_sensors, 0, N_SENSORS);
    fusion_ix = 0;

    char *buff = strtok(input_string, "+");
    while (buff != NULL) {
        int8_t found_axis = ei_find_axis(buff);
        if (found_axis >= 0 && fusion_ix < N_SENSORS) {
            fusion_sensors[fusion_ix++] = found_axis;
            sensors[found_axis].status = NOT_INIT;
        }
        buff = strtok(NULL, "+ ");
    }

    ei_free(input_string);
    return fusion_ix > 0;
}
