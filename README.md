# SteadyView BLE controller

Проект для **STM32WB55CGU6, UFQFPN48**: BLE peripheral, FreeRTOS (CMSIS-RTOS v2), SPI1 и RTC. Сборка через CMake и GNU Arm Embedded Toolchain. Основа — транспорт и инициализация из ST BLE_HeartRateFreeRTOS; демонстрационный пульсометр удалён.

## Выводы

| Сигнал | Вывод | Настройка |
| --- | --- | --- |
| SPI1 SCK | PA1 | AF5 |
| SPI1 MOSI | PA7 | AF5, двунаправленный SDIO без отдельного MISO |
| SPI CS | PA4 | GPIO output, active low, в покое high |
| HSE | OSC_IN / OSC_OUT | кварц 32 МГц |
| SWD | PA13 / PA14 | отладка и прошивка |

PA4 расположен между PA1 и PA7 по периметру корпуса. Назначения SPI сверены с [datasheet ST, DS11929](https://www.st.com.cn/resource/en/datasheet/stm32wb55vg.pdf).

SPI используется для протокола SteadyView: аппаратная передача 25 бит LSB-first,
чтение по той же PA7 (half-duplex), CS на PA4. Частота — 250 кГц.
При старте `defaultTask` восстанавливает настройки из Flash (по умолчанию A1/Mix), затем обрабатывает FIFO
пакетов BLE через nanopb. [Описание протокола, UUID и примеры](Protocol/README.md).

## Тактирование и RTC

- HSE 32 МГц → PLL M=2, N=8, R=2 → SYSCLK/CPU1 64 МГц.
- CPU2 32 МГц; AHB4, APB1 и APB2 64 МГц.
- RTC и RNG от LSI1, номинально 32 кГц; RF wake-up от HSE/1024 = 31,25 кГц.
- SMPS от HSI, 4 МГц. LSE не требуется.
- FreeRTOS SysTick — 1 кГц; HAL timebase — TIM17.

RTC: 24 часа, делители 15 и 1999, начальная дата 01.01.2000. Метка в backup DR1 предотвращает повторную установку даты после обычного сброса. RTC wake-up используется таймерным сервером BLE — не перенастраивать его для пользовательских таймеров. Время можно читать через HAL_RTC_GetTime и затем HAL_RTC_GetDate. LSI не обеспечивает точность кварцевых часов и не поддерживает ход RTC только от VBAT при отключённом VDD.

STOP/tickless отключены, чтобы сохранять HSE для BLE и стабильное тактирование SPI. Для энергосберегающих режимов потребуется отдельная настройка.

## Сборка

Нужны `arm-none-eabi-gcc` с newlib, CMake >= 3.20 и Ninja:

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

Результат: `build/steadyview_ble_ctrl.elf`, `.hex`, `.bin`, `.map`. Код CPU1 размещён с 0x08000000, ограничен нижними 504 КиБ Flash; следующие 8 КиБ зарезервированы для журнала настроек; общая память CPU1/CPU2 выделена отдельными секциями линкера.

`steadyview_ble_ctrl.ioc` содержит настройки корпуса, выводов и периферии для CubeMX. Он адаптирован вручную; открытие и генерация в CubeMX здесь не проверялись. Источник проверенной сборки — CMake и C-файлы. Генерацию CubeMX следует выполнять в отдельный каталог: она не воспроизводит ручные изменения приложения автоматически.

## BLE и прошивка

Устройство **SteadyView** предоставляет Device Information и кастомный сервис с RX Write / TX Notify. `SetProp` и `GetProp` используют типизированный protobuf `oneof`. На RX принимаются ATT-записи до 20 байт; Request до 128 байт собирается из фрагментов в основном потоке, который обрабатывает FIFO из восьми пакетов. Response разбивается на TX-фрагменты. GetState одним запросом возвращает всё состояние; настройки автоматически сохраняются во Flash после SetProp. Подробности — в [Protocol/README.md](Protocol/README.md).

CPU2 должен содержать совместимый BLE stack. В `Wireless/stm32wb5x_BLE_Stack_full_fw.bin` сохранён стек из исходного CubeWB вместе с `Wireless/Release_Notes.html`. Устанавливать через STM32CubeProgrammer/FUS по этим release notes; адрес зависит от версии стека/FUS. Этот файл нельзя прошивать как приложение CPU1 по адресу 0x08000000.

После установки стека прошить CPU1 файлом `.hex` или `.elf`. На реальной плате проверить рекламу SteadyView, подключение, ход RTC и сигналы SPI осциллографом. Сборка проверена GNU Arm GCC 10.3.1; оборудование не прошивалось, работа радио и SPI на плате не проверялась. В исходниках ST остаются предупреждения о неиспользуемых параметрах и неполном инициализаторе debug-конфигурации.

## Состав

`Core/` — инициализация, FreeRTOS и SPI; `STM32_WPAN/` — BLE-приложение и IPCC; `Vendor/` — только используемые исходники и заголовки HAL, CMSIS, FreeRTOS, WPAN и LPM с лицензиями; `Wireless/` — стек CPU2 и инструкция ST. Зависимости скопированы в репозиторий, submodules не требуются. Остальные примеры, платы и middleware CubeWB удалены; история Git сохранена.

## Nanopb

В `Vendor/Middlewares/Third_Party/nanopb/` включены runtime и генератор
[nanopb 0.4.9.1](https://github.com/nanopb/nanopb/tree/0.4.9.1), лицензия и
контрольная сумма исходного архива (`UPSTREAM.txt`). `pb_common.c`, `pb_encode.c`
и `pb_decode.c` компилируются вместе с прошивкой с теми же флагами Cortex-M4/FPU.
Из приложения доступны `#include "pb_encode.h"` и `#include "pb_decode.h"`.
Динамическая память nanopb по умолчанию отключена (`PB_ENABLE_MALLOC` не задан).

Для обычной сборки CMake Python и protoc не нужны. Для генерации описаний сообщений
создайте Python-окружение и установите зависимости генератора:

```sh
python3 -m venv .venv-nanopb
.venv-nanopb/bin/python -m pip install protobuf grpcio-tools
```

Схема уже находится в `Protocol/steadyview.proto`, сгенерированные файлы включены в CMake.
Для обновления из активированного Python-окружения:

```sh
./Protocol/regenerate.sh
```

Проверка протокола, драйвера с моделью регистров и FIFO/BLE с подменой стека:

```sh
./tests/run.sh
```

Тесты запускаются на хосте с AddressSanitizer/UBSan. Они покрывают 48 каналов,
режимы, Request/Response, снимок состояния, ошибки, очередь, переподключение,
фрагментацию RX/TX, восстановление настроек, прерывания записи журнала, HMAC, replay и смену ключа.
Аппаратное чтение SPI и радио проверяются отдельно на плате.

## Веб-пульт

Готовая страница в `web/dist`, без сборки и внешних зависимостей: Web Bluetooth, все каналы,
режимы, регистры, чтение состояния и сохранение. Пассфраза защищает команды в прошивке;
браузер может запоминать производный ключ для конкретного приёмника. Первая установка ключа открыта,
смена — только после авторизации. Ключ сохраняется во flash вместе с настройками.

Для GitHub Pages включить **Settings → Pages → Source → GitHub Actions** и запустить
workflow **Deploy Web Bluetooth controller**. Инструкция, локальный запуск и ограничения
WebView — в [web/README.md](web/README.md). Публикуется только `web/dist`.
