# SensorsController

Firmware Arduino para **WEMOS D1 R32 (ESP32)** que coleta sensores de qualidade do ar e de aquário, sincroniza o relógio via NTP e grava os dados no Firebase Realtime Database por REST. As operações pendentes ficam em uma fila persistente LittleFS, com reenvio por `PUT`.

## Hardware identificado no código

A placa é indicada por `BOARD_NAME` em `config.h`; o alvo Arduino documentado no projeto é `esp32:esp32:d1_uno32`.

| Componente | Uso | Pinos configurados |
| --- | --- | --- |
| BME680 | Temperatura, umidade e pressão do ar | SDA 21, SCL 22 |
| MQ135 | Estimativas de gases após calibração | A3 |
| DS18B20 | Temperatura da água, via OneWire | GPIO17 |
| Relé SSR do DS18B20 | Alimentação durante a leitura; `LOW` liga | GPIO25 |
| Sensor TDS | Sólidos dissolvidos, com compensação de temperatura | A2 |
| Sensor de turbidez | Leitura analógica e estimativa de NTU | A5 |
| Sensor de pH | Leitura analógica com calibração em três pontos | A4 |

Os modelos dos módulos TDS, turbidez, pH e relé não estão especificados no código. A tabela reproduz a configuração do firmware; não é um esquema elétrico. As entradas ADC do ESP32 devem respeitar o limite de 3,3 V. Os pinos, tempos e calibrações existentes foram preservados em `config.h`.

## Bibliotecas

Ambiente local identificado (versões instaladas):

| Pacote | Versão |
| --- | --- |
| Core ESP32 (`esp32:esp32`) | 2.0.14 |
| Adafruit BME680 Library | 2.0.6 |
| Adafruit BusIO | 1.17.4 |
| Adafruit Unified Sensor | 1.1.15 |
| ArduinoJson | 7.4.3 |
| DallasTemperature | 4.0.6 |
| MQUnifiedsensor | 3.0.0 |
| NTPClient | 3.2.1 |
| OneWire | 2.3.8 |
| Time (`TimeLib.h`) | 1.6.1 |

`Wire`, `WiFi`, `HTTPClient`, `WiFiClientSecure`, `WiFiUdp`, `LittleFS` e `Preferences` fazem parte do core ESP32. O acesso ao Firebase usa REST e não depende de uma biblioteca cliente Firebase.

## Configuração local

1. Clone este repositório em uma pasta chamada `SensorsController`, para corresponder ao sketch `SensorsController.ino`.
2. Copie `secrets.example.h` para `secrets.h` na raiz. No PowerShell:

   ```powershell
   Copy-Item secrets.example.h secrets.h
   ```

3. Edite somente os valores de `secrets.h`, mantendo as declarações:

   | Identificador | Valor a configurar |
   | --- | --- |
   | `ssid` | Nome da sua rede Wi-Fi |
   | `password` | Senha da sua rede Wi-Fi |
   | `FIREBASE_HOST` | URL HTTPS do seu Firebase Realtime Database, incluindo o domínio e a barra final |
   | `FIREBASE_AUTH` | Token aceito pelo seu banco para autenticação REST |

   O exemplo contém apenas placeholders. `secrets.h`, arquivos `.env`, chaves privadas e configurações locais estão no `.gitignore`. Mantenha os valores reais somente no ambiente local; não os inclua em comentários, documentação ou exemplos.
4. Configure o banco e a autorização para permitir as escritas necessárias. Os caminhos usados são `/historico/AirQuality/...`, `/historico/Aquario/...` e `/statusSensores/...`.

O firmware mantém o comportamento original de usar `client.setInsecure()` no HTTPS e o token na query string. Isso desativa a validação do certificado do servidor; evite registrar ou compartilhar URLs autenticadas. A publicação não altera esse comportamento.

## Compilação

Na IDE Arduino, instale o pacote de placas ESP32, selecione **WEMOS D1 R32**, instale as bibliotecas acima e abra `SensorsController.ino` junto com as demais abas na mesma pasta.

Com Arduino CLI, depois de configurar o índice do pacote ESP32:

```powershell
arduino-cli core install esp32:esp32@2.0.14
arduino-cli lib install "Adafruit BME680 Library@2.0.6" "Adafruit BusIO@1.17.4" "Adafruit Unified Sensor@1.1.15" "ArduinoJson@7.4.3" "DallasTemperature@4.0.6" "MQUnifiedsensor@3.0.0" "NTPClient@3.2.1" "OneWire@2.3.8" "Time@1.6.1"
arduino-cli compile --warnings all --fqbn esp32:esp32:d1_uno32 .
```

Execute os comandos na pasta `SensorsController`, com `secrets.h` já criado. Compilar não exige uma placa conectada; gravar e validar sensores, Wi-Fi e Firebase exige o hardware e credenciais locais válidas. O monitor serial usa 9600 baud.

## Organização

- `SensorsController.ino`: bibliotecas, globais, `setup()` e `loop()`.
- `config.h`: pinagem, calibrações e parâmetros de execução.
- `01_Utils.ino` a `06_Tasks.ino`: horário, conectividade, sensores, Firebase e agenda.
- `07_FirebaseQueue.ino`: fila persistente LittleFS e reenvio.
- `secrets.example.h`: modelo público sem credenciais reais.
- `docs/`: documentação de arquitetura, dependências e regras do firmware.

O relógio precisa estar sincronizado para as gravações de histórico. O MQ135 é calibrado após aquecimento; o pH é lido antes do TDS. A fila tem os limites definidos em `config.h` e usa uma partição de filesystem; o primeiro uso pode formatá-la conforme a lógica já existente no firmware.
