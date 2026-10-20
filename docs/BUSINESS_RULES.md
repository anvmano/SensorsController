# BUSINESS_RULES

## Regra: Horario valido antes de operacoes historicas

### Arquivo

`01_Utils.ino`, `SensorsController.ino`, `06_Tasks.ino`

### Metodo

`horarioSincronizado`, `sincronizarHorario`, `setup`, `executarTarefasPeriodicas`

### Objetivo

Evitar uso de data/hora invalida em caminhos de Firebase.
O boot apenas sincroniza horario; nao deve gravar historico inicial.

### Entradas

- `timeClient.getEpochTime()`
- `MIN_VALID_YEAR`
- status Wi-Fi

### Saidas

- `true` ou `false`
- logs Serial
- nenhuma gravacao inicial no `setup()`

### Impacto

Critico.

### O que acontece se for alterada

Pode gravar dados em data errada ou impedir gravacao correta do historico.

## Regra: Caminho historico padronizado

### Arquivo

`01_Utils.ino`

### Metodo

`montarCaminhoHistorico`

### Objetivo

Gerar caminho `/historico/<categoria>/DD-MM-AAAA/HH-MM`.

### Entradas

- categoria
- data/hora atuais

### Saidas

String de caminho Firebase.

### Impacto

Critico.

### O que acontece se for alterada

Consumidores externos do Firebase podem deixar de encontrar dados.

## Regra: Status de sensores pos-boot fora de historico

### Arquivo

`01_Utils.ino`, `05_Firebase.ino`, `06_Tasks.ino`

### Metodo

`montarCaminhoStatusSensores`, `gravarStatusSensores`, `executarTarefasPeriodicas`

### Objetivo

Permitir validar sensores poucos minutos apos ligar a placa sem esperar a gravacao horaria e sem poluir `/historico`.

### Entradas

- `SENSOR_STATUS_BOOT_DELAY_MS`
- `SENSOR_STATUS_RETRY_INTERVAL_MS`
- horario NTP valido
- status Wi-Fi
- leituras de BME680, MQ135, DS18B20, pH, turbidez e TDS

### Saidas

`PUT` em `/statusSensores/DD-MM-AAAA/HH-MM/SensorsController_boot_<millis>` contendo `status_de_cada_sensor`.

### Impacto

Alta criticidade.

### O que acontece se for alterada

Pode voltar a poluir historico, gravar status com data invalida, ou impedir validacao pos-boot dos sensores.

## Regra: Escritas Firebase passam pela fila persistente

### Arquivo

`05_Firebase.ino`

### Metodo

`enviarJsonFirebase`

### Objetivo

Persistir a operacao em LittleFS antes de sincronizar e usar PUT em um filho deterministico.

### Entradas

- caminho Firebase
- `JsonDocument`
- descricao

### Saidas

- `true` quando persistida localmente (mesmo que ainda pendente)
- `false` se LittleFS falhar ou atingir o limite
- itens removidos somente apos HTTP 200

### Impacto

Critico.

### O que acontece se for alterada

Consumidores que dependem das chaves push aleatorias precisam ser avaliados; caminhos e campos do objeto permanecem iguais.

## Regra: Evitar duplicidade por minuto

### Arquivo

`05_Firebase.ino`

### Metodo

`gravarHistoricoAtual`

### Objetivo

Evitar multiplas gravacoes para a mesma categoria no mesmo minuto.

### Entradas

- chave atual `DD-MM-AAAA HH-MM`
- `ultimaChaveHistoricoArGravada`
- `ultimaChaveHistoricoAquarioGravada`

### Saidas

Gravacao de AirQuality e/ou Aquario quando ainda nao gravados no minuto.

### Impacto

Alta criticidade.

### O que acontece se for alterada

Pode gerar registros duplicados ou impedir retry de categoria que falhou.

## Regra: Fila FIFO e limite

Cada operacao e gravada em LittleFS antes do envio em `/firebase-queue.jsonl`. A fila guarda tipo, destino, payload, timestamp do caminho original, ID e metodo. O arquivo temporario e validado no boot e promovido por rename; item so e removido apos HTTP 200. Sincroniza no maximo um item ao fim das tarefas de cada volta do loop e para na primeira falha, com backoff de 30 s ate 1 h. Limite: 64 KiB ou 100 itens (aproximadamente 2 dias de historicos horarios). Ao lotar, rejeita o item novo com log critico e preserva os antigos; a categoria nao e amostrada de novo ate a proxima janela horaria. A fila e de historicos e eventos de boot, nao de estado substituivel.

## Regra: AirQuality grava flags e campos condicionais

### Arquivo

`05_Firebase.ino`

### Metodo

`gravarFirebaseAr`

### Objetivo

Registrar status dos sensores e campos somente quando leituras forem validas.

### Entradas

- BME680
- MQ135

### Saidas

JSON com `bme680Ok`, `mq135Ok` e campos ambientais/de gases condicionais.

### Impacto

Alta criticidade.

### O que acontece se for alterada

Pode mudar contrato de dados de AirQuality.

## Regra: Aquario grava flags e campos condicionais

### Arquivo

`05_Firebase.ino`

### Metodo

`gravarFirebaseAquario`

### Objetivo

Registrar status dos sensores de aquario e campos somente quando leituras forem validas.

### Entradas

- DS18B20
- TDS
- turbidez
- pH

### Ordem de leitura

1. pH.
2. Espera `AQUARIUM_SENSOR_INTERVAL_MS`.
3. Turbidez.
4. Espera `AQUARIUM_SENSOR_INTERVAL_MS`.
5. DS18B20 com rele.
6. Espera `AQUARIUM_SENSOR_INTERVAL_MS`.
7. TDS.
8. Monta JSON e grava no Firebase.

### Saidas

JSON com `ds18b20Ok`, `tdsOk`, `turbidezOk`, `phOk` e campos condicionais.

### Impacto

Alta criticidade.

### O que acontece se for alterada

Pode mudar contrato de dados de Aquario.

## Regra: DS18B20 so energiza durante leitura

### Arquivo

`SensorsController.ino`, `04_AquariumSensors.ino`, `config.h`

### Metodo

`setup`, `lerDS18B20`

### Objetivo

Energizar o sensor de temperatura somente durante a leitura, mantendo o rele SSR desligado em repouso.

### Entradas

- `TEMPERATURE_SENSOR_RELAY_PIN`
- `TEMPERATURE_SENSOR_RELAY_ACTIVE_STATE`
- `TEMPERATURE_SENSOR_RELAY_INACTIVE_STATE`
- `TEMPERATURE_SENSOR_POWER_ON_DELAY_MS`
- `DS18B20_DETECTION_ATTEMPTS`
- `DS18B20_DETECTION_RETRY_DELAY_MS`
- `DS18B20_DISCARD_FIRST_READ`
- `DS18B20_READ_ATTEMPTS`
- `DS18B20_RETRY_DELAY_MS`

### Saidas

- Rele SSR low-level trigger desligado em repouso.
- DS18B20 ligado somente durante a leitura.
- Sensor detectado apos energizar antes das leituras reais.
- Primeira leitura descartada quando `DS18B20_DISCARD_FIRST_READ` esta ativo.
- Temperatura valida gravada em `temperaturaDS18B20`.

### Impacto

Alta criticidade.

### O que acontece se for alterada

Pode manter o sensor de temperatura energizado, falhar ao detectar o DS18B20, ou prejudicar a compensacao de TDS.

## Regra: MQ135 precisa calibrar antes de ler gases

### Arquivo

`03_AirSensors.ino`, `06_Tasks.ino`

### Metodo

`calibrarMQ135`, `lerMQ135`, `executarTarefasPeriodicas`

### Objetivo

Evitar uso de leituras de gases sem R0 calibrado.

### Entradas

- ADC MQ135
- `MQ135_WARMUP_MS`
- `MQ135_RETRY_CALIBRATION_MS`
- `ENABLE_MQ135_AUTO_CALIBRATION`

### Saidas

- `mq135Calibrado`
- gases se calibrado

### Impacto

Alta criticidade.

### O que acontece se for alterada

Pode gravar gases sem calibracao ou nunca gravar gases.

## Regra: BME680 nao bloqueia boot

### Arquivo

`03_AirSensors.ino`

### Metodo

`configurarBME680`

### Objetivo

Tentar inicializar BME680 com timeout.

### Entradas

I2C.

### Saidas

`true` ou `false`; valor usado em `bme680Disponivel`.

### Impacto

Media criticidade.

### O que acontece se for alterada

Firmware pode travar no boot se sensor estiver ausente.

## Regra: Leitura BME680 valida faixa fisica

### Arquivo

`03_AirSensors.ino`

### Metodo

`lerBME680`

### Objetivo

Rejeitar temperatura/umidade fora de faixa.

### Entradas

Temperatura, umidade e pressao do BME680.

### Saidas

Variaveis globais ambientais ou `false`.

### Impacto

Media criticidade.

### O que acontece se for alterada

Dados invalidos podem ser gravados.

## Regra: DS18B20 desconectado nao grava temperatura

### Arquivo

`04_AquariumSensors.ino`, `05_Firebase.ino`

### Metodo

`lerDS18B20`, `gravarFirebaseAquario`

### Objetivo

Detectar `DEVICE_DISCONNECTED_C`/`NaN`, tentar novamente e omitir temperatura invalida.

### Entradas

Leitura DallasTemperature.

### Saidas

`ds18b20Ok` e temperatura opcional.

### Impacto

Alta criticidade.

### O que acontece se for alterada

Pode gravar temperatura invalida ou prejudicar compensacao do TDS.

## Regra: TDS usa compensacao por temperatura

### Arquivo

`04_AquariumSensors.ino`, `05_Firebase.ino`

### Metodo

`lerTDS`, `gravarFirebaseAquario`

### Objetivo

Calcular TDS considerando a temperatura da agua ou fallback de 25 C.

### Entradas

- media ADC de TDS
- temperatura DS18B20 ou 25 C

### Saidas

TDS em ppm/mgL e campos de diagnostico.

### Impacto

Media/alta criticidade.

### O que acontece se for alterada

Pode alterar significativamente leituras historicas de TDS.

## Regra: pH deve ficar entre 0 e 14

### Arquivo

`04_AquariumSensors.ino`

### Metodo

`lerPH`

### Objetivo

Evitar gravacao de leitura fisicamente invalida.

### Entradas

ADC pH com `ADC_11db`, 100 amostras e calibracao segmentada em tres pontos.

### Saidas

`ph_act` ou `false`.

### Impacto

Media criticidade.

### O que acontece se for alterada

Pode gravar pH impossivel ou ocultar leituras validas.

## Regra: pH deve ser lido antes do TDS

### Arquivo

`05_Firebase.ino`

### Metodo

`gravarFirebaseAquario`

### Objetivo

Reduzir interferencia eletrica do TDS sobre o sensor de pH.

### Entradas

- ordem de leitura dos sensores de aquario
- `AQUARIUM_SENSOR_INTERVAL_MS`

### Saidas

Leitura de pH feita antes de ler o TDS.

### Impacto

Alta criticidade.

### O que acontece se for alterada

Pode contaminar a leitura de pH por polarizacao/interferencia do sensor TDS.

# REGRAS QUE NAO DEVEM SER ALTERADAS

- Caminhos Firebase historicos.
- Bloqueio de historico quando horario nao esta sincronizado.
- Controle de duplicidade por categoria/minuto.
- Omissao de campos quando sensor falha.
- Limite ADC de 3.3 V no ESP32.
- Calibracao previa do MQ135.
- pH antes de TDS na sequencia de Aquario.
- Rele da temperatura desligado fora da leitura do DS18B20.

# MAPA DE IMPACTO

## Arquivo: SensorsController.ino

Responsabilidade: boot, globais, setup/loop.

O que pode quebrar: inicializacao de sensores, Wi-Fi, NTP e fluxo geral.

Nivel de risco: Critico.

## Arquivo: config.h

Responsabilidade: pinos e parametros.

O que pode quebrar: ligacao fisica, ADC, agenda e calibracoes.

Nivel de risco: Critico.

## Arquivo: 05_Firebase.ino

Responsabilidade: contrato de escrita.

O que pode quebrar: historico Firebase e consumidores externos.

Nivel de risco: Critico.

## Arquivo: 06_Tasks.ino

Responsabilidade: agenda.

O que pode quebrar: frequencia de leitura, gravacao, calibracao e NTP.

Nivel de risco: Critico.

## Arquivo: 03_AirSensors.ino

Responsabilidade: sensores de ar.

O que pode quebrar: AirQuality.

Nivel de risco: Alto.

## Arquivo: 04_AquariumSensors.ino

Responsabilidade: sensores de aquario.

O que pode quebrar: Aquario.

Nivel de risco: Alto.

# CALL GRAPH

```text
Arduino runtime
  -> setup()
    -> analogReadResolution()
    -> configurarMQ135()
    -> configurarBME680()
    -> conectarWiFi()
    -> timeClient.begin()
    -> sincronizarHorario()
    -> nao grava historico inicial
  -> loop()
    -> executarTarefasPeriodicas()
      -> conectarWiFi()
      -> timeClient.update()
      -> calibrarMQ135()
      -> sincronizarHorario()
      -> gravarHistoricoAtual()
      -> gravarFirebaseAr()
        -> lerBME680()
        -> lerMQ135()
        -> enviarJsonFirebase()
      -> gravarFirebaseAquario()
        -> lerPH()
        -> lerTurbidez()
        -> lerDS18B20()
        -> lerTDS()
        -> enviarJsonFirebase()
```
