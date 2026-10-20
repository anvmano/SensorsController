static const char* FIREBASE_QUEUE_FILE = "/firebase-queue.jsonl";
static const char* FIREBASE_QUEUE_TEMP = "/firebase-queue.tmp";
static bool firebaseQueueReady = false;
static unsigned long firebaseRetryAt = 0;
static unsigned long firebaseRetryDelay = FIREBASE_QUEUE_SYNC_INTERVAL_MS;

static bool registroFilaValido(JsonDocument& registro) {
  if (!registro["tipo"].is<const char*>() || !registro["destino"].is<const char*>() ||
      !registro["conteudo"].is<const char*>() || !registro["timestamp"].is<const char*>() ||
      !registro["id"].is<const char*>() || !registro["metodo"].is<const char*>()) return false;
  String id = registro["id"].as<String>();
  return registro["metodo"] == "PUT" && id.length() > 0 && id.indexOf('/') < 0;
}

static bool filaValida(const char* caminho) {
  File fila = LittleFS.open(caminho, "r");
  if (!fila) return false;
  size_t tamanho = fila.size();
  if (tamanho == 0 || !fila.seek(tamanho - 1, SeekSet) || fila.read() != '\n' || !fila.seek(0, SeekSet)) {
    fila.close();
    return false;
  }
  while (fila.available()) {
    String linha = fila.readStringUntil('\n');
    JsonDocument registro;
    if (deserializeJson(registro, linha) || !registroFilaValido(registro)) {
      fila.close();
      return false;
    }
  }
  fila.close();
  return true;
}

bool inicializarFilaFirebase() {
  Preferences preferencias;
  if (!preferencias.begin("fbqueue", false)) {
    Serial.println("FALHA CRITICA: NVS indisponivel; fila Firebase nao pode ser inicializada.");
    return false;
  }
  bool filesystemInicializado = preferencias.getBool("fsready", false);
  bool montou = LittleFS.begin(false);
  if (!montou && (filesystemInicializado || !LittleFS.format() || !LittleFS.begin(false))) {
    preferencias.end();
    Serial.println("FALHA CRITICA: LittleFS indisponivel; fila Firebase nao pode ser usada.");
    return false;
  }
  if (!filesystemInicializado && preferencias.putBool("fsready", true) != 1) {
    LittleFS.end();
    preferencias.end();
    Serial.println("FALHA CRITICA: marcador de inicializacao LittleFS nao persistiu.");
    return false;
  }
  preferencias.end();
  firebaseQueueReady = true;

  if (LittleFS.exists(FIREBASE_QUEUE_TEMP)) {
    if (filaValida(FIREBASE_QUEUE_TEMP)) {
      LittleFS.remove(FIREBASE_QUEUE_FILE);
      if (!LittleFS.rename(FIREBASE_QUEUE_TEMP, FIREBASE_QUEUE_FILE)) {
        Serial.println("FALHA CRITICA: arquivo temporario valido nao pode ser recuperado.");
        firebaseQueueReady = false;
        return false;
      }
    } else {
      LittleFS.remove(FIREBASE_QUEUE_TEMP);
    }
  }
  Serial.printf("Fila Firebase: LittleFS pronta, limite %u bytes / %u itens.\n",
                FIREBASE_QUEUE_MAX_BYTES, FIREBASE_QUEUE_MAX_ITEMS);
  return true;
}

static bool copiarFila(String* novoItem, bool removerPrimeiro) {
  // shortcut: bounded FIFO is rewritten per change; use an append journal if write volume grows.
  File origem = LittleFS.open(FIREBASE_QUEUE_FILE, "r");
  File destino = LittleFS.open(FIREBASE_QUEUE_TEMP, "w");
  if (!destino) {
    if (origem) origem.close();
    return false;
  }

  size_t bytes = 0;
  unsigned int itens = 0;
  bool ok = true;
  bool primeiro = true;
  if (origem) {
    while (origem.available()) {
      String linha = origem.readStringUntil('\n');
      if (linha.length() == 0) continue;
      JsonDocument registro;
      if (deserializeJson(registro, linha) || !registroFilaValido(registro)) {
        ok = false;
        Serial.println("FALHA CRITICA: registro invalido na fila; arquivo preservado.");
        break;
      }
      if (removerPrimeiro && primeiro) {
        primeiro = false;
        continue;
      }
      primeiro = false;
      size_t linhaBytes = linha.length() + 1;
      if (bytes + linhaBytes > FIREBASE_QUEUE_MAX_BYTES || ++itens > FIREBASE_QUEUE_MAX_ITEMS ||
          destino.write((const uint8_t*)linha.c_str(), linha.length()) != linha.length() || destino.write('\n') != 1) {
        ok = false;
        break;
      }
      bytes += linhaBytes;
    }
    origem.close();
  }

  if (ok && novoItem) {
    size_t linhaBytes = novoItem->length() + 1;
    if (bytes + linhaBytes > FIREBASE_QUEUE_MAX_BYTES || ++itens > FIREBASE_QUEUE_MAX_ITEMS ||
        destino.write((const uint8_t*)novoItem->c_str(), novoItem->length()) != novoItem->length() || destino.write('\n') != 1) {
      ok = false;
    }
  }
  destino.flush();
  destino.close();

  if (!ok || !LittleFS.rename(FIREBASE_QUEUE_TEMP, FIREBASE_QUEUE_FILE)) {
    LittleFS.remove(FIREBASE_QUEUE_TEMP);
    return false;
  }
  return true;
}

static bool enviarRegistroFila(JsonDocument& registro) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Firebase pendente: Wi-Fi desconectado.");
    return false;
  }

  String destino = registro["destino"].as<String>() + "/" + registro["id"].as<String>();
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(FIREBASE_HTTP_TIMEOUT_MS);
  if (!http.begin(client, montarUrlFirebase(destino))) {
    Serial.println("Firebase pendente: nao foi possivel iniciar HTTP.");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  int statusCode = http.PUT(registro["conteudo"].as<String>());
  http.end();
  if (statusCode == HTTP_CODE_OK) {
    Serial.printf("Firebase confirmado [%s]: %s\n", registro["tipo"].as<const char*>(), destino.c_str());
    return true;
  }
  Serial.printf("Firebase pendente [%s]: %s - HTTP %d\n", registro["tipo"].as<const char*>(), destino.c_str(), statusCode);
  return false;
}

void processarFilaFirebase() {
  if (!LittleFS.exists(FIREBASE_QUEUE_FILE) || millis() - firebaseRetryAt >= firebaseRetryDelay) {
    if (!LittleFS.exists(FIREBASE_QUEUE_FILE)) return;
  } else {
    return;
  }
  firebaseRetryAt = millis();

  File fila = LittleFS.open(FIREBASE_QUEUE_FILE, "r");
  if (!fila) {
    Serial.println("FALHA CRITICA: arquivo da fila existe, mas nao pode ser aberto.");
    return;
  }
  String linha = fila.readStringUntil('\n');
  fila.close();
  if (linha.length() == 0) return;

  JsonDocument registro;
  if (deserializeJson(registro, linha) || !registroFilaValido(registro)) {
    Serial.println("FALHA CRITICA: nao foi possivel ler a primeira operacao da fila.");
    return;
  }
  if (enviarRegistroFila(registro)) {
    if (!copiarFila(nullptr, true)) {
      Serial.println("FALHA CRITICA: envio confirmado, mas fila nao pode ser atualizada; PUT idempotente sera repetido.");
      return;
    }
    firebaseRetryDelay = FIREBASE_QUEUE_SYNC_INTERVAL_MS;
    Serial.println("Operacao removida da fila apos confirmacao.");
  } else {
    firebaseRetryDelay = firebaseRetryDelay < FIREBASE_QUEUE_BACKOFF_MAX_MS / 2
                             ? firebaseRetryDelay * 2
                             : FIREBASE_QUEUE_BACKOFF_MAX_MS;
  }
}

bool enfileirarEEnviarFirebase(const String& destino, const String& conteudo, const String& id, const char* tipo) {
  if (!firebaseQueueReady) {
    Serial.printf("FALHA CRITICA: sem armazenamento local para %s em %s.\n", tipo, destino.c_str());
    return false;
  }

  JsonDocument registro;
  registro["tipo"] = tipo;
  registro["destino"] = destino;
  registro["conteudo"] = conteudo;
  int horaSeparador = destino.lastIndexOf('/');
  int dataSeparador = destino.lastIndexOf('/', horaSeparador - 1);
  registro["timestamp"] = destino.substring(dataSeparador + 1);
  registro["id"] = id;
  registro["metodo"] = "PUT";
  String linha;
  serializeJson(registro, linha);
  if (!copiarFila(&linha, false)) {
    Serial.printf("FALHA CRITICA: fila cheia ou gravacao LittleFS falhou; %s em %s nao foi armazenado.\n",
                  tipo, destino.c_str());
    return false;
  }

  Serial.printf("Firebase enfileirado [%s]: %s (%u bytes).\n", tipo, destino.c_str(), linha.length());
  return true;
}
