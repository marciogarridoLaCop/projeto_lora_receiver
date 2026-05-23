# ESP32 LoRa Receiver

Sistema embarcado baseado em **ESP32 + LoRa 868 MHz** que recebe pacotes de sensores remotos e os exibe em tempo real num painel web responsivo. Quando a rede WiFi não está configurada, o dispositivo levanta um **portal de configuração** próprio, eliminando a necessidade de recompilar o firmware para trocar credenciais.

---

## Sumário

- [Visão Geral](#visão-geral)
- [Hardware](#hardware)
- [Pinagem](#pinagem)
- [Interfaces Web](#interfaces-web)
- [Display OLED](#display-oled)
- [Fluxo de Operação](#fluxo-de-operação)
- [API REST](#api-rest)
- [Instalação e Flash](#instalação-e-flash)
- [Dependências](#dependências)
- [Estrutura do Projeto](#estrutura-do-projeto)

---

## Visão Geral

```
[ Sensor Node ]  ---LoRa 868 MHz--->  [ ESP32 Receiver ]  ---WiFi/HTTP--->  [ Navegador ]
  temperatura                           Heltec WiFi                           Dashboard
  pressão                               LoRa 32 V2                            em tempo real
  altitude
```

O receptor decodifica pacotes JSON enviados pelo nó sensor via LoRa, obtém o timestamp via NTP e disponibiliza os dados num servidor HTTP embarcado. Todos os arquivos estáticos (HTML/CSS/JS) são armazenados no sistema de arquivos SPIFFS da flash interna.

---

## Hardware

| Componente | Modelo |
|---|---|
| Microcontrolador | **Heltec WiFi LoRa 32 V2** (ESP32 + SX1276) |
| Display | OLED 0.96" SSD1306 128×64 (integrado) |
| Rádio LoRa | SX1276 — 868 MHz (integrado) |
| Conectividade | WiFi 802.11 b/g/n (integrado) |

---

## Pinagem

### LoRa (SPI)

| Sinal | GPIO |
|---|---|
| SCK | 5 |
| MISO | 19 |
| MOSI | 27 |
| NSS (SS) | 18 |
| RESET | 14 |
| DIO0 | 26 |

### Display OLED (I²C)

| Sinal | GPIO |
|---|---|
| SDA | 4 |
| SCL | 15 |
| RST | 16 |

> Todos os pinos são os padrões da placa Heltec WiFi LoRa 32 V2 — nenhuma soldagem adicional necessária.

---

## Interfaces Web

### Painel de Dados — `http://<IP do dispositivo>/`

Exibe os dados recebidos via LoRa em cards atualizados automaticamente a cada **3 segundos**.

```
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║          📡  ESP32 LoRa Receiver                             ║
║                                                              ║
║   🕐 2024-03-15 14:32:07      📶 RSSI: -87 dBm              ║
║                                                              ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║   ┌──────────────┐  ┌──────────────┐                        ║
║   │   #          │  │   🌡         │                        ║
║   │              │  │              │                        ║
║   │  PACOTE ID   │  │ TEMPERATURA  │                        ║
║   │              │  │              │                        ║
║   │     42       │  │    27.5      │                        ║
║   │              │  │     °C       │                        ║
║   └──────────────┘  └──────────────┘                        ║
║                                                              ║
║   ┌──────────────┐  ┌──────────────┐                        ║
║   │   ⏱         │  │   ▲          │                        ║
║   │              │  │              │                        ║
║   │   PRESSÃO    │  │   ALTITUDE   │                        ║
║   │              │  │              │                        ║
║   │   101325     │  │    142.3     │                        ║
║   │     Pa       │  │      m       │                        ║
║   └──────────────┘  └──────────────┘                        ║
║                                                              ║
║              ESP32 LoRa — Atualiza a cada 3s                 ║
╚══════════════════════════════════════════════════════════════╝
```

**Cards e cores:**

| Card | Ícone | Cor |
|---|---|---|
| Pacote ID | `#` | Roxo `#a78bfa` |
| Temperatura | Termômetro | Vermelho `#f87171` |
| Pressão | Velocímetro | Amarelo `#fbbf24` |
| Altitude | Montanha | Verde `#34d399` |

---

### Configurador WiFi — `http://192.168.4.1/`

Ativado automaticamente quando o dispositivo não consegue se conectar à rede salva. O ESP32 cria o ponto de acesso **`LoRa-Config`** e serve esta página na rota raiz.

```
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║         📶  ESP32 LoRa — WiFi Setup                         ║
║         Configure a rede WiFi do dispositivo                 ║
║                                                              ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║  ┌─ REDES DISPONÍVEIS ───────────────────── ↺ Atualizar ─┐  ║
║  │                                                        │  ║
║  │  🔒  MinhaRedeWiFi                    ▌▌▌▌  -52 dBm  │  ║
║  │  🔒  Vizinho_2G             ████      ▌▌▌   -65 dBm  │  ║
║  │      RedeAberta                       ▌▌    -72 dBm  │  ║
║  │  🔒  CLARO_5GHz                       ▌     -81 dBm  │  ║
║  │                                                        │  ║
║  └────────────────────────────────────────────────────────┘  ║
║                                                              ║
║  ┌─ CREDENCIAIS ──────────────────────────────────────────┐  ║
║  │                                                        │  ║
║  │  NOME DA REDE (SSID)                                   │  ║
║  │  ┌──────────────────────────────────────────────────┐  │  ║
║  │  │ MinhaRedeWiFi                                    │  │  ║
║  │  └──────────────────────────────────────────────────┘  │  ║
║  │                                                        │  ║
║  │  SENHA                                                 │  ║
║  │  ┌──────────────────────────────────────────────── 👁 ┐│  ║
║  │  │ ••••••••••••                                       ││  ║
║  │  └────────────────────────────────────────────────────┘│  ║
║  │                                                        │  ║
║  │  ┌──────────────────────────────────────────────────┐  │  ║
║  │  │            Salvar e Conectar                     │  │  ║
║  │  └──────────────────────────────────────────────────┘  │  ║
║  │                                                        │  ║
║  │  ✅ Salvo! O dispositivo vai reiniciar e conectar...   │  ║
║  │                                                        │  ║
║  └────────────────────────────────────────────────────────┘  ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
```

**Funcionalidades do configurador:**

- Varredura automática de redes ao abrir a página
- Seleção por clique — preenche o campo SSID automaticamente
- Indicador de sinal (4 barras) com valor RSSI em dBm
- Ícone de cadeado para redes protegidas
- Botão mostrar/ocultar senha
- Feedback visual de sucesso ou erro após salvar
- Dispositivo reinicia automaticamente e conecta à nova rede

---

## Display OLED

O display 128×64 exibe o estado do sistema em cada etapa:

| Etapa | Conteúdo |
|---|---|
| Boot | Logo do projeto |
| Inicialização LoRa | `LoRa Initializing OK!` |
| Conectando WiFi | SSID + `Aguarde...` |
| WiFi conectado | IP local |
| Aguardando pacote | `LoRa Receiver OK` / `Aguardando pacote` |
| Pacote recebido | ID, Temperatura, Pressão, Altitude |
| Modo configuração | `LoRa-Config` + `192.168.4.1` |

---

## Fluxo de Operação

```
                    ┌─────────────┐
                    │    BOOT     │
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │ Inicializa  │
                    │ OLED + LoRa │
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │ Carrega     │
                    │ credenciais │
                    │ (NVS/Flash) │
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │ Tenta WiFi  │ ──── timeout 15s ────┐
                    └──────┬──────┘                      │
                           │ conectou                    │
               ┌───────────▼───────────┐    ┌───────────▼───────────┐
               │  Inicia servidor HTTP │    │  Modo AP: LoRa-Config  │
               │  Inicia NTP client   │    │  Serve config.html     │
               └───────────┬───────────┘    │  Aguarda credenciais  │
                           │                │  → salva → reinicia   │
               ┌───────────▼───────────┐    └───────────────────────┘
               │       LOOP            │
               │  ┌─────────────────┐  │
               │  │ LoRa packet?    │  │
               │  │  → getLoRaData  │  │
               │  │  → getTimeStamp │  │
               │  │  → OLED update  │  │
               │  └─────────────────┘  │
               │  handleClient()       │
               └───────────────────────┘
```

---

## API REST

O servidor HTTP embarcado expõe os seguintes endpoints (método `GET`):

| Endpoint | Descrição | Exemplo de resposta |
|---|---|---|
| `GET /` | Painel web completo (HTML) | — |
| `GET /temperature` | Temperatura em °C | `27.5` |
| `GET /pressure` | Pressão em Pa | `101325` |
| `GET /altitude` | Altitude em metros | `142.3` |
| `GET /rssi` | RSSI do último pacote (dBm) | `-87` |
| `GET /timestamp` | Data e hora UTC (NTP) | `2024-03-15 14:32:07` |
| `GET /readingid` | ID sequencial do pacote | `42` |

**Endpoints do Portal de Configuração** (disponíveis apenas no modo AP):

| Endpoint | Descrição |
|---|---|
| `GET /` | Página de configuração WiFi |
| `GET /scan` | Lista redes WiFi em JSON |
| `POST /save` | Salva SSID e senha na flash (NVS) |

---

## Instalação e Flash

### Pré-requisitos

- [VS Code](https://code.visualstudio.com/) + extensão [PlatformIO](https://platformio.org/)
- Driver USB-Serial (CP2102 — já incluído no macOS/Linux; Windows requer instalação manual)

### Passos

```bash
# 1. Clone o repositório
git clone https://github.com/marciogarridoLaCop/projeto_lora_receiver.git
cd projeto_lora_receiver

# 2. Abra no VS Code com PlatformIO instalado

# 3. Faça o upload do sistema de arquivos (HTML/CSS/JS → SPIFFS)
pio run --target uploadfs

# 4. Compile e faça o upload do firmware
pio run --target upload

# 5. Acompanhe o monitor serial (115200 baud)
pio device monitor
```

### Primeira Configuração WiFi

1. Após o flash, se não houver credenciais salvas, o dispositivo cria a rede **`LoRa-Config`**
2. Conecte-se a ela pelo celular ou computador (sem senha)
3. Acesse `http://192.168.4.1` no navegador
4. Selecione a sua rede, insira a senha e clique em **Salvar e Conectar**
5. O dispositivo reinicia e se conecta automaticamente
6. O IP local é exibido no display OLED

---

## Dependências

Gerenciadas automaticamente pelo PlatformIO via `platformio.ini`:

| Biblioteca | Finalidade |
|---|---|
| `arduino-LoRa` | Driver do rádio SX1276 |
| `Adafruit SSD1306` | Driver do display OLED |
| `Adafruit GFX Library` | Gráficos para OLED |
| `NTPClient` | Sincronização de horário via NTP |
| `WiFi` / `WebServer` | Servidor HTTP embarcado (ESP32 core) |
| `Preferences` | Armazenamento de credenciais na NVS |
| `SPIFFS` | Sistema de arquivos para HTML/CSS/JS |

---

## Estrutura do Projeto

```
projeto_lora_receiver/
├── src/
│   └── main.cpp          # Firmware principal
├── data/
│   ├── index.html        # Painel de dados (SPIFFS)
│   └── config.html       # Portal de configuração WiFi (SPIFFS)
├── include/
│   └── logo.h            # Logo bitmap para o OLED
├── platformio.ini         # Configuração do PlatformIO
└── README.md
```

---

## Licença

Baseado no projeto original de [Rui Santos & Sara Santos — Random Nerd Tutorials](https://RandomNerdTutorials.com/esp32-lora-sensor-web-server/).  
Modificações e melhorias por **Marcio Garrido**.
