# AI_QUICK_CONTEXT

## O que e o sistema

`SensorsController` e um firmware Arduino para Wemos D1 R32/ESP32. Ele coleta sensores de qualidade do ar e de aquario, sincroniza horario via NTP e grava historico no Firebase Realtime Database por HTTP REST.

Local atual do projeto: `SensorsController/`.

Este projeto e uma das duas partes do firmware original. A iluminacao do aquario foi separada para o projeto `LightingController`; aqui nao ha controle de LEDs, horarios solares ou fan. Existe apenas um rele SSR low-level trigger para energizar o sensor DS18B20/temperatura durante a leitura.

## Tecnologias

- Arduino/C++ em abas `.ino`.
- Placa alvo historica: `esp32:esp32:d1_uno32`.
- Wi-Fi via core ESP32.
- Firebase Realtime Database via REST (`HTTPClient` + `WiFiClientSecure`).
- Bibliotecas verificadas no Arduino CLI: ArduinoJson 7.4.3, NTPClient 3.2.1, TimeLib (pacote Time) 1.6.1, Adafruit BME680 2.0.6, MQUnifiedsensor 3.0.0, OneWire 2.3.8 e DallasTemperature 4.0.6. WiFi, HTTPClient, LittleFS e Preferences pertencem ao core ESP32 2.0.14.
- JSON via `ArduinoJson`.
- NTP via `NTPClient`.
- Data/hora via `TimeLib`.
- I2C via `Wire`.
- BME680 via `Adafruit_BME680`.
- MQ135 via `MQUnifiedsensor`.
- DS18B20 via `OneWire` e `DallasTemperature`.

## Arquitetura Resumida

Arquivos ativos:

- `SensorsController.ino`: includes, objetos globais, variaveis, `setup()` e `loop()`.
- `config.h`: pinos, limites eletricos, tempos, parametros de sensores e agenda.
- `secrets.h`: credenciais de Wi-Fi e Firebase.
- `01_Utils.ino`: data/hora, validacao NTP e caminhos Firebase.
- `02_Connectivity.ino`: conexao Wi-Fi.
- `03_AirSensors.ino`: BME680, MQ135 e sensacao termica.
- `04_AquariumSensors.ino`: DS18B20, TDS, turbidez e pH.
- `05_Firebase.ino`: montagem de JSONs e escrita REST no Firebase.
- `06_Tasks.ino`: agenda periodica do loop.
- `07_FirebaseQueue.ino`: fila LittleFS FIFO, PUT idempotente e backoff.

## Fluxo Principal

No `setup()`:

1. Inicia Serial.
2. Configura ADC em 12 bits.
3. Configura atenuacao `ADC_11db` para o pH.
4. Inicializa o rele SSR da temperatura desligado.
5. Inicializa MQ135.
6. Inicializa BME680 com timeout.
7. Conecta Wi-Fi.
8. Inicializa NTP com UTC-3.
9. Sincroniza horario.

O `setup()` nao grava historico inicial. A primeira gravacao de historico acontece pela agenda periodica no `loop()`.

No `loop()`:

- `executarTarefasPeriodicas()` reconecta Wi-Fi, atualiza NTP, calibra MQ135 apos warmup e grava historico no minuto configurado.
- Apos 3 minutos de boot, com horario valido, grava uma validacao unica em `/statusSensores/DD-MM-AAAA/HH-MM/SensorsController_boot_<millis>`.
- AirQuality e gravado antes de Aquario.
- Aquario e lido em ordem controlada: pH, turbidez, DS18B20 e TDS, com 10 s entre sensores antes de gravar no Firebase.

## Firebase

Escrita:

- `/historico/AirQuality/DD-MM-AAAA/HH-MM/SensorsController_AirQuality`
- `/historico/Aquario/DD-MM-AAAA/HH-MM/SensorsController_Aquario`
- `/statusSensores/DD-MM-AAAA/HH-MM/SensorsController_boot_<millis>`

As escritas sao persistidas em `/firebase-queue.jsonl` (LittleFS) e sincronizadas em FIFO via `PUT`, com ID filho deterministico. Limite: 64 KiB/100 itens; 30 s a 1 h de backoff; uma operacao por ciclo.
O hostname Wi-Fi e `SensorsController`. Nao ha codigo do consumidor Firebase neste repositorio, portanto a dependencia de chaves push precisa ser verificada externamente. No primeiro boot, uma particao LittleFS sem formato e formatada uma vez; NVS impede formatacao automatica apos falhas futuras.

Campos AirQuality:

- `bme680Ok`, `mq135Ok`
- quando BME680 valido: `temperatura`, `sensacaoTermica`, `pressao`, `umidade`
- quando MQ135 calibrado: `Aceton`, `Alcohol`, `CO`, `CO2`, `NH4`, `Toluen`

Campos Aquario:

- `ds18b20Ok`, `tdsOk`, `turbidezOk`, `phOk`
- quando DS18B20 valido: `temperaturaDS18B20`
- quando TDS valido: `TDS`, `tdsPpm`, `tdsMgL`, `tdsRaw`, `tdsVoltage`, `tdsCompensationVoltage`, `tdsCompensationTemperature`
- quando turbidez valida: `Turbidez`, `turbidezNtu`, `turbidezRaw`, `turbidezVoltage`, `turbidezSensorVoltage`
- quando pH valido: `PH`, `phRaw`, `phVoltage`, `phSensorVoltage`

## Pinos

- BME680: SDA 21, SCL 22.
- MQ135: A3 / GPIO34.
- DS18B20: GPIO17.
- Rele SSR da temperatura/DS18B20: GPIO25, low-level trigger (`LOW` liga, `HIGH` desliga).
- TDS: A2 / GPIO35.
- Turbidez: A5 / GPIO39.
- pH: A4 / GPIO36.

## Fluxos Criticos

- NTP precisa estar valido (`year >= MIN_VALID_YEAR`) antes de gravar historico.
- Caminhos historicos usam `DD-MM-AAAA/HH-MM`.
- Gravacao REST exige Wi-Fi conectado.
- MQ135 so grava gases quando `mq135Calibrado` e verdadeiro.
- BME680 ausente nao bloqueia boot; apenas grava `bme680Ok = false`.
- DS18B20 fica desligado em repouso; `lerDS18B20()` liga o rele, espera 3 s, tenta detectar o sensor ate `DS18B20_DETECTION_ATTEMPTS`, descarta a primeira leitura e tenta ler ate `DS18B20_READ_ATTEMPTS`.
- StatusSensores pos-boot nao substitui o historico; serve apenas para validacao inicial fora de `/historico`.
- pH e sensivel a interferencia eletrica; e lido antes do TDS.
- TDS nao e chaveado por rele no firmware atual; faz media de 100 leituras e usa temperatura DS18B20 ou fallback de 25 C para compensacao.
- Duplicidade por minuto e controlada separadamente para AirQuality e Aquario.
- Entradas analogicas do ESP32 devem respeitar 3.3 V.

## Resumo para IA

Antes de modificar, identifique se a alteracao e de sensor, Firebase, horario ou conectividade. Preserve os caminhos Firebase e os nomes de campos ja gravados. Nao exponha credenciais de `secrets.h`. Compile com `arduino-cli compile --warnings all --fqbn esp32:esp32:d1_uno32` quando houver mudanca de codigo.
