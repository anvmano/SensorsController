void executarTarefasPeriodicas() {
  static unsigned long lastWiFiReconnectAttempt = 0;
  static unsigned long lastMQ135CalibrationAttempt = 0;
  static unsigned long lastNtpRetry = 0;
  static unsigned long lastSensorStatusAttempt = 0;
  static bool statusSensoresPosBootGravado = false;
  unsigned long currentMillis = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (currentMillis - lastWiFiReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS) {
      lastWiFiReconnectAttempt = currentMillis;
      conectarWiFi();
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update();
  }

  bool podeTentarCalibrarMQ135 = lastMQ135CalibrationAttempt == 0 || currentMillis - lastMQ135CalibrationAttempt >= MQ135_RETRY_CALIBRATION_MS;
  if (ENABLE_MQ135_AUTO_CALIBRATION && !mq135Calibrado && currentMillis >= MQ135_WARMUP_MS && podeTentarCalibrarMQ135) {
    lastMQ135CalibrationAttempt = currentMillis;
    calibrarMQ135();
  }

  if (!horarioSincronizado()) {
    if (WiFi.status() == WL_CONNECTED && currentMillis - lastNtpRetry >= WIFI_RECONNECT_INTERVAL_MS) {
      lastNtpRetry = currentMillis;
      sincronizarHorario();
    }
    processarFilaFirebase();
    return;
  }

  bool statusSensoresPendente = !statusSensoresPosBootGravado && currentMillis >= SENSOR_STATUS_BOOT_DELAY_MS;
  bool podeTentarStatusSensores = lastSensorStatusAttempt == 0 || currentMillis - lastSensorStatusAttempt >= SENSOR_STATUS_RETRY_INTERVAL_MS;
  if (statusSensoresPendente && podeTentarStatusSensores) {
    lastSensorStatusAttempt = currentMillis;
    statusSensoresPosBootGravado = gravarStatusSensores();
    processarFilaFirebase();
    return;
  }

  if (minute(timeClient.getEpochTime()) == HOURLY_RECORD_MINUTE) {
    gravarHistoricoAtual();
  }
  processarFilaFirebase();
}
