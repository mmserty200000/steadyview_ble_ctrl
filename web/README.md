# SteadyView Web Bluetooth

Статическая одностраничная панель: `dist/index.html`, CSS и ES modules, без npm, CDN и сервера приложения.
Вся связь с приёмником идёт напрямую через Web Bluetooth. Прошивка и веб-пульт должны быть этой версии протокола.

## GitHub Pages

В репозитории есть `.github/workflows/pages.yml`.

1. Сохранить изменения в Git и отправить в GitHub в основную ветку (`master` или `main`).
2. В GitHub открыть **Settings → Pages → Build and deployment → Source → GitHub Actions**.
3. Открыть **Actions → Deploy Web Bluetooth controller → Run workflow** и выбрать основную ветку.
4. После зелёного deployment ссылка появится в **Settings → Pages** и в environment `github-pages`.
   Обычно это `https://<owner>.github.io/<repository>/`.

Далее push изменений `web/dist` автоматически обновляет сайт. Workflow публикует только эту папку,
а не исходники прошивки. Пути ресурсов относительные: работают и под `/repository/`, и на отдельном домене.
Если основная ветка называется иначе, изменить `on.push.branches` в workflow.
[Документация GitHub Pages](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).

## Локальный запуск

```sh
python3 -m http.server 8080 --directory web/dist
```

Открыть `http://localhost:8080` на том же компьютере. Для телефона нужен HTTPS-хостинг;
обычный `http://192.168…` не считается secure context. Не открывать через file://.
Поддержка зависит от браузера и ОС: проверяется `navigator.bluetooth`, HTTPS и Web Crypto.
Generic Android/iOS WebView может не предоставлять Web Bluetooth; сохранение ключа само по себе
не добавляет поддержку BLE. Для своей оболочки WebView нужен доступный Web Bluetooth API или отдельный native bridge
(в этом проекте bridge не реализован). Базовые требования: [Chrome Web Bluetooth](https://developer.chrome.com/docs/capabilities/bluetooth),
[MDN](https://developer.mozilla.org/en-US/docs/Web/API/Web_Bluetooth_API).

## Работа

«Найти приёмник» вызывает системный chooser с фильтром по advertised UUID
`bad8dbcb-268e-4df5-aeef-f3bd1961161a`. Совпадение сервиса проверяется повторно после подключения.
Произвольные устройства по одному только имени не принимаются. Первый выбор всегда требует действия пользователя.
«Подключить снова» использует `getDevices()`, если браузер сохранил разрешение, без повторного chooser.
Один приёмник может обслуживать только одно подключение: отключить старый клиент перед подключением нового.

Устройство без ключа предлагает первую пассфразу (минимум 12 символов). После настройки — вход.
При включённом «Запомнить» производный ключ хранится в localStorage, по browser device.id и проверенному salt
приёмника. Пассфраза строкой не сохраняется. Новый origin/профиль/очистка браузера требуют нового входа.
Если storage запрещён, вход работает, а запоминание явно сообщает об ошибке.
«Забыть ключ» удаляет его только из браузера и отключает BLE; защита в прошивке остаётся.

Сохранённый производный ключ даёт те же права, что пассфраза. Не добавлять к странице сторонние скрипты.
Доверять всему origin, на котором размещён пульт: разные project Pages одного владельца обычно делят origin
`owner.github.io` и localStorage. Для изоляции от других своих сайтов использовать отдельный домен.
Первую установку выполнять в доверенном окружении: она открыта, последующие команды подписаны;
это не шифрование всей телеметрии. Полная модель доступа и wire format — в `../Protocol/README.md`.

## Проверки

```sh
# Node >=22; Python protobuf + grpcio-tools для сравнения со схемой
./tests/run_web.sh
# Прошивка и протокол, включая HMAC/replay/смену ключа/ошибки flash
./tests/run.sh
cmake --build build
```

Web-тесты проверяют кодек с protoc, PBKDF2/HMAC, BLE-фрагментацию и полный клиент
с моделью GATT (установка ключа, команды, смена, переподключение). Аппаратный BLE проверяется отдельно.
