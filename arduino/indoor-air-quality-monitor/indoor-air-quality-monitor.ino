#include <TH02_dev.h>
#include "Arduino.h"
#include <Wire.h> 
#include "Adafruit_SGP30.h"

Adafruit_SGP30 sgp;

/* 
 * return absolute humidity [mg/m^3] with approximation formula
 * @param temperature [°C]
 * @param humidity [%RH]
 */
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
}

/*
 * return unique identifier to be used when uploading measurements
 */
String getId() {
    String id = "0x" + String(sgp.serialnumber[0], HEX) + String(sgp.serialnumber[1], HEX) + String(sgp.serialnumber[2], HEX);
    return id;
}

/*
 * upload data to the server
 * @param data
 */
void uploadData(String data) {
    Serial.println(data);
}

void reset() {
    Serial.println("Trying to reset SGP30. This fails most of the times and the program stops. ");
    if (sgp.softReset()) {
        Serial.println("SGP30 soft reset succeeded.");
        if (sgp.begin()) {
            Serial.println("SGP30 begin succeeded.");
        } else {
            Serial.println("SGP30 begin failed.");
        }
    } else {
        Serial.println("SGP30 soft reset failed.");
    }
}

/*
 * Get the ISO 8601 timestamp of the measurement. Access a web service to get it. 
 */
String getTimestamp() {
    return "2021-07-01 00:00:00Z";
}

/*
 * Wait before staring the next loop
 * @param delayS - delay in seconds
 * @param error - error code, 0 means no error --> slow blink, > 0 means error --> fast blink
 */
void wait(int delayS, int error) {
    int totalDelayMs = delayS / 2 * 1000;
    int delayMs = 1000;
    if (error != 0) {
        delayMs = 250;
    } 
    int n = totalDelayMs / delayMs;
    for (int i = 0; i < n; i ++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(delayMs);
        digitalWrite(LED_BUILTIN, LOW);
        delay(delayMs);
    }
}

void setup() {
    Serial.begin(115200);

    // Wait for serial console to open
    while (!Serial) { delay(10); } 

    // initialize digital pin LED_BUILTIN as an output.
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.println("SGP30 test");

    if (! sgp.begin()){
        Serial.println("Sensor not found :(");
        while (1);
    }

    Serial.print("SGP30 TVOC/eCO2 sensor is available. ID: "); Serial.print(getId() + "\r\n"); 

    Serial.println("TH02 test");
    /* Power up,delay 150ms,until voltage is stable */
    delay(150);
    /* Reset HP20x_dev */
    TH02.begin();
    delay(100);
  
    /* Determine TH02_dev is available or not */
    Serial.println("TH02 temperaure/humidity sensor is available.");  

    Serial.print("\r\n");
}

int counter = 0;

void loop() {
    Serial.print("Next loop. Counter: "); Serial.print(counter); Serial.print("\r\n");
  
    int error = 0;

    String id = getId();

    counter++;

    // Get baseline for SGP sensor every 30 loops
    if (counter % 30 == 0) {
        Serial.println("Get baseline readings for SGP30");
        uint16_t TVOC_base, eCO2_base;
        if (! sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
            Serial.println("Failed to get baseline readings");
            error = 1;
            counter = 30;  // To make sure attempt is made the next loop
        } else {
            Serial.print("Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX); Serial.print(", TVOC: 0x"); Serial.println(TVOC_base, HEX);
            error = 0;
            counter = 0;
        }
    }

    String timestamp = getTimestamp();

    String data = "";

    float temperature = TH02.ReadTemperature(); 
    Serial.print("Temperature: "); Serial.print(temperature); Serial.print("\r\n");
    data += id + ";" + String(timestamp) + ";" + String(error) + ";" + "temperature" + ";" + String(temperature) + "\r\n";
   
    float humidity = TH02.ReadHumidity();
    Serial.print("Humidity: "); Serial.print(humidity); Serial.print("\r\n");
    data += id + ";" + String(timestamp) + ";" + String(error) + ";" + "humidity" + ";" + String(humidity) + "\r\n";

    // If you have a temperature / humidity sensor, you can set the absolute humidity to enable the humditiy compensation for the air quality signals
    sgp.setHumidity(getAbsoluteHumidity(temperature, humidity));

    if (sgp.IAQmeasure()) {
        float tvoc = sgp.TVOC;
        Serial.print("TVOC: "); Serial.print(tvoc); Serial.print("\r\n");
        data += id + ";" + String(timestamp) + ";" + String(error) + ";" + "tvoc" + ";" + String(tvoc) + "\r\n";

        float eco2 = sgp.eCO2;
        Serial.print("eCO2: "); Serial.print(eco2); Serial.print("\r\n");
        data += id + ";" + String(timestamp) + ";" + String(error) + ";" + "eco2" + ";" + String(eco2) + "\r\n";
    } else {
        Serial.println("Measurement failed");
        error = 2;
        reset();
    }

    if (sgp.IAQmeasureRaw()) {
        float rawH2 = sgp.rawH2;
        Serial.print("Raw H2: "); Serial.print(rawH2); Serial.print("\r\n");
        data += id + ";" + String(timestamp) + ";" + String(error) + ";" + "raw_h2" + ";" + String(rawH2) + "\r\n";

        float rawEthanol = sgp.rawEthanol;
        Serial.print("Raw Ethanol: "); Serial.print(rawEthanol); Serial.print("\r\n");
        data += id + ";" + String(timestamp) + ";" + String(error) + ";" + "raw_ethanol" + ";" + String(rawEthanol) + "\r\n";
    } else {
        Serial.println("Raw measurement failed");
        error = 3;
        reset();
    }

    Serial.print("\r\n");

    uploadData(data);

    wait(10, error);
}
