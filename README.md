# API de marés — Lauro de Freitas

API intermediária para um ESP32 com display. Ela coleta a janela pública de previsão de `tabuademares.com`, converte os valores para JSON e mantém cache por 30 minutos para não sobrecarregar a fonte.

## Dados disponíveis

- quatro eventos de maré quando publicados: `high`/`low`, horário, altura em metros e coeficiente;
- coeficiente mínimo e máximo do dia;
- nascer e pôr do sol;
- fase, idade e iluminação da lua;
- atividade e períodos solunares, quando publicados na página detalhada de hoje.

A fonte pública apresenta normalmente uma janela de sete dias. Não há tentativa de contornar a assinatura do Nautide ou de acessar endpoints privados. Para previsões além da janela pública, use uma fonte/licença de dados apropriada.

## Executar

```bash
cd lauro-freitas-tide-api
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
uvicorn main:app --host 0.0.0.0 --port 8000
```

Endpoints:

- `GET /health`
- `GET /api/v1/forecast` — janela de até sete dias e detalhes de hoje.
- `GET /api/v1/day/AAAA-MM-DD` — um dia da janela disponível.
- `GET /api/v1/esp32` — payload compacto para o display.
- `GET /docs` — documentação OpenAPI.

Exemplo:

```bash
curl http://localhost:8000/api/v1/esp32
```

O ESP32 deve chamar a URL pública ou o IP do computador/Raspberry Pi que hospeda a API. O ideal é atualizar o display a cada 15–30 minutos, tratar HTTP 502 usando o último JSON válido e não fazer requisições a cada segundo.

Exemplo conceitual no Arduino:

```cpp
HTTPClient http;
http.begin("http://192.168.1.10:8000/api/v1/esp32");
int code = http.GET();
if (code == HTTP_CODE_OK) {
  String json = http.getString();
  // ArduinoJson: deserializeJson(doc, json)
  // doc["tides"][0]["time"], doc["tides"][0]["height_m"]
}
http.end();
```

## Licença e uso responsável

Os dados são atribuídos à fonte no campo `source`. Verifique os termos de uso do site antes de publicar a API na internet. O scraper usa User-Agent identificável, timeout e cache.
