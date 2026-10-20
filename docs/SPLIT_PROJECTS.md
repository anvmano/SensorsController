# Divisao em dois firmwares

O projeto original `WebServer` foi separado em dois sketches Arduino independentes.

## SensorsController

Pasta: `SensorsController/`

Local atual: `SensorsController/`

Responsavel por leitura e historico:

- Wi-Fi;
- NTP;
- Firebase Realtime Database via REST;
- BME680 via `Adafruit_BME680`;
- MQ135;
- DS18B20;
- rele SSR do DS18B20/temperatura;
- TDS;
- pH;
- turbidez;
- gravacao em `/historico/AirQuality/...`;
- gravacao em `/historico/Aquario/...`.

Arquivos ativos:

- `SensorsController.ino`;
- `config.h`;
- `secrets.h`;
- `01_Utils.ino`;
- `02_Connectivity.ino`;
- `03_AirSensors.ino`;
- `04_AquariumSensors.ino`;
- `05_Firebase.ino`;
- `06_Tasks.ino`.

## LightingController

Pasta esperada: `LightingController/`

Responsavel por iluminacao:

- Wi-Fi;
- NTP;
- Firebase Realtime Database;
- leitura de horarios solares em `/historico/NascePorDoSol/DD-MM-AAAA`;
- fita FastLED/NeoPixel;
- rele da regua branca;
- curva diaria por amanhecer, nascer do sol, zenite, por do sol e anoitecer.

## Observacoes

- O `secrets.h` foi copiado para os sketches porque a IDE/CLI do Arduino normalmente espera includes dentro da pasta do sketch.
- O firmware de sensores nao contem codigo de LEDs, horarios solares ou fan. O unico rele nele e o SSR do DS18B20/temperatura.
- O rele de iluminacao/regua branca pertence ao `LightingController`, nao ao `SensorsController`.
- O firmware de iluminacao nao deve conter codigo de sensores, gravacao historica ou calibracao do MQ135.
- `Toluen` e gravado no historico de ar neste firmware.
- TDS, pH e turbidez estao ativos na gravacao de aquario neste firmware.
