# Hospedar a API publicamente para o ESP32

## Recomendação: Render Free

Para este projeto, o Render é a opção mais simples para começar sem cobrança mensal. O serviço recebe uma URL HTTPS pública e pode ser conectado a um repositório GitHub. O plano gratuito pode suspender serviços sem tráfego; o primeiro acesso depois disso pode ser mais lento. Para o ESP32, isso significa usar um intervalo razoável de atualização e manter o último JSON válido em cache local.

### 1. Publicar o código no GitHub

Na pasta do projeto:

```bash
git init
git add .
git commit -m "API de mares para ESP32"
git branch -M main
git remote add origin https://github.com/SEU_USUARIO/lauro-freitas-tide-api.git
git push -u origin main
```

Não publique arquivos com senhas Wi-Fi. Neste projeto, as credenciais de Wi-Fi ficam somente no sketch do ESP32, não na API.

### 2. Criar o Web Service

1. Acesse `https://dashboard.render.com/` e escolha **New > Web Service**.
2. Conecte o GitHub e selecione o repositório.
3. Escolha **Docker** como runtime, ou use o `render.yaml` pela opção Blueprint.
4. Se não usar o Blueprint, defina:
   - **Plan:** Free;
   - **Health Check Path:** `/health`.
5. Clique em **Create Web Service**.

O `Dockerfile` já usa a variável `PORT` fornecida pelo Render. Ao finalizar, copie a URL semelhante a:

```text
https://lauro-freitas-tide-api.onrender.com
```

Teste no navegador:

```text
https://lauro-freitas-tide-api.onrender.com/health
https://lauro-freitas-tide-api.onrender.com/api/v1/esp32
```

### 3. Atualizar o ESP32

No arquivo `tide_clock_esp32.ino`, troque o endereço local:

```cpp
const char* API_URL = "http://192.168.1.100:8000/api/v1/esp32";
```

por:

```cpp
const char* API_URL = "https://lauro-freitas-tide-api.onrender.com/api/v1/esp32";
```

Como a URL é HTTPS, o ESP32 deve usar `WiFiClientSecure` e um cliente HTTP seguro. Para um primeiro teste simples, pode-se usar `client.setInsecure()`, mas isso não valida o certificado TLS. Em um produto final, prefira fixar o certificado raiz ou usar um domínio próprio com certificado confiável.

## Railway

O Railway também aceita deploy direto do GitHub e detecta o `Dockerfile`. Depois de criar o projeto:

1. Escolha **Deploy from GitHub repo**.
2. Selecione o repositório.
3. Abra **Settings > Networking**.
4. Clique em **Generate Domain**.
5. Teste `/health` e `/api/v1/esp32`.

O arquivo `railway.toml` configura o health check. O Railway usa cobrança baseada em uso: a página oficial informa um crédito de teste inicial por 30 dias e, depois, limites/créditos conforme o plano. Verifique a tela de Billing antes de deixar o serviço ativo por longos períodos.

## HTTPS no código do ESP32

Para usar a URL pública do Render/Railway, altere os includes e a criação do cliente HTTP:

```cpp
#include <WiFiClientSecure.h>

WiFiClientSecure secureClient;
secureClient.setInsecure(); // teste inicial; não valida o certificado
HTTPClient http;
http.begin(secureClient, API_URL);
```

A chamada continua igual:

```cpp
int code = http.GET();
String payload = http.getString();
http.end();
```

## Boas práticas para o ESP32

- Consulte a API a cada 15 ou 30 minutos, não a cada segundo.
- Guarde o último payload válido na memória flash ou em Preferences.
- Se receber `502`, `503` ou erro de TLS, mostre o último valor e tente novamente depois.
- Use `/health` para diagnóstico, mas faça o ESP32 consumir `/api/v1/esp32`.
- Não inclua chaves ou credenciais no JSON público.

## Limitação importante dos dados

A API intermediária coleta a janela pública disponibilizada pelo Tábua de Marés. Ela não contorna assinatura do Nautide nem acessa endpoints privados. Antes de publicar o scraper, confira os termos de uso da fonte e mantenha o cache para reduzir requisições.
