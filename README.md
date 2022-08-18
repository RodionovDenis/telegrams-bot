# telegram-bots

Необходимо создать папку `config` и положить туда файл `config.json` последнего обновления

`docker build . --tag pushupbot`

`docker run -v $(pwd)/config:/push-up-bot/config pushupbot`
