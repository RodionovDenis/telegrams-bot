# push-up-bot

Необходимо создать папку `config` и положить туда файл `config.json`

`docker build . --tag pushupbot`

`docker run -v $(pwd)/config:/push-up-bot/config pushupbot`
