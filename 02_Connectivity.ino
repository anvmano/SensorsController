bool conectarWiFi() {
  unsigned long startTime = millis();

  WiFi.hostname("SensorsController");
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime >= WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println("\nErro: timeout ao conectar no WiFi.");
      return false;
    }
    delay(1000);
    Serial.print(".");
  }
  Serial.print("\n");
  Serial.print("Connected to WiFi: ");
  Serial.println(WiFi.localIP());
  return true;
}
