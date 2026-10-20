String obterDataFormatada() {
  char buf[11];
  sprintf(buf, "%02d-%02d-%04d", day(timeClient.getEpochTime()), month(timeClient.getEpochTime()), year(timeClient.getEpochTime()));
  return String(buf);
}

String obterHoraFormatada() {
  char buf[6];
  sprintf(buf, "%02d-%02d", hour(timeClient.getEpochTime()), minute(timeClient.getEpochTime()));
  return String(buf);
}

bool horarioSincronizado() {
  return year(timeClient.getEpochTime()) >= MIN_VALID_YEAR;
}

bool sincronizarHorario() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Horario nao sincronizado: Wi-Fi desconectado.");
    return false;
  }

  bool sincronizou = timeClient.forceUpdate();
  if (!sincronizou || !horarioSincronizado()) {
    Serial.println("Horario nao sincronizado: falha ao consultar NTP.");
    return false;
  }

  Serial.print("Horario sincronizado: ");
  Serial.print(obterDataFormatada());
  Serial.print(" ");
  Serial.println(obterHoraFormatada());
  return true;
}

String montarCaminhoHistorico(const String& categoria) {
  return "/historico/" + categoria + "/" + obterDataFormatada() + "/" + obterHoraFormatada();
}

String montarCaminhoStatusSensores() {
  return "/statusSensores/" + obterDataFormatada() + "/" + obterHoraFormatada();
}

String obterChaveHistoricoAtual() {
  return obterDataFormatada() + " " + obterHoraFormatada();
}
