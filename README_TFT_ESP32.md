# Relógio circular de maré — ESP32 + ILI9341/ST7789

Arquivo principal: `tide_clock_esp32.ino`.

O sketch consulta o endpoint compacto da API:

```text
http://IP_DO_SERVIDOR:8000/api/v1/esp32
```

Ele desenha no display TFT:

- anel de 24 horas;
- marcas dos eventos de preia-mar e baixa-mar;
- ponteiro vermelho para o horário atual;
- altura interpolada da maré;
- estado `SUBINDO` ou `DESCENDO`;
- próximo evento e altura prevista;
- coeficiente mínimo/máximo;
- segunda tela com sol, lua, atividade e status da API.

## Ligações SPI recomendadas

| Display | ESP32 DevKit |
|---|---:|
| VCC | 3V3 |
| GND | GND |
| SCK/CLK | GPIO 18 |
| MOSI/SDI | GPIO 23 |
| MISO/SDO | GPIO 19 |
| CS | GPIO 5 |
| DC/RS | GPIO 2 |
| RST/RESET | GPIO 4 |
| LED/BL | GPIO 15 |

Se o seu módulo não tiver `MISO`, deixe essa ligação desconectada. Se o backlight estiver sempre ligado, altere `TFT_BL` para `-1`.

## Selecionar o display

Para ILI9341, mantenha:

```cpp
#define DISPLAY_ILI9341 1
// #define DISPLAY_ST7789 1
```

Para ST7789, troque para:

```cpp
// #define DISPLAY_ILI9341 1
#define DISPLAY_ST7789 1
```

O sketch foi preparado para módulos de 320×240 em orientação horizontal. Alguns ST7789 exigem ajuste de `TFT_WIDTH`, `TFT_HEIGHT` ou de um deslocamento de origem conforme a placa adquirida.

## Antes de gravar

Edite estas três linhas:

```cpp
const char* WIFI_SSID = "SUA_REDE_WIFI";
const char* WIFI_PASSWORD = "SUA_SENHA_WIFI";
const char* API_URL = "http://192.168.1.100:8000/api/v1/esp32";
```

O computador que executa a API precisa estar na mesma rede Wi-Fi do ESP32. Use o IP local do computador, e libere a porta 8000 no firewall se necessário.

## Bibliotecas

Instale pelo Gerenciador de Bibliotecas da Arduino IDE:

- Adafruit GFX Library;
- Adafruit ILI9341 **ou** Adafruit ST7735 and ST7789 Library;
- ArduinoJson 7.

Selecione `ESP32 Dev Module`, escolha a porta USB e grave o sketch. Abra o Monitor Serial em 115200 baud para ver o IP do ESP32 e os códigos HTTP.

## Observação sobre o cálculo

A fonte fornece eventos discretos de maré. O ponteiro mostra o horário atual e a altura exibida entre dois eventos é uma interpolação linear visual. Isso é adequado para um painel indicativo, mas não substitui uma previsão hidrodinâmica ou instrumento náutico certificado.
