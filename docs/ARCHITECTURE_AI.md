# ARCHITECTURE_AI

## Visao Geral

`SensorsController` e o firmware de sensores do sistema. Ele roda em uma Wemos D1 R32/ESP32, coleta dados de qualidade do ar e de aquario, valida leituras e grava historico no Firebase Realtime Database.

O codigo atual contem:

- leitura de BME680 para temperatura, umidade e pressao;
- calculo de sensacao termica a partir de temperatura e umidade;
- leitura de MQ135 para gases apos calibracao;
- leitura de DS18B20 para temperatura da agua com rele SSR, deteccao e retries;
- leitura ativa de TDS com compensacao por temperatura;
- leitura ativa de turbidez com conversao estimada para NTU;
- leitura ativa de pH com calibracao de tres pontos;
- sincronizacao de horario por NTP;
- gravacao no Firebase via REST `PUT` idempotente;
- validacao pos-boot dos sensores fora de `/historico`.

Nao ha interface local, display, servidor HTTP local, controle de LEDs, fan, horarios solares, FastLED, testes automatizados, banco SQL, React, Angular, Vue, C# ou .NET neste projeto. O unico rele e o SSR low-level trigger do DS18B20/temperatura.

## Estrutura Fisica

Local atual do projeto:

```text
SensorsController/
```

```text
SensorsController/
  SensorsController.ino
  config.h
  secrets.h
  01_Utils.ino
  02_Connectivity.ino
  03_AirSensors.ino
  04_AquariumSensors.ino
  05_Firebase.ino
  06_Tasks.ino
  07_FirebaseQueue.ino
```

Responsabilidades:

- `SensorsController.ino`: arquivo principal do sketch. Declara includes, objetos globais, variaveis globais, `setup()` e `loop()`.
- `config.h`: constantes de placa, ADC, pinos, tempos, parametros de sensores e agenda.
- `secrets.h`: credenciais de Wi-Fi e Firebase. Nao deve ser publicado.
- `01_Utils.ino`: formatacao de data/hora, montagem de caminhos Firebase e validacao/sincronizacao NTP.
- `02_Connectivity.ino`: conexao Wi-Fi com timeout.
- `03_AirSensors.ino`: BME680 e MQ135.
- `04_AquariumSensors.ino`: DS18B20, TDS, turbidez e pH.
- `05_Firebase.ino`: montagem de JSONs e gravacao REST no Firebase.
- `06_Tasks.ino`: orquestracao periodica no `loop()`.
- `07_FirebaseQueue.ino`: fila FIFO persistente LittleFS e envio idempotente.
- Hostname Wi-Fi: `SensorsController`, configurado em `02_Connectivity.ino`.

## Fluxo de Execucao

### Inicializacao (`setup()`)

1. Inicia Serial em 9600.
2. Configura resolucao ADC com `analogReadResolution(ADC_BIT_RESOLUTION)`.
3. Configura `ADC_11db` no pino do pH.
4. Deixa o rele SSR da temperatura desligado e configura o pino como saida.
5. Inicializa MQ135 com `configurarMQ135()`.
6. Inicializa BME680 com `configurarBME680()`.
7. Tenta conectar Wi-Fi com `conectarWiFi()`.
8. Inicializa NTP com `timeClient.begin()` e offset configurado.
9. Tenta sincronizar horario via `sincronizarHorario()`.

O `setup()` nao grava historico inicial.

### Execucao Continua (`loop()`)

`loop()` chama somente `executarTarefasPeriodicas()`.

Em `executarTarefasPeriodicas()`:

- tenta reconectar Wi-Fi em intervalo configurado;
- atualiza NTP quando Wi-Fi esta conectado;
- tenta calibrar MQ135 apos aquecimento e em retry configurado, enquanto nao calibrado;
- se horario nao estiver sincronizado, tenta NTP e nao grava historico;
- apos `SENSOR_STATUS_BOOT_DELAY_MS`, grava uma validacao unica em `/statusSensores/...`;
- no minuto configurado de cada hora, grava historico no Firebase.

Na gravacao, AirQuality e lido/gravado antes de Aquario. Aquario e coletado em ordem: pH, pausa de 10 s, turbidez, pausa de 10 s, DS18B20 com rele, pausa de 10 s, TDS, entao grava no Firebase.

## Componentes

### BME680

- Arquivo: `03_AirSensors.ino`
- Objeto: `Adafruit_BME680 bme680`
- Entrada: I2C SDA/SCL.
- Saidas: `temperaturaBME680`, `umidadeBME680`, `pressaoBME680`, `sensacaoTermicaBME680`.
- Validacao: temperatura entre -40 e 85; umidade entre 0 e 100.
- Observacao: o gas interno do BME680 nao e gravado neste codigo.

### MQ135

- Arquivo: `03_AirSensors.ino`
- Objeto: `MQUnifiedsensor MQ135`
- Entrada: ADC em `MQ135_PIN`.
- Saidas: `CO`, `CO2`, `Alcohol`, `Toluen`, `NH4`, `Aceton`.
- Pre-condicao: `mq135Calibrado == true`.
- Calibracao: media de 10 chamadas a `MQ135.calibrate(RATIO_MQ135_CLEAN_AIR)`.

### DS18B20

- Arquivo: `04_AquariumSensors.ino`
- Objetos: `OneWire oneWire`, `DallasTemperature sensors`
- Entrada: barramento OneWire em `ONE_WIRE_BUS`.
- Controle eletrico: rele SSR low-level trigger em `TEMPERATURE_SENSOR_RELAY_PIN`.
- Saida: `temperaturaDS18B20`.
- Fluxo: liga rele, espera `TEMPERATURE_SENSOR_POWER_ON_DELAY_MS`, tenta detectar o sensor, descarta a primeira leitura se configurado, tenta ler temperatura e desliga o rele.
- Validacao: rejeita sensor nao detectado, `DEVICE_DISCONNECTED_C` e `NaN`.
- Retry: deteccao ate `DS18B20_DETECTION_ATTEMPTS`; leitura ate `DS18B20_READ_ATTEMPTS`.

### TDS

- Arquivo: `04_AquariumSensors.ino`
- Entrada: `TDS_SENSOR_PIN`.
- Saidas: `tdsValue`, `tdsRawValue`, `averageVoltage`, `tdsCompensationVoltage`, `tdsCompensationTemperature`.
- Amostragem: media de `TDS_SAMPLE_COUNT` leituras.
- Compensacao: usa temperatura do DS18B20 ou 25 C como fallback.
- Observacao: TDS e lido por ultimo para reduzir interferencia no pH.

### Turbidez

- Arquivo: `04_AquariumSensors.ino`
- Entrada: `TURBIDEZ_SENSOR_PIN`.
- Saidas: `turbidity`, `turbidityVoltage`, `turbiditySensorVoltage`, `turbidityNtu`.
- Conversao: formula quadratica com limite em `TURBIDEZ_MAX_NTU`.

### pH

- Arquivo: `04_AquariumSensors.ino`
- Entrada: `PH_SENSOR_PIN`.
- Saidas: `phRawValue`, `phVoltage`, `phSensorVoltage`, `ph_act`.
- Metodo: mediana de `PH_SAMPLE_COUNT` amostras, 20 ms entre amostras, `ADC_11db` no pino e calibracao segmentada em tres pontos.
- Validacao: pH entre 0 e 14.
- Observacao: pH e lido antes do TDS por sensibilidade a interferencia eletrica.

### Firebase

- Arquivo: `05_Firebase.ino`
- Transporte: `WiFiClientSecure` + `HTTPClient`.
- Escrita: `HTTP PUT` em `<FIREBASE_HOST>/<path>/<id>.json?auth=<FIREBASE_AUTH>`.
- Operacoes passam primeiro por `/firebase-queue.jsonl` em LittleFS; cada item guarda tipo, destino, JSON, timestamp, ID e metodo.
- Na primeira inicializacao, se a particao estiver sem filesystem, o firmware a formata uma vez e marca a inicializacao em NVS. Depois disso nunca formata automaticamente se houver falha de montagem.
- A particao `default` do core 2.0.14 tem 1.375 MiB para SPIFFS/LittleFS. A fila limita-se a 64 KiB e 100 itens; fila cheia rejeita a nova operacao e registra falha critica, sem apagar itens antigos.
- Com duas gravacoes historicas por hora, o limite de 100 itens equivale a cerca de 2 dias offline (menos se o limite de bytes for atingido; status de boot tambem ocupa item). Estimativa nao substitui medicao com payloads reais.
- Cada alteracao reescreve a fila limitada em arquivo temporario e rename, aumentando escritas conforme a fila cresce. A frequencia normal e baixa (horaria), mas outages longas elevam a escrita; um diario append-only seria necessario para volume maior.
- Se a particao estiver vazia/sem formato na primeira inicializacao, ela e formatada para LittleFS; isso pode apagar dados antigos nessa particao. NVS impede formatacao automatica em boots posteriores.
- Uma operacao e tentada ao fim das tarefas de cada volta do loop, com intervalo/backoff de 30 s ate 1 h. O timeout HTTP ainda pode bloquear ate 15 s, depois da coleta prevista naquela volta.
- PUT usa filho deterministico `SensorsController_<tipo>` no historico e ID por boot para status. Consumidores que dependam da forma da chave push precisam ser verificados.
- Caminhos:
  - `/historico/AirQuality/DD-MM-AAAA/HH-MM/SensorsController_AirQuality`
  - `/historico/Aquario/DD-MM-AAAA/HH-MM/SensorsController_Aquario`
  - `/statusSensores/DD-MM-AAAA/HH-MM/SensorsController_boot_<millis>`
- AirQuality e Aquario sao historicos, no maximo uma operacao por hora/minuto configurado. StatusSensores e uma validacao de boot, fora do historico; o seu payload mantem `status_de_cada_sensor`.
- Nao ha consumidor Firebase no repositorio. A ordenacao documentada pelos pais de data/hora e preservada, mas qualquer consumidor que use a chave push como identificador deve ser conferido.

## Fluxo de Dados

```text
Sensores fisicos
  -> funcoes de leitura
  -> variaveis globais
  -> JsonDocument
  -> payload JSON
  -> fila FIFO LittleFS
  -> HTTP PUT
  -> Firebase Realtime Database
```

## Firebase

### Escrita AirQuality

Campos escritos:

- `bme680Ok`
- `mq135Ok`
- quando BME680 valido:
  - `temperatura`
  - `sensacaoTermica`
  - `pressao`
  - `umidade`
- quando MQ135 calibrado e lido:
  - `Aceton`
  - `Alcohol`
  - `CO`
  - `CO2`
  - `NH4`
  - `Toluen`

### Escrita Aquario

Campos escritos:

- `ds18b20Ok`
- `tdsOk`
- `turbidezOk`
- `phOk`
- quando DS18B20 valido:
  - `temperaturaDS18B20`
- quando TDS valido:
  - `TDS`
  - `tdsPpm`
  - `tdsMgL`
  - `tdsRaw`
  - `tdsVoltage`
  - `tdsCompensationVoltage`
  - `tdsCompensationTemperature`
- quando turbidez valida:
  - `Turbidez`
  - `turbidezNtu`
  - `turbidezRaw`
  - `turbidezVoltage`
  - `turbidezSensorVoltage`
- quando pH valido:
  - `PH`
  - `phRaw`
  - `phVoltage`
  - `phSensorVoltage`

### Escrita StatusSensores

Gravacao unica pos-boot, fora de `/historico`, apos `SENSOR_STATUS_BOOT_DELAY_MS` e somente com horario NTP valido.

Caminho:

- `/statusSensores/DD-MM-AAAA/HH-MM/SensorsController_boot_<millis>`

Campo principal:

- `status_de_cada_sensor`

Sensores avaliados:

- `BME680`
- `MQ135`
- `DS18B20`
- `PH`
- `Turbidez`
- `TDS`

A ordem de aquario no status segue a mesma regra de sensibilidade: pH, turbidez, DS18B20 com rele e TDS por ultimo. O DS18B20 e energizado somente dentro de `lerDS18B20()`.

## Configuracoes

Todas em `config.h`:

- ADC: 12 bits, referencia 3.3 V, maximo 4095.
- MQ135: pino A3, warmup 120000 ms, retry 600000 ms.
- BME680 I2C: SDA 21, SCL 22, timeout 15000 ms.
- Wi-Fi: timeout 20000 ms, retry 30000 ms.
- Firebase HTTP: timeout 15000 ms.
- Fila Firebase: 64 KiB, 100 itens, sincronizacao a cada 30 s apos falhas, backoff ate 1 h.
- StatusSensores pos-boot: delay 180000 ms, retry 30000 ms.
- NTP: UTC-3.
- DS18B20: GPIO17, rele SSR em GPIO25 low-level trigger, 3 s de estabilizacao, 5 tentativas de deteccao, descarte da primeira leitura, 5 tentativas de leitura e 250 ms entre tentativas.
- Sequencia Aquario: 10 s entre sensores.
- TDS: A2/GPIO35, 100 amostras, 20 ms entre amostras.
- Turbidez: A5, 10 amostras, 10 ms entre amostras.
- pH: A4/GPIO36, `ADC_11db`, 100 amostras, 20 ms entre amostras.
- Calibracao pH: `V_PH_4 = 0.576996`, `V_PH_7 = 1.642344`, `V_PH_10 = 2.677070`, slope acido `2.815981`, slope alcalino `2.899320`.
- Gravacao horaria: minuto 0.

## Eventos

Eventos baseados em tempo:

- reconexao Wi-Fi por intervalo;
- atualizacao de NTP quando Wi-Fi conectado;
- calibracao MQ135 apos warmup e retries;
- validacao unica de sensores apos 3 minutos de boot;
- gravacao horaria quando minuto atual igual a `HOURLY_RECORD_MINUTE`.

## Fluxos Criticos

1. Horario precisa estar sincronizado para montar caminhos de data/hora.
2. Wi-Fi precisa estar conectado para escrita HTTP no Firebase.
3. MQ135 so gera dados depois de calibrado.
4. StatusSensores pos-boot nao deve gravar em `/historico`.
5. BME680 ausente nao bloqueia boot; apenas omite campos ambientais.
6. DS18B20 ausente grava `ds18b20Ok = false` e usa 25 C como fallback para compensar TDS.
7. pH e lido antes do TDS para evitar interferencia por polarizacao.
8. DS18B20 fica desligado fora da janela de leitura.
9. TDS, turbidez e pH gravam campos somente quando validos.
10. Duplicidade por minuto e evitada separadamente para AirQuality e Aquario; PUT em ID deterministico torna retries idempotentes.

## Arquivos Mais Importantes

1. `SensorsController.ino`
2. `config.h`
3. `secrets.h`
4. `06_Tasks.ino`
5. `05_Firebase.ino`
6. `03_AirSensors.ino`
7. `04_AquariumSensors.ino`
8. `01_Utils.ino`
9. `02_Connectivity.ino`

## Riscos Tecnicos

- `secrets.h` contem credenciais.
- Leitura analogica ESP32 exige tensao ate 3.3 V.
- `client.setInsecure()` ignora validacao de certificado TLS.
- Uso de `String` em caminhos e URLs.
- Uso de token Firebase em query string.
- Ausencia de testes automatizados.
- Contrato Firebase esta implicito no codigo.
- Fila cheia ou falha do LittleFS rejeita nova operacao com log critico; limite de armazenamento e finito.
- Flash fisica e comportamento de queda de energia ainda precisam de verificacao na placa.

## Validacao do firmware

Build local: `arduino-cli compile --warnings all --fqbn esp32:esp32:d1_uno32 .`, core ESP32 2.0.14, particao `default`. Resultado: 1.007.169 bytes de flash (76% dos 1.310.720 bytes do app) e 47.836 bytes de globais (14% dos 327.680 bytes; 279.844 bytes restantes). Warnings observados vieram das bibliotecas OneWire e LittleFS do core. Wi-Fi, Firebase, falhas de energia, timeout ambiguo e capacidade real da fila ainda requerem teste fisico.

## Mapa para IA

Para alterar sensores de ar, ler `SensorsController.ino`, `config.h`, `03_AirSensors.ino`, `05_Firebase.ino`.

Para alterar sensores de aquario, ler `SensorsController.ino`, `config.h`, `04_AquariumSensors.ino`, `05_Firebase.ino`.

Para alterar Firebase, ler `01_Utils.ino`, `05_Firebase.ino`, `06_Tasks.ino`.

Para alterar Wi-Fi/NTP, ler `01_Utils.ino`, `02_Connectivity.ino`, `06_Tasks.ino`, `SensorsController.ino`.

## Resumo Executivo

Firmware ESP32 organizado por abas Arduino, focado exclusivamente em sensores e historico. A placa coleta dados ambientais e de aquario, registra historico por hora no Firebase e depende de horario NTP valido para criar caminhos corretos. As areas mais sensiveis sao contrato Firebase, agenda NTP, calibracao de sensores e seguranca eletrica das entradas ADC.
