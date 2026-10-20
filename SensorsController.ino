#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <MQUnifiedsensor.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <Adafruit_BME680.h>
#include "config.h"
#include "secrets.h"

MQUnifiedsensor MQ135(BOARD_NAME, MQ135_VOLTAGE_RESOLUTION, ADC_BIT_RESOLUTION, MQ135_PIN, MQ135_TYPE);
bool mq135Calibrado = false;
float CO2;
float CO;
float Alcohol;
float Toluen;
float NH4;
float Aceton;

Adafruit_BME680 bme680;
bool bme680Disponivel = false;
float temperaturaBME680;
float umidadeBME680;
float sensacaoTermicaBME680;
float pressaoBME680;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float temperaturaDS18B20;

int analogBuffer[TDS_SAMPLE_COUNT];
float averageVoltage = 0;
float tdsValue = 0;
int tdsRawValue = 0;
float tdsCompensationVoltage = 0;
float tdsCompensationTemperature = 25.0;

int turbidity = 0;
float turbidityVoltage = 0;
float turbiditySensorVoltage = 0;
float turbidityNtu = 0;

int phRawValue = 0;
float phVoltage = 0;
float phSensorVoltage = 0;
float ph_act;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

String ultimaChaveHistoricoArGravada = "";
String ultimaChaveHistoricoAquarioGravada = "";

void setup() {
  Serial.begin(9600);

  inicializarFilaFirebase();

  analogReadResolution(ADC_BIT_RESOLUTION);
  analogSetPinAttenuation(PH_SENSOR_PIN, ADC_11db);
  digitalWrite(TEMPERATURE_SENSOR_RELAY_PIN, TEMPERATURE_SENSOR_RELAY_INACTIVE_STATE);
  pinMode(TEMPERATURE_SENSOR_RELAY_PIN, OUTPUT);

  configurarMQ135();
  bme680Disponivel = configurarBME680();

  conectarWiFi();

  timeClient.begin();
  timeClient.setTimeOffset(NTP_TIME_OFFSET_SECONDS);

  if (!sincronizarHorario()) {
    Serial.println("Historico inicial ignorado ate o relogio sincronizar.");
  }
}

void loop() {
  executarTarefasPeriodicas();
}
