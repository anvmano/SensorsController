bool configurarBME680() {
  unsigned long startTime = millis();

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.println("Tentando inicializar BME680...");
  while (!bme680.begin()) {
    if (millis() - startTime >= BME680_SETUP_TIMEOUT_MS) {
      Serial.println("Erro: timeout ao inicializar BME680.");
      return false;
    }
    Serial.println(".");
    delay(1000);
  }

  bme680.setTemperatureOversampling(BME680_OS_8X);
  bme680.setHumidityOversampling(BME680_OS_2X);
  bme680.setPressureOversampling(BME680_OS_4X);
  Serial.println("BME680 inicializado.");
  return true;
}

bool lerBME680() {
  if (!bme680Disponivel || !bme680.performReading()) {
    return false;
  }

  float temperaturaLida = bme680.temperature;
  float umidadeLida = bme680.humidity;
  float pressaoLida = bme680.pressure;

  if (temperaturaLida < -40 || temperaturaLida > 85 || umidadeLida < 0 || umidadeLida > 100) {
    return false;
  }

  temperaturaBME680 = temperaturaLida;
  umidadeBME680 = umidadeLida;
  sensacaoTermicaBME680 = calcularSensacaoTermica(temperaturaBME680, umidadeBME680);
  pressaoBME680 = pressaoLida / 100.0;
  return true;
}

float calcularSensacaoTermica(float temperatura, float umidade) {
  float temperaturaF = (temperatura * 9.0 / 5.0) + 32.0;
  float umidadeRelativa = umidade;
  float sensacaoF = temperaturaF;

  if (temperaturaF >= 80.0) {
    sensacaoF = -42.379 + 2.04901523 * temperaturaF + 10.14333127 * umidadeRelativa - 0.22475541 * temperaturaF * umidadeRelativa - 0.00683783 * pow(temperaturaF, 2) - 0.05481717 * pow(umidadeRelativa, 2) + 0.00122874 * pow(temperaturaF, 2) * umidadeRelativa + 0.00085282 * temperaturaF * pow(umidadeRelativa, 2) - 0.00000199 * pow(temperaturaF, 2) * pow(umidadeRelativa, 2);

    if (umidadeRelativa < 13 && temperaturaF >= 80.0 && temperaturaF <= 112.0) {
      sensacaoF -= ((13.0 - umidadeRelativa) / 4.0) * sqrt((17.0 - abs(temperaturaF - 95.0)) / 17.0);
    } else if (umidadeRelativa > 85.0 && temperaturaF >= 80.0 && temperaturaF <= 87.0) {
      sensacaoF += ((umidadeRelativa - 85.0) / 10.0) * ((87.0 - temperaturaF) / 5.0);
    }
  }

  return (sensacaoF - 32.0) * 5.0 / 9.0;
}

void configurarMQ135() {
  MQ135.setRegressionMethod(1);
  MQ135.init();
}

bool calibrarMQ135() {
  Serial.print("Calibrando MQ135...");
  float calcR0 = 0;
  for (int i = 1; i <= 10; i++) {
    MQ135.update();
    calcR0 += MQ135.calibrate(RATIO_MQ135_CLEAN_AIR);
    Serial.print(".");
  }
  MQ135.setR0(calcR0 / 10);
  Serial.println(" concluido.");

  if (isnan(calcR0) || isinf(calcR0)) {
    Serial.println("Erro: R0 invalido no MQ135. Verifique alimentacao e sinal analogico.");
    mq135Calibrado = false;
    return false;
  }
  if (calcR0 <= 0) {
    Serial.println("Erro: R0 zero ou negativo no MQ135. Verifique curto para GND.");
    mq135Calibrado = false;
    return false;
  }

  mq135Calibrado = true;
  return true;
}

bool lerMQ135() {
  if (!mq135Calibrado) {
    Serial.println("MQ135 ainda nao calibrado. Leituras de gases ignoradas.");
    return false;
  }

  MQ135.update();

  MQ135.setA(605.18);
  MQ135.setB(-3.937);
  CO = MQ135.readSensor();

  MQ135.setA(77.255);
  MQ135.setB(-3.18);
  Alcohol = MQ135.readSensor();

  MQ135.setA(110.47);
  MQ135.setB(-2.862);
  CO2 = MQ135.readSensor();

  MQ135.setA(44.947);
  MQ135.setB(-3.445);
  Toluen = MQ135.readSensor();

  MQ135.setA(102.2);
  MQ135.setB(-2.473);
  NH4 = MQ135.readSensor();

  MQ135.setA(34.668);
  MQ135.setB(-3.369);
  Aceton = MQ135.readSensor();
  return true;
}
