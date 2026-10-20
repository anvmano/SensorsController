String montarUrlFirebase(const String& path) {
  String baseUrl = FIREBASE_HOST;
  if (!baseUrl.endsWith("/")) {
    baseUrl += "/";
  }

  String cleanPath = path;
  if (cleanPath.startsWith("/")) {
    cleanPath.remove(0, 1);
  }

  return baseUrl + cleanPath + ".json?auth=" + FIREBASE_AUTH;
}

bool enviarJsonFirebase(const String& path, JsonDocument& json, const char* descricao) {
  String payload;
  serializeJson(json, payload);
  String id = String(descricao) == "StatusSensores"
                  ? "SensorsController_boot_" + String(millis())
                  : "SensorsController_" + String(descricao);
  return enfileirarEEnviarFirebase(path, payload, id, descricao);
}

bool gravarFirebaseAquario() {
  String pathAquario = montarCaminhoHistorico("Aquario");

  bool phOk = lerPH();
  delay(AQUARIUM_SENSOR_INTERVAL_MS);

  bool turbidezOk = lerTurbidez();
  delay(AQUARIUM_SENSOR_INTERVAL_MS);

  bool ds18b20Ok = lerDS18B20();
  delay(AQUARIUM_SENSOR_INTERVAL_MS);

  float temperaturaCompensacaoTDS = ds18b20Ok ? temperaturaDS18B20 : 25.0;
  bool tdsOk = lerTDS(temperaturaCompensacaoTDS);

  JsonDocument jsonAquario;
  jsonAquario["ds18b20Ok"] = ds18b20Ok;
  jsonAquario["tdsOk"] = tdsOk;
  jsonAquario["turbidezOk"] = turbidezOk;
  jsonAquario["phOk"] = phOk;

  if (ds18b20Ok) {
    jsonAquario["temperaturaDS18B20"] = temperaturaDS18B20;
  }

  if (tdsOk) {
    jsonAquario["TDS"] = tdsValue;
    jsonAquario["tdsPpm"] = tdsValue;
    jsonAquario["tdsMgL"] = tdsValue;
    jsonAquario["tdsRaw"] = tdsRawValue;
    jsonAquario["tdsVoltage"] = averageVoltage;
    jsonAquario["tdsCompensationVoltage"] = tdsCompensationVoltage;
    jsonAquario["tdsCompensationTemperature"] = tdsCompensationTemperature;
  }

  if (turbidezOk) {
    jsonAquario["Turbidez"] = turbidityNtu;
    jsonAquario["turbidezNtu"] = turbidityNtu;
    jsonAquario["turbidezRaw"] = turbidity;
    jsonAquario["turbidezVoltage"] = turbidityVoltage;
    jsonAquario["turbidezSensorVoltage"] = turbiditySensorVoltage;
  }

  if (phOk) {
    jsonAquario["PH"] = ph_act;
    jsonAquario["phRaw"] = phRawValue;
    jsonAquario["phVoltage"] = phVoltage;
    jsonAquario["phSensorVoltage"] = phSensorVoltage;
  }

  return enviarJsonFirebase(pathAquario, jsonAquario, "Aquario");
}

bool gravarFirebaseAr() {
  String pathAirQuality = montarCaminhoHistorico("AirQuality");

  bool bme680Ok = lerBME680();
  bool mq135Ok = lerMQ135();
  JsonDocument jsonAirQuality;

  jsonAirQuality["bme680Ok"] = bme680Ok;
  jsonAirQuality["mq135Ok"] = mq135Ok;

  if (bme680Ok) {
    jsonAirQuality["temperatura"] = temperaturaBME680;
    jsonAirQuality["sensacaoTermica"] = sensacaoTermicaBME680;
    jsonAirQuality["pressao"] = pressaoBME680;
    jsonAirQuality["umidade"] = umidadeBME680;
  }

  if (mq135Ok) {
    jsonAirQuality["Aceton"] = Aceton;
    jsonAirQuality["Alcohol"] = Alcohol;
    jsonAirQuality["CO"] = CO;
    jsonAirQuality["CO2"] = CO2;
    jsonAirQuality["NH4"] = NH4;
    jsonAirQuality["Toluen"] = Toluen;
  }

  return enviarJsonFirebase(pathAirQuality, jsonAirQuality, "AirQuality");
}

bool gravarStatusSensores() {
  String pathStatus = montarCaminhoStatusSensores();

  bool bme680Ok = lerBME680();
  bool mq135Ok = lerMQ135();

  bool phOk = lerPH();
  delay(AQUARIUM_SENSOR_INTERVAL_MS);

  bool turbidezOk = lerTurbidez();
  delay(AQUARIUM_SENSOR_INTERVAL_MS);

  bool ds18b20Ok = lerDS18B20();
  delay(AQUARIUM_SENSOR_INTERVAL_MS);

  float temperaturaCompensacaoTDS = ds18b20Ok ? temperaturaDS18B20 : 25.0;
  bool tdsOk = lerTDS(temperaturaCompensacaoTDS);

  JsonDocument jsonStatus;
  jsonStatus["tipo"] = "validacao_pos_boot";
  jsonStatus["uptimeMs"] = millis();

  JsonObject status = jsonStatus["status_de_cada_sensor"].to<JsonObject>();

  JsonObject bme680Status = status["BME680"].to<JsonObject>();
  bme680Status["ok"] = bme680Ok;
  if (bme680Ok) {
    bme680Status["temperatura"] = temperaturaBME680;
    bme680Status["sensacaoTermica"] = sensacaoTermicaBME680;
    bme680Status["pressao"] = pressaoBME680;
    bme680Status["umidade"] = umidadeBME680;
  }

  JsonObject mq135Status = status["MQ135"].to<JsonObject>();
  mq135Status["ok"] = mq135Ok;
  mq135Status["calibrado"] = mq135Calibrado;
  if (mq135Ok) {
    mq135Status["Aceton"] = Aceton;
    mq135Status["Alcohol"] = Alcohol;
    mq135Status["CO"] = CO;
    mq135Status["CO2"] = CO2;
    mq135Status["NH4"] = NH4;
    mq135Status["Toluen"] = Toluen;
  }

  JsonObject ds18b20Status = status["DS18B20"].to<JsonObject>();
  ds18b20Status["ok"] = ds18b20Ok;
  if (ds18b20Ok) {
    ds18b20Status["temperaturaDS18B20"] = temperaturaDS18B20;
  }

  JsonObject phStatus = status["PH"].to<JsonObject>();
  phStatus["ok"] = phOk;
  if (phOk) {
    phStatus["PH"] = ph_act;
    phStatus["phRaw"] = phRawValue;
    phStatus["phVoltage"] = phVoltage;
    phStatus["phSensorVoltage"] = phSensorVoltage;
  }

  JsonObject turbidezStatus = status["Turbidez"].to<JsonObject>();
  turbidezStatus["ok"] = turbidezOk;
  if (turbidezOk) {
    turbidezStatus["Turbidez"] = turbidityNtu;
    turbidezStatus["turbidezRaw"] = turbidity;
    turbidezStatus["turbidezVoltage"] = turbidityVoltage;
    turbidezStatus["turbidezSensorVoltage"] = turbiditySensorVoltage;
  }

  JsonObject tdsStatus = status["TDS"].to<JsonObject>();
  tdsStatus["ok"] = tdsOk;
  if (tdsOk) {
    tdsStatus["TDS"] = tdsValue;
    tdsStatus["tdsRaw"] = tdsRawValue;
    tdsStatus["tdsVoltage"] = averageVoltage;
    tdsStatus["tdsCompensationVoltage"] = tdsCompensationVoltage;
    tdsStatus["tdsCompensationTemperature"] = tdsCompensationTemperature;
  }

  return enviarJsonFirebase(pathStatus, jsonStatus, "StatusSensores");
}

bool gravarHistoricoAtual() {
  static String ultimaTentativaAr;
  static String ultimaTentativaAquario;
  String chaveAtual = obterChaveHistoricoAtual();
  bool arOk = false;
  bool aquarioOk = false;
  bool arJaGravado = chaveAtual == ultimaChaveHistoricoArGravada;
  bool aquarioJaGravado = chaveAtual == ultimaChaveHistoricoAquarioGravada;
  bool arJaTentado = arJaGravado || chaveAtual == ultimaTentativaAr;
  bool aquarioJaTentado = aquarioJaGravado || chaveAtual == ultimaTentativaAquario;

  if (arJaTentado && aquarioJaTentado) {
    Serial.println("Historico ja processado para este minuto. Ignorando duplicado.");
    return arJaGravado && aquarioJaGravado;
  }

  if (!arJaTentado) {
    ultimaTentativaAr = chaveAtual;
    arOk = gravarFirebaseAr();
    if (arOk) {
      ultimaChaveHistoricoArGravada = chaveAtual;
    }
  } else {
    arOk = arJaGravado;
  }

  if (!aquarioJaTentado) {
    ultimaTentativaAquario = chaveAtual;
    aquarioOk = gravarFirebaseAquario();
    if (aquarioOk) {
      ultimaChaveHistoricoAquarioGravada = chaveAtual;
    }
  } else {
    aquarioOk = aquarioJaGravado;
  }

  return arOk && aquarioOk;
}
