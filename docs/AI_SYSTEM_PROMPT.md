# AI_SYSTEM_PROMPT

Voce esta trabalhando no projeto `SensorsController`, um firmware Arduino para Wemos D1 R32/ESP32.

## Objetivo do sistema

Coletar dados de sensores ambientais e de aquario, sincronizar horario por NTP e gravar historico no Firebase Realtime Database usando REST.

Este projeto e somente o controlador de sensores. O controle de iluminacao foi separado para `LightingController`. O unico rele neste firmware e o SSR low-level trigger que energiza o sensor DS18B20/temperatura somente durante a leitura.

## Tecnologias

- Arduino/C++.
- ESP32 alvo historico `esp32:esp32:d1_uno32`.
- Firebase Realtime Database via `HTTPClient` e `WiFiClientSecure`.
- ArduinoJson.
- NTPClient.
- TimeLib.
- WiFi/WiFiUdp do core ESP32.
- Adafruit_BME680.
- MQUnifiedsensor.
- OneWire e DallasTemperature.

## Arquivos importantes

- `SensorsController.ino`: globais, `setup()` e `loop()`.
- `config.h`: constantes de hardware e agenda.
- `secrets.h`: credenciais sensiveis.
- `01_Utils.ino`: horario, NTP e caminhos Firebase.
- `02_Connectivity.ino`: Wi-Fi.
- `03_AirSensors.ino`: BME680, MQ135, sensacao termica.
- `04_AquariumSensors.ino`: DS18B20, TDS, turbidez e pH.
- `05_Firebase.ino`: gravacao no Firebase via REST.
- `06_Tasks.ino`: tarefas periodicas.

## Fluxos principais

Boot:

1. Serial.
2. ADC 12 bits.
3. Atenuacao `ADC_11db` no pino de pH.
4. Rele da temperatura desligado.
5. MQ135.
6. BME680.
7. Wi-Fi.
8. NTP.
9. Sincronizacao de horario.
10. Nao grava historico inicial no boot.

Loop:

1. Reconectar Wi-Fi se necessario.
2. Atualizar NTP se conectado.
3. Calibrar MQ135 apos warmup, se auto-calibracao estiver ativa.
4. Se horario invalido, tentar NTP e sair.
5. Apos 3 minutos de boot, gravar uma validacao unica em `/statusSensores/DD-MM-AAAA/HH-MM/SensorsController_boot_<millis>`.
6. Gravar historico no minuto configurado.

Na gravacao horaria, AirQuality e lido/gravado antes de Aquario. O fluxo de Aquario le pH, espera 10 s, le turbidez, espera 10 s, le DS18B20 com rele, espera 10 s, le TDS e so depois grava o JSON no Firebase.

## Regras criticas

- Nao gravar historico se o horario NTP nao estiver valido.
- Manter caminhos Firebase:
  - `/historico/AirQuality/DD-MM-AAAA/HH-MM/SensorsController_AirQuality`
  - `/historico/Aquario/DD-MM-AAAA/HH-MM/SensorsController_Aquario`
- Validacao pos-boot deve ficar fora de `/historico`, em `/statusSensores/DD-MM-AAAA/HH-MM/SensorsController_boot_<millis>`; o objeto inclui `status_de_cada_sensor`.
- Todas as escritas devem passar pela fila persistente LittleFS. O `PUT` com ID deterministico evita duplicatas em retry.
- Limite da fila: 64 KiB/100 operacoes; ao lotar, recusar nova operacao com log critico e manter as pendentes.
- Nao alterar nomes de campos Firebase sem avaliar consumidores externos.
- MQ135 so deve gerar gases quando calibrado.
- BME680 ausente nao deve travar boot.
- DS18B20 desconectado nao deve gravar temperatura invalida.
- pH deve ser lido antes do TDS por sensibilidade a interferencia eletrica.
- Rele SSR da temperatura/DS18B20 e low-level trigger: `LOW` liga, `HIGH` desliga.
- Respeitar ADC ESP32 com limite fisico de 3.3 V.

## Nunca altere sem necessidade

- Credenciais em `secrets.h`.
- Estrutura dos caminhos Firebase.
- Nomes de campos ja gravados no Firebase.
- Pinos em `config.h` sem revisar hardware.
- Protecao contra duplicidade por minuto.
- Validacoes de sensores e omissao de campos invalidos.

## Pontos de atencao

- `secrets.h` contem dados sensiveis.
- O codigo usa `client.setInsecure()` para HTTPS sem validacao de certificado.
- Firebase e acessado pela URL de `FIREBASE_HOST` e auth token de `FIREBASE_AUTH`.
- TDS, turbidez e pH estao ativos na gravacao atual.
- DS18B20 usa rele em GPIO25; ao ler, liga, espera 3 s, detecta o sensor ate 5 vezes, descarta a primeira leitura e tenta ler ate 5 vezes.
- TDS nao e chaveado por rele no firmware atual; faz media de 100 leituras.
- pH usa `analogRead()` com 100 amostras, 20 ms entre amostras, `ADC_11db` no pino A4/GPIO36 e calibracao de tres pontos.
- `Toluen` e calculado pelo MQ135 e gravado em AirQuality.
- Nao ha codigo de LED, fan, horarios solares ou FastLED neste projeto.

## Estrategia recomendada para alteracoes

1. Ler `config.h` e o arquivo da area alterada.
2. Verificar dependencias em `SensorsController.ino`.
3. Preservar campos e caminhos Firebase, salvo solicitacao explicita.
4. Conferir se a mudanca depende de horario NTP valido.
5. Conferir limites eletricos dos sensores analogicos.
6. Compilar com warnings.

## Checklist antes de finalizar alteracoes

- Compilou com `arduino-cli compile --warnings all --fqbn esp32:esp32:d1_uno32`?
- `setup()` e `loop()` continuam validos?
- Nao houve exposicao de segredo?
- Caminhos Firebase foram preservados?
- Campos antigos do Firebase foram preservados?
- Historico continua bloqueado quando horario esta invalido?
- Duplicidade por categoria/minuto continua protegida?
