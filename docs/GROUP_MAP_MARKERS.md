# Карточки отрядов на карте

Союзные отряды используют оформление машин логистики: квадрат «О» в цвете
фракции, тёмная карточка с двумя строками и подробности при наведении.
Например, `A0 · Атака · 8 чел.` / `Движется к цели`. Это пример отображения,
а не наблюдение игрового прогона.

В подробностях показаны позывной, сторона и роль, текущее действие, число живых
бойцов и плановый состав, состояние транспорта, назначенная база или точка,
расстояние до неё по прямой с направлением и источник приказа. Состояние
«Обходит препятствие» берётся из действующего recovery отряда. Приказ и близость
к цели не доказывают, что бойцы сейчас ведут огонь или имеют проходимый путь.

Позывные A0/D0/R0 пересчитываются при смене роли без пересоздания маркера.
Позиция следует живому leader/участнику; controller origin не используется.
Подпись и подробности публикует сервер, только при фактическом изменении.
Подробности входят в `RplProp` для JIP и читаются заново при создании виджета.
Callbacks отрядов и логистики изолированы по kind.

Общий `AICF_MapMarkerCardWidget` выравнивает символ «О»/«Л», добавляет отступы
и цветную полоску. Наведение поддерживается на значке и фоне; подробности
примыкают к карточке, чтобы курсор мог перейти между ними. Поднятый при hover
Z-order сбрасывается при уходе. Виджет принадлежит stock layout и не заводит
таймеров. Объективы атаки и метки приказов на точку сохраняют свои виды.

Ширина и высота обеих панелей подстраиваются под содержимое. Строки выровнены
по левому верхнему краю; только символ «О»/«Л» остаётся по центру значка.
Краткая подпись имеет отступы 8 px по горизонтали и 4 px по вертикали,
подробности — по 8 px. Это единицы reference resolution. Ширина текста
ограничена 240/360 px соответственно; длинные названия переносятся, а фон
растёт по высоте. Размеры шрифтов сохранены: 13/15. Постоянных панелей
250×44 и 500×184 больше нет. Размер пересчитывается при изменении текста и
открытии подробностей, включая обновления уже существующего маркера.

## Изменённые файлы

- `AIConflictCore/Scripts/Game/AIConflict/UI/AICF_GroupMapMarkers.c` — подпись,
  подробности, репликация и подключение общей карточки;
- `AIConflictCore/Scripts/Game/AIConflict/UI/AICF_MapMarkerCardWidget.c` — общий
  виджет отрядов и логистики;
- `AIConflictCore/Scripts/Game/AIConflict/UI/AICF_LogisticsMapMarkers.c` —
  извлечение прежнего локального виджета, read model логистики сохранён;
- `tools/Test-GroupMapMarkersStatic.ps1` — authority/JIP, callbacks, живые
  обновления, позиция, видимость и lifecycle hover;
- `tools/Test-Stage3Static.ps1`, `tools/Test-Stage3StaticContracts.ps1`,
  `tools/Test-Stage4Static.ps1` — проверки перенесённого поля техники и нового
  формата направления;
- `README.md`, `docs/ARCHITECTURE.md`, `docs/TESTING.md`,
  `docs/LOGISTICS_MAP_MARKERS.md` и этот документ — описание и evidence.

## Проверки 2026-09-08

Evidence: `.codex-runtime/group-marker-cards-20260908-221457/`.
`before/` хранит исходные версии основных изменяемых файлов; чужие и ранее
выполненные gameplay-изменения в рабочем дереве сохранены.

Шаблон запуска аудиторов:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage3Static.ps1
```

| Скрипт | До | После |
|---|---|---|
| `Test-Stage3Static.ps1` | PASS / 0 | PASS / 0 |
| `Test-Stage35Static.ps1` | PASS / 0 | PASS / 0 |
| `Test-Stage3StaticContracts.ps1` | PASS / 0 | PASS / 0 |
| `Test-Stage4Static.ps1` | PASS / 0 | PASS / 0 |
| `Test-MapPointOrdersStatic.ps1` | PASS / 0 | PASS / 0 |
| `Test-LogisticsMapMarkersStatic.ps1` | PASS / 0 | PASS / 0 |
| `Test-GroupMapMarkersStatic.ps1` | Новый аудитор | PASS / 0 |

Baseline failures отсутствуют. Отрицательные fixtures по-прежнему обнаруживают
удаление источника состояния транспорта, placeholder и выводимого аргумента.
Переход с `ТЕХНИКА %5` на `Техника: %1` и с `DIR` на русское направление
является изменением presentation contract, не обходом проверки данных.

Терминальный `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent
-gproj <root>/addon.gproj -addonsDir <installed addons,repo[,RHS]>
-addons <dependency graph> -logsDir <evidence> -wbModule=ScriptEditor -run
-validate` выполнен для Everon, Arland и RHS. Полные аргументы каждого запуска
сохранены в `workbench-*-args.json`, логи — в `workbench-*`.

Все три root: **PASS / exit 0**, `Game successfully created`,
`Script validation successful`, `SCRIPT (E/F)=0`, `ENGINE (F)=0`, VM/null errors=0.
Stock warnings и resource leak diagnostics на закрытии Workbench не являются
нулевым error log и не использованы как доказательство runtime.

Server/client runtime, фактическая репликация/JIP и визуальная проверка —
**NOT RUN** для этой правки. Компиляция и статические контракты их не заменяют.

Ручная проверка: открыть карту за обе стороны, сравнить карточки «О»/«Л»,
навести курсор на значок и подпись, проверить длинные названия, перекрытие
соседних маркеров и читаемость на масштабе пользователя. Затем проверить
обновление после смены роли/приказа, потерь и входа нового клиента. Здесь агент
не открывал и не управлял игровым UI.

## Компактное отображение — 2026-09-09

По пользовательскому снимку выявлено лишнее пространство фиксированных панелей.
Изменён только общий `UI/AICF_MapMarkerCardWidget.c` и документация:
`docs/GROUP_MAP_MARKERS.md`, `docs/ARCHITECTURE.md`. Модель данных, сетевые поля,
текст состояния и lifecycle markers сохраняются. Новый measurement использует
`TextWidget.GetTextSize()` из pinned API 1.8.0.13: результат уже задан в reference
resolution и не требует дополнительного `DPIUnscale`. Сначала измеряется текст
без переноса, затем его высота при ограниченной ширине. При изменении высоты
подписи подробности остаются вплотную к её нижнему краю.

Evidence: `.codex-runtime/compact-marker-cards-20260909/`, исходная версия
`card-before.c`, git status, `before-*.log`, `after-*.log`, результаты аудиторов,
source SHA256 и терминальный Workbench.

Команды `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/<имя>.ps1`:
`Test-GroupMapMarkersStatic`, `Test-LogisticsMapMarkersStatic`, `Test-Stage4Static`,
`Test-SupplyMapUIStatic` — PASS / 0 до и после изменения; baseline failures нет.

Терминальный Workbench `-wbModule=ScriptEditor -run -validate` для Arland,
Everon и RHS — PASS / exit 0; в каждом полном логе есть `Game successfully
created` и `Script validation successful`, без `SCRIPT (E/F)`, `ENGINE (F)` и
VM/null errors. Наборы прежних resource errors не изменились: 25 stock и 26 RHS,
delta пуст. Source SHA256 после compile совпал. `git diff --check` — PASS / 0.
Команды, аргументы и полный output сохранены в evidence.

Текущая игровая сессия не перезапускалась; она продолжает использовать ранее
загруженную сборку. Runtime/визуальная проверка компактных размеров — NOT RUN.
Для просмотра этого изменения нужен следующий запуск сервера и клиента.

Ручной критерий после перезапуска сборки: короткая карточка занимает только
площадь текста с отступами, длинное название переносится без обрезки, после
смены цели фон уменьшается обратно. Проверить обе категории маркеров и переход
курсора из подписи в подробности. Автоматическая визуальная проверка — NOT RUN.
