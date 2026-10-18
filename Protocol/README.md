# BLE control protocol

`steadyview.proto` — источник протокола. `steadyview.pb.c/.h` сгенерированы nanopb 0.4.9.1 и включены в CMake. Обновление: `./Protocol/regenerate.sh` (Python с protobuf и grpcio-tools).

## GATT

| Назначение | UUID | Свойства |
| --- | --- | --- |
| Service | `bad8dbcb-268e-4df5-aeef-f3bd1961161a` | Primary, custom 128-bit |
| RX | `bad8dbcb-268e-4df5-aeef-f3bd1961161b` | Write with response |
| TX | `bad8dbcb-268e-4df5-aeef-f3bd1961161c` | Notify |

Один клиент. Сначала подписаться на TX (CCCD `01 00`), затем писать в RX.
**RX — protobuf `Request`, TX — фрагменты protobuf `Response`.**
Короткий Request (до 20 байт) передаётся одной ATT-записью без length-prefix.
Больший Request (до 128 байт) разбивается на RX-фрагменты: `[0xA2, transfer_id, index, count]`
и до 16 байт данных. Index начинается с 0; count — 1..8; все промежуточные фрагменты
содержат ровно 16 байт данных. Последовательность не перемежается другими запросами.
Несогласованные фрагменты отбрасываются; незавершённая сборка истекает через 5 секунд
между фрагментами и сбрасывается при смене BLE-сессии/CCCD. ACK ATT означает приём в FIFO,
а не выполнение команды. После неопределённого результата переподключиться и прочитать состояние,
не повторять команду автоматически.
Write without response и Prepare/Execute Write не используются.

Advertising содержит UUID сервиса и сокращённое имя `SteadyV`, scan response — полное `SteadyView`. Характеристики не требуют pairing/encryption. Nanopb декодирует protobuf, а не криптографически расшифровывает данные.

## Request / Response

```text
Request {
  fixed32 request_id
  oneof command {
    SetProp { Property property }
    GetProp { Property property }
    GetState {}
    SaveSettings {}
    GetSettingsInfo {}
    GetAuth {}
    Authenticate { bytes proof }
    SetPassphrase { bytes key }
    SecureCommand { bytes payload; bytes mac; fixed32 sequence }
  }
}
Response {
  fixed32 request_id
  Status status
  oneof result {
    PropertyResult { Status status; Property property }
    ReceiverState { repeated PropertyResult properties; SettingsInfo settings }
    SettingsInfo { saved; dirty; revision; restore_status }
    AuthState { configured; authorized; salt; challenge; iterations }
  }
}
Property {
  oneof kind {
    ChannelIndex channel_index
    VideoMode video_mode
    FrequencyMHz frequency_mhz
    Initialize initialize
    SynthesizerRegA synthesizer_reg_a
    SynthesizerRegB synthesizer_reg_b
    ... остальные регистры ...
  }
}
```

Свойство определяется типом в `oneof`, отдельного enum-идентификатора на проводе нет.
Каждый тип имеет `optional value` (`uint32`, enum режима или `bool`).
У **SetProp** value обязательно присутствует, даже для нуля; у **GetProp** отсутствует.
Response повторяет request_id и тип свойства. Неразбираемый protobuf возвращает
MALFORMED_PACKET с request_id=0.

**GetState** одним запросом возвращает один логический Response со снимком всех 16
свойств: канал, номинальная частота, режим, инициализация и 12 регистров, включая
read-only StateRegister. У каждого свойства собственный status; отдельная ошибка
чтения не скрывает остальные значения. В снимок входит статус сохранения.
Обработка последовательная в основной задаче: другие BLE-команды не изменяют
настройки посреди снимка. Канал/режим — последнее известное программное состояние,
регистры читаются с устройства.

**SaveSettings** позволяет явно повторить сохранение. Обычно он не нужен:
каждый успешный SetProp автоматически сохраняет все настройки.
**GetSettingsInfo** возвращает только сведения о сохранении:

- saved — есть сохранённая запись;
- dirty — текущие настройки изменились или восстановление не удалось;
- revision — номер последней сохранённой записи;
- restore_status — результат восстановления именно при старте; при пустой/невалидной Flash это NO_SAVED_SETTINGS.

Если SetProp применился, но не сохранился, вложенный PropertyResult.status=OK,
а верхний Response.status сообщает STORAGE_ERROR (или IO_ERROR при чтении снимка).
Настройки в RAM/приёмнике уже изменены, dirty остаётся true; повторите SaveSettings.
Отсутствие ошибок SPI не является подтверждением физического присутствия приёмника.

Примеры protobuf text format:

```protobuf
request_id: 1
set_prop { property { channel_index { value: 0 } } }
```

Байты RX: `0d 01 00 00 00 12 06 0a 04 0a 02 08 00`.

```protobuf
request_id: 2
get_prop { property { synthesizer_reg_b {} } }
```

Байты RX: `0d 02 00 00 00 1a 05 0a 03 8a 01 00`.

```protobuf
request_id: 3
get_state {}
```

Байты RX: `0d 03 00 00 00 22 00` — **один пакет запрашивает всё состояние**.

## Фрагментация Response на TX

Любой Response, даже короткий, передаётся в одинаковом формате. Каждая notification:

| Байты | Значение |
| --- | --- |
| 0 | `0xA1`, версия формата |
| 1 | transfer_id, увеличивается для каждого Response, переполняется после 255 |
| 2 | index, начиная с 0 |
| 3 | count, общее число фрагментов |
| 4..19 | до 16 байт protobuf Response |

Фрагменты одного ответа идут последовательно, без перемешивания с другими ответами.
Склейте payload в порядке index, затем декодируйте **Response** и проверьте request_id.
Не декодируйте отдельный фрагмент как protobuf. Максимальный размер Response по схеме
263 байта (до 17 notifications); буфер прошивки — 512 байт. Это работает с MTU=23.
В `ble_codec.py` есть `ResponseAssembler`, который проверяет последовательность и
возвращает собранные байты. При reconnect/timeout его нужно сбрасывать.
Notifications не имеют прикладного ACK: при пропущенном фрагменте повторите GetState,
а не пытайтесь использовать неполный снимок.

```python
# После генерации Python-классов:
# python3 -m grpc_tools.protoc -I Protocol --python_out=Protocol Protocol/steadyview.proto
from steadyview_pb2 import Request, Response
from ble_codec import ResponseAssembler

request = Request(request_id=3)
request.get_state.SetInParent()
rx_bytes = request.SerializeToString()  # write RX with response=True
assembler = ResponseAssembler()

def on_notification(data):
    payload = assembler.feed(data)
    if payload is not None:
        response = Response.FromString(payload)
        print(response)  # все поля состояния и ошибки, без потери типов
```

## Сохранение во Flash

Зарезервированы страницы CPU1 `0x0807E000..0x0807FFFF` (8 КиБ).
Линкер ограничивает программу первыми 504 КиБ и не включает страницы настроек
в ELF/HEX/BIN. Обычная прошивка с постраничным стиранием приложения их сохраняет;
mass erase удаляет настройки.

Запись содержит версию, revision, выбранные канал/частоту/режим, признаки
достоверности, все 11 записываемых регистров, CRC32 и финальную отметку commit.
StateRegister — текущий статус, он читается по GetState, но не восстанавливается
как настройка. В каждой странице 32 записи; при заполнении стирается другая
страница. До завершения новой записи предыдущая валидная запись сохраняется.
Одинаковые настройки повторно не программируются.

При старте выбирается последняя завершённая запись с корректным CRC и версией.
Восстанавливаются режим и частота, затем сырые регистры, регистр power-down — последним.
Если записи нет или она невалидна, используются A1/Mix. Ошибка восстановления
видна в restore_status; выполняется попытка запуска с A1/Mix без перезаписи журнала.

Операции Flash согласованы с CPU2 через HSEM2/6/7 и уведомления об erase activity,
по образцу ST BLE_RfWithFlash. Запрашивается интервал BLE 50–80 мс для окон стирания;
центральное устройство вправе отказать. Если CPU2 не допускает операцию в течение
2 секунд, возвращается ошибка сохранения с dirty=true; старая запись остаётся.

## Свойства и регистры

| Сообщение | Значение / доступ |
| --- | --- |
| ChannelIndex | Set/Get, 0..47: восемь каналов каждой полосы A, B, E, F, R, L |
| VideoMode | Set/Get, `MIX=0`, `DIVERSITY=1` |
| FrequencyMHz | Get, номинальная частота выбранного канала |
| Initialize | Set `true`: канал A1 + Mix; Get: результат инициализации |
| SynthesizerRegA | `0x00`, Set/Get |
| SynthesizerRegB | `0x01`, Set/Get |
| SynthesizerRegC | `0x02`, Set/Get |
| SynthesizerRegD | `0x03`, Set/Get |
| VcoSwitchCapControl | `0x04`, Set/Get |
| DfcControl | `0x05`, Set/Get |
| SixMAudioDemodulatorControl | `0x06`, Set/Get |
| SixM5AudioDemodulatorControl | `0x07`, Set/Get |
| ReceiverControl1 | `0x08`, Set/Get |
| ReceiverControl2 | `0x09`, Set/Get |
| PowerDownControl | `0x0A`, Set/Get |
| StateRegister | `0x0F`, Get |

Регистры содержат **20-битное значение** `0..0xFFFFF`, без адреса и R/W-бита. Зарезервированные адреса не выставлены. StateRegister оставлен только для чтения.

Инициализация и восстановление сохранённых настроек выполняются автоматически в `defaultTask`. Высокоуровневые getters возвращают последнее успешно заданное состояние; getter регистра действительно читает линию PA7. Сырые записи в регистры сбрасывают достоверность cached channel/mode. `ChannelIndex` восстанавливается SetProp(ChannelIndex), режим — последующим SetProp(VideoMode), либо всё сразу через Initialize. До этого getter возвращает `NOT_INITIALIZED`.

`IO_ERROR` означает сбой SPI/таймаут. У шины нет ACK: успешная передача сама по себе не доказывает присутствие и правильную работу приёмника. `Initialize=true` сообщает об успешном выполнении последовательности, а не аппаратном обнаружении модуля.

## FIFO и выполнение

`control_service.c` копирует RX-данные в FIFO CMSIS-RTOS из 8 элементов; буфер события BLE не сохраняется. Callback не выполняет SPI и не разбирает protobuf. `ControlApp_Run()` вызывается из `StartDefaultTask()` в `main.c`: берёт FIFO → декодирует nanopb → выполняет действие → кодирует Response → фрагменты TX Notify.

ATT Write Response подтверждает только постановку в очередь. Результат операции приходит через TX. Ошибки ATT приложения:

- `0x80`: нет подписки TX или неподходящее соединение;
- `0x81`: очередь заполнена — запрос не принят, можно повторить позже;
- `0x82`: пустой или слишком длинный пакет;
- `0x83`: Prepare Write не поддерживается.

При занятости BLE TX-буферов удерживается готовый Response и текущий фрагмент, повторяется только отправка фрагмента. Команда повторно не выполняется. Disconnect и смена подписки инвалидируют очередь по номеру сессии. Уже начатая SPI-операция может завершиться после disconnect, но её ответ не предназначен новой сессии. `request_id` служит корреляции, не дедупликации: повторно присланный SetProp выполнится снова. После неопределённого исхода соединения клиент должен перечитать состояние.

## Аппаратный обмен

PA1 — SCK, PA7 — двунаправленный SDIO, PA4 — CS. Запись: **5 аппаратных кадров по 5 бит**, LSB first, один CS. Чтение: 5-битный адрес/RW, переключение BIDIOE, два аппаратных 10-битных кадра. Никакого MISO-провода не требуется; PA7 не должен быть подключён через однонаправленный буфер.

SPI mode 0, PCLK2/256 = **250 кГц**. Это минимальная частота SPI1 при сохранении APB2=64 МГц со схемы пользователя; она отличается от программного обмена ExpressLRS. Для Mix только SCK временно становится GPIO: HIGH 100 мс, LOW 500 мс при CS=HIGH; затем восстанавливается AF5.

Для остановки half-duplex RX используется окно внутри последнего кадра по RM0434. CPU1 IRQ маскируются примерно на 80 мкс (ограничение ожидания — 1 мс), CPU2 радио продолжает работать. Флаг BSY после master RX не используется согласно ES0394. Число тактов, turnaround PA7, рабочую частоту и чтение регистра необходимо подтвердить логическим анализатором на плате; автоматические тесты электрический интерфейс не проверяют.

## Источники и лицензия

Карта регистров, таблица каналов, формула синтезатора и последовательности Init/SendIndexCmd/SetMode адаптированы из [ExpressLRS/Backpack steadyview.h](https://github.com/ExpressLRS/Backpack/blob/master/src/steadyview.h) и [steadyview.cpp](https://github.com/ExpressLRS/Backpack/blob/master/src/steadyview.cpp), полученных 18.10.2026. Перенос находится в `Core/Src/steadyview.c`; лицензия GPLv3 сохранена в `LICENSES/Backpack-GPL-3.0.txt`. Прочие vendor-компоненты сохраняют свои лицензии.

Аппаратный SPI: [ST RM0434](https://www.st.com/resource/en/reference_manual/dm00318631.pdf), раздел Procedure for disabling the SPI; [ST ES0394](https://www.st.com/resource/en/errata_sheet/es0394-stm32wb55xxstm32wb35cx-device-errata-stmicroelectronics.pdf), §2.16.1.

## Пассфраза и доступ

Пустота ключа проверяется во **flash приёмника**, а не в localStorage браузера.
Пока приёмник не настроен, команды открыты, включая первую SetPassphrase.
Веб-пульт сначала предлагает установить пассфразу. После установки все команды состояния,
чтения/записи регистров и сохранения требуют SecureCommand; открыты только GetAuth и Authenticate.

1. GetAuth возвращает configured, authorized=false, salt (16 байт), challenge (32 случайных
   байта от CPU2 HCI LE Rand), iterations=600000. Каждый GetAuth сбрасывает авторизацию и счётчик.
   Salt = 3 слова STM32 UID в little-endian + ASCII `SV01`.
2. Браузер получает K = PBKDF2-HMAC-SHA256(UTF-8 пассфразы, salt, 600000, 32 байта).
   Строка не обрезается и не нормализуется. Веб-пульт требует минимум 12 символов для новой пассфразы.
3. При первой настройке SetPassphrase.key = K. После успешного сохранения выполнить новый GetAuth.
4. Authenticate.proof = HMAC-SHA256(K, ASCII `SV-AUTH-v1` || challenge).
   Proof одноразовый, включая неудачную попытку. При успехе authorized=true, sequence=0.
5. Каждая обычная команда кодируется в отдельный Request (payload, максимум 64 байта).
   Внешний Request содержит SecureCommand и **тот же request_id**, sequence от 1 до UINT32_MAX,
   mac = HMAC-SHA256(K, ASCII `SV-CMD-v1` || challenge || LE32(sequence) || payload).
   Принимается только следующий sequence. Валидная подпись расходует sequence даже при ошибке
   внутреннего protobuf или выполнении команды. SecureCommand нельзя вкладывать в себя.
   Неавторизованные/неподписанные команды возвращают AUTH_REQUIRED (9), неверная подпись — AUTH_FAILED (10).
6. Для смены пассфразы вычислить новый Knew с тем же salt. Во вложенном SetPassphrase передать
   key = Knew XOR HMAC-SHA256(K, ASCII `SV-KEY-v1` || challenge), затем подписать внешний SecureCommand.
   Успешное сохранение сбрасывает авторизацию: новый GetAuth + Authenticate с Knew.
   При ошибке flash старый ключ остаётся действительным; повторить с новым sequence.

Ключ занимает ранее зарезервированные 32 байта журнальной записи, защищён тем же CRC/commit.
Обычные сохранения настроек сохраняют ключ. Legacy-записи с 32 байтами FF означают отсутствие
пассфразы. Ключ из одних FF отклоняется. Reset приёмника (Initialize) не стирает ключ.
Без старой пассфразы смены по BLE нет: восстановление доступа через отладчик требует стереть
обе страницы настроек 0x0807E000..0x0807FFFF, что удалит и настройки приёмника.

Это авторизация команд, не шифрование радиоканала и не привязка к аппаратной идентичности телефона.
Первая установка передаёт производный ключ открыто — выполнять в доверенном окружении.
Подписываются команды; ответы не подписаны, телеметрия не зашифрована. Защита от активного
MITM первой настройки, физического чтения flash и радиопомех не заявляется.
Перехваченный challenge/proof позволяет офлайн-подбор слабой пассфразы; выбирайте длинную.
Сохранённый в браузере ключ эквивалентен праву доступа и защищён только профилем/origin браузера.
Другой клиент с той же пассфразой тоже сможет войти. Очистка браузера не снимает защиту приёмника.

Криптография: TinyCrypt v0.2.8 (Intel, BSD-3-Clause), vendored SHA-256/HMAC/utils;
браузер использует Web Crypto. Host-тесты покрывают RFC 4231, смену сессии, replay,
ошибки сохранения и смену ключа. При ошибке RNG доступ не выдаётся.
