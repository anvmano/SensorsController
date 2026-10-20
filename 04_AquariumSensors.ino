bool lerDS18B20() {
  digitalWrite(TEMPERATURE_SENSOR_RELAY_PIN, TEMPERATURE_SENSOR_RELAY_ACTIVE_STATE);
  delay(TEMPERATURE_SENSOR_POWER_ON_DELAY_MS);

  bool ds18b20Detectado = false;
  for (int tentativaDeteccao = 1; tentativaDeteccao <= DS18B20_DETECTION_ATTEMPTS; tentativaDeteccao++) {
    sensors.begin();

    if (sensors.getDeviceCount() > 0) {
      ds18b20Detectado = true;
      break;
    }

    Serial.print("Erro: DS18B20 nao detectado apos energizar. Tentativa ");
    Serial.print(tentativaDeteccao);
    Serial.print("/");
    Serial.println(DS18B20_DETECTION_ATTEMPTS);

    if (tentativaDeteccao < DS18B20_DETECTION_ATTEMPTS) {
      delay(DS18B20_DETECTION_RETRY_DELAY_MS);
    }
  }

  if (!ds18b20Detectado) {
    digitalWrite(TEMPERATURE_SENSOR_RELAY_PIN, TEMPERATURE_SENSOR_RELAY_INACTIVE_STATE);
    return false;
  }

  if (DS18B20_DISCARD_FIRST_READ) {
    sensors.requestTemperatures();
    sensors.getTempCByIndex(0);
    delay(DS18B20_RETRY_DELAY_MS);
  }

  for (int tentativa = 1; tentativa <= DS18B20_READ_ATTEMPTS; tentativa++) {
    sensors.requestTemperatures();
    float temperaturaLida = sensors.getTempCByIndex(0);

    if (temperaturaLida != DEVICE_DISCONNECTED_C && !isnan(temperaturaLida)) {
      temperaturaDS18B20 = round(temperaturaLida * 100.0) / 100.0;
      digitalWrite(TEMPERATURE_SENSOR_RELAY_PIN, TEMPERATURE_SENSOR_RELAY_INACTIVE_STATE);
      return true;
    }

    Serial.print("Erro: DS18B20 desconectado ou leitura invalida. Tentativa ");
    Serial.print(tentativa);
    Serial.print("/");
    Serial.println(DS18B20_READ_ATTEMPTS);

    if (tentativa < DS18B20_READ_ATTEMPTS) {
      delay(DS18B20_RETRY_DELAY_MS);
    }
  }

  digitalWrite(TEMPERATURE_SENSOR_RELAY_PIN, TEMPERATURE_SENSOR_RELAY_INACTIVE_STATE);
  return false;
}

bool lerTDS(float temperaturaCompensacao) {
  long tdsRawSum = 0;
  for (int i = 0; i < TDS_SAMPLE_COUNT; i++) {
    analogBuffer[i] = analogRead(TDS_SENSOR_PIN);
    tdsRawSum += analogBuffer[i];
    delay(TDS_SAMPLE_INTERVAL_MS);
  }

  tdsRawValue = round((float)tdsRawSum / TDS_SAMPLE_COUNT);
  averageVoltage = tdsRawValue * (float)ADC_REFERENCE_VOLTAGE / ADC_MAX_VALUE;
  tdsCompensationTemperature = temperaturaCompensacao;
  float compensationCoefficient = 1.0 + 0.02 * (tdsCompensationTemperature - 25.0);

  if (compensationCoefficient <= 0) {
    Serial.println("Erro: coeficiente de compensacao do TDS invalido.");
    return false;
  }

  tdsCompensationVoltage = averageVoltage / compensationCoefficient;
  tdsValue = (133.42 * tdsCompensationVoltage * tdsCompensationVoltage * tdsCompensationVoltage - 255.86 * tdsCompensationVoltage * tdsCompensationVoltage + 857.39 * tdsCompensationVoltage) * 0.5;

  if (isnan(tdsValue) || tdsValue < 0) {
    Serial.println("Erro: leitura TDS invalida.");
    return false;
  }

  Serial.print("TDS: ");
  Serial.print(tdsValue, 0);
  Serial.print(" ppm | raw: ");
  Serial.print(tdsRawValue);
  Serial.print(" | tensao ADC: ");
  Serial.print(averageVoltage, 3);
  Serial.println(" V");
  return true;
}

int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++) {
    bTab[i] = bArray[i];
  }

  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }

  if ((iFilterLen & 1) > 0) {
    bTemp = bTab[(iFilterLen - 1) / 2];
  } else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}

bool lerTurbidez() {
  int turbiditySamples[TURBIDEZ_SAMPLE_COUNT];
  for (int i = 0; i < TURBIDEZ_SAMPLE_COUNT; i++) {
    turbiditySamples[i] = analogRead(TURBIDEZ_SENSOR_PIN);
    delay(TURBIDEZ_SAMPLE_INTERVAL_MS);
  }

  turbidity = getMedianNum(turbiditySamples, TURBIDEZ_SAMPLE_COUNT);
  turbidityVoltage = turbidity * (float)ADC_REFERENCE_VOLTAGE / ADC_MAX_VALUE;
  turbiditySensorVoltage = turbidityVoltage * TURBIDEZ_ADC_TO_SENSOR_VOLTAGE_FACTOR;

  if (turbidity < 0 || turbidity > ADC_MAX_VALUE || isnan(turbidityVoltage) || isnan(turbiditySensorVoltage)) {
    Serial.println("Erro: leitura de turbidez invalida.");
    return false;
  }

  if (turbiditySensorVoltage >= TURBIDEZ_CLEAR_WATER_VOLTAGE) {
    turbidityNtu = 0;
  } else {
    turbidityNtu = -1120.4 * turbiditySensorVoltage * turbiditySensorVoltage + 5742.3 * turbiditySensorVoltage - 4352.9;
    turbidityNtu = constrain(turbidityNtu, 0.0, TURBIDEZ_MAX_NTU);
  }

  Serial.print("Turbidez raw: ");
  Serial.print(turbidity);
  Serial.print(" | tensao ADC: ");
  Serial.print(turbidityVoltage, 3);
  Serial.print(" V | tensao sensor: ");
  Serial.print(turbiditySensorVoltage, 3);
  Serial.print(" V | NTU estimado: ");
  Serial.print(turbidityNtu, 0);
  Serial.println(" NTU");
  return true;
}

bool lerPH() {
  int phSamples[PH_SAMPLE_COUNT];
  for (int i = 0; i < PH_SAMPLE_COUNT; i++) {
    phSamples[i] = analogRead(PH_SENSOR_PIN);
    delay(PH_SAMPLE_INTERVAL_MS);
  }

  phRawValue = getMedianNum(phSamples, PH_SAMPLE_COUNT);
  phVoltage = phRawValue * (float)ADC_REFERENCE_VOLTAGE / ADC_MAX_VALUE;
  phSensorVoltage = phVoltage * PH_ADC_TO_SENSOR_VOLTAGE_FACTOR;
  if (phSensorVoltage < PH_CALIBRATION_VOLTAGE_7) {
    ph_act = 7.0 + ((phSensorVoltage - PH_CALIBRATION_VOLTAGE_7) * PH_CALIBRATION_SLOPE_ACID);
  } else {
    ph_act = 7.0 + ((phSensorVoltage - PH_CALIBRATION_VOLTAGE_7) * PH_CALIBRATION_SLOPE_ALKALINE);
  }

  if (phRawValue < 0 || phRawValue > ADC_MAX_VALUE || isnan(phVoltage) || isnan(phSensorVoltage) || isnan(ph_act) || ph_act < 0 || ph_act > 14) {
    Serial.println("Erro: leitura de pH invalida.");
    return false;
  }

  Serial.print("pH: ");
  Serial.print(ph_act, 2);
  Serial.print(" | raw: ");
  Serial.print(phRawValue);
  Serial.print(" | tensao ADC: ");
  Serial.print(phVoltage, 3);
  Serial.println(" V");
  return true;
}
