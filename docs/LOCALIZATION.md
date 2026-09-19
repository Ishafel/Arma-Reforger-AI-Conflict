# Язык сценария и интерфейса AI Conflict

AI Conflict использует язык **клиента**: `ru_ru` — русский; остальные
поддерживаемые языки Reforger — английский. Сервер не выбирает язык игроков.
На одном сервере клиенты могут использовать разные языки. После смены языка
в настройках следует заново открыть карту/форму, чтобы пересоздать постоянные
надписи; уже открытые формы не имеют отдельной подписки на смену языка.

Локализованы названия/описания четырёх сценариев, HUD, командование, роли,
состояния и задачи отрядов, кнопки приказов, метки отрядов/логистики/точек,
направления и расстояния, форма снабжения и её ответы, редактор экипировки,
его категории, подписи, подсказки и сообщения.
Позывные, пользовательские названия комплектов, идентификаторы RPC и коды
диагностики остаются данными. Названия штатных баз, оружия и машин переводятся
через собственные string tables vanilla/RHS; отсутствующие переводы чужого
контента AI Conflict не подменяет.

## Ресурсы и граница представления

- `AIConflictCore/Language/AICF_Localization.st` — 382 строки с
  `Target_en_us` и `Target_ru_ru`.
- `AICF_Localization.en_us.conf` и `AICF_Localization.ru_ru.conf` —
  runtime tables с соответствующими `.meta`.
- `AIConflictCore/addon.gproj` регистрирует таблицы; HEADLESS и консольные
  конфигурации наследуют PC. Для других поддерживаемых языков явно
  зарегистрирована английская таблица.
- Четыре `Missions/AICF_*.conf` используют обычные `#AICF_Scenario_*`
  в `m_sName`, `m_sDescription`, `m_sDetails`.
- `UI/AICF_Localization.c` создаёт сообщения с ключом и параметрами;
  `Resolve` выполняется перед отображением. `Key` сохраняет исходные
  `#AR-*` и другие stock/RHS ключи до клиента.
- `UI/AICF_LocalizedStaticMarker.c` переводит AICF custom text только
  серверных static markers непосредственно в stock widget.

Формат `{AICF:key}` представляет простой текст; `Format` добавляет девять
параметров с длинами. Это позволяет вложить состояния, имена и числовые значения,
не переводя их на сервере и не используя разделители campaign summary
`|`, `~`, `;`. Число и порядок 14 полей group summary не изменены.
Исходные `RplProp`, owner/faction scope, JIP и change-only `BumpMe`
сохранены. Ни таблицы, ни перевод не меняют игровое состояние.

`Resolve` ограничен глубиной 8 и 128 токенами; повреждённая строка сохраняется
без попытки продолжить разбор неизвестных параметров. Переносы `\n` в
config-тексте преобразуются **в шаблоне до подстановки**, поэтому содержимое
параметров не переписывается. Производственный код не вызывает `SetLanguage`.

Нативная схема ресурсов сверена с
[примером Bohemia](https://github.com/BohemiaInteractive/Arma-Reforger-Samples/tree/main/SampleMod_NewWeapon/Language)
и [руководством по локализации](https://community.bohemia.net/wiki/Arma_Reforger:Mod_Localisation).
Сигнатуры WidgetManager/string и marker widget проверены в закреплённом
Script Diff `1.8.0.13`.

## Изменённые области

В `AIConflictCore/Scripts/Game/AIConflict/`:

| Файлы | Изменение |
|---|---|
| `UI/AICF_Localization.c`, `UI/AICF_LocalizedStaticMarker.c` | Новая клиентская граница перевода |
| `UI/AICF_StrategicUI.c` | HUD, меню командования, отображение state codes |
| `UI/AICF_GroupMapMarkers.c`, `UI/AICF_LogisticsMapMarkers.c`, `UI/AICF_MapMarkerCardWidget.c` | Ключи в server data, перевод при отображении |
| `UI/AICF_LogisticsArrivalEstimate.c`, `UI/AICF_SupplyMapUI.c`, `UI/AICF_SupplyMapData.c`, `UI/AICF_SupplyTransportRpc.c` | Снабжение, ETA и ответы |
| `UI/AICF_LoadoutEditor.c`, `UI/AICF_LoadoutItemGrid.c`, `UI/AICF_LoadoutSlotView.c`, `UI/AICF_LoadoutMagazines.c` | Редактор и подсказки экипировки |
| `Loadouts/AICF_LoadoutCatalog.c`, `Loadouts/AICF_LoadoutService.c` | Запасные подписи предметов и стандартное имя комплекта |
| `Bootstrap/AICF_MatchController.c`, `Economy/AICF_ManualSupplyDispatch.c`, `State/Vehicles/AICF_TransportTripRegistry.c`, `Vehicles/AICF_VehicleCoordinator.c` | Только построение текста/параметров; игровые решения и ownership не менялись |

В `tools/` добавлены `AICFLocalization.Common.ps1`,
`Build-AICFLocalization.ps1`, `Test-LocalizationStatic.ps1` и
`fixtures/AICF_LocalizationProbe.c`. Проверки ScenarioHeaders, Stage3,
Stage3StaticContracts, Stage4, AICommanderMode и MapPointOrders адаптированы к
ссылкам на таблицу. Проверки русского/английского текста разворачивают именно
используемые ключи; неизвестный ключ не считается найденным текстом.

## Обновление переводов

Менять нужно `.st`, сохраняя ID и параметры `%1..%9`; затем:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Build-AICFLocalization.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Build-AICFLocalization.ps1 -Check
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LocalizationStatic.ps1
```

Runtime `.conf` коммитятся вместе с исходной таблицей. Это необходимые
ресурсы локализации, а не `resourceDatabase.rdb`.
Новые client labels проходят через `Resolve`. В сетевых производителях
запрещено заранее вызывать `WidgetManager.Translate`.

## Проверки 2026-09-19

Исходный HEAD: `8880e46621ec4ce3cfc788858c68b487c4a0d2e7`.
До правок в рабочем дереве были только пользовательские untracked
`docs/AI_LOGISTICS_IMPLEMENTATION_PROMPT.md` и
`docs/VEHICLE_SPAWN_PARITY_PROMPT.md`; они не изменены.
Game/Server/Tools: `1.8.0.13`.

Evidence:
`C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict\.codex-runtime\localization-20260919\`.
Файлы `before-*.txt` и `after-*.txt` сохраняют результаты аудиторов.

Все команды ниже использовали
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/<имя>.ps1`:

| Команды | До → после |
|---|---|
| `Test-ScenarioHeadersStatic`, `Test-Stage3Static`, `Test-Stage35Static`, `Test-Stage4Static` | PASS / 0 → PASS / 0 |
| `Test-GroupMapMarkersStatic`, `Test-LogisticsMapMarkersStatic`, `Test-SupplyMapUIStatic`, `Test-ManualSupplyStatic` | PASS / 0 → PASS / 0 |
| `Test-AILoadoutStatic`, `Test-MapPointOrdersStatic`, `Test-RHSIntegrationStatic`, `Test-RuntimeLauncherStatic`, `Test-LogisticsStatic` | PASS / 0 → PASS / 0 |
| `Test-AICommanderModeStatic` | Сохранён FAIL / 1, один прежний `AI_COMMANDER_UI_STATE` |
| `Test-LocalizationStatic`, `Build-AICFLocalization -Check` | Новые проверки: PASS / 0 |
| `Test-Stage3StaticContracts` | PASS / 0, parser fixtures и 10 negative runs |
| `Test-ScenarioHeadersStatic -RepositoryRoot <scenario-negative>` | Ожидаемый FAIL / 1, `SCENARIO_MENU_VISIBILITY` при отключённой плитке |

Терминальный Workbench Validate по командам `docs/DEVELOPMENT.md`:
Arland, Everon, ArlandRHS, EveronRHS — PASS / 0 после удаления fixture.
Полные логи лежат в `wb-final-<variant>/console.log`, stdout —
`wb-final-<variant>-output.txt`. Нет `SCRIPT (E/F)`, `ENGINE (F)`,
VM/null errors проекта; stock resource leak diagnostics при завершении
сохранены в полных логах.

Runtime fixture временно копировалась в Core/UI; перед final audits/Workbench
она удалена. Команды из отдельных терминальных сессий:

```powershell
& tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -AdditionalArguments @('-aicfLocalizationProbe','1','-aicfRequirePlayerForResult','0',
    '-addr','127.0.0.1:22029')

& tools/Start-AICFRuntime.ps1 -Role Client -Variant Stock `
  -ServerProfileRoot 'C:\Users\retar\AppData\Local\AICF\Server-20260919-145159-190' `
  -ServerPort 22029 -ClientAddress '127.0.0.1:22029' `
  -AdditionalArguments @('-aicfLocalizationProbe','1')
```

Launcher подтвердил exact CLI, live server и `ROSTER_READY` перед запуском
клиента; оба `AICF_RUNTIME_MANIFEST_JSON` сохранены в
`server-final-launch.txt` и `client-final-launch.txt`.

- Server: `C:\Users\retar\AppData\Local\AICF\Server-20260919-145159-190\logs\logs_2026-09-19_14-51-59\console.log`,
  14:51:59–14:55:24 MSK; exit 0, fixture `RequestClose`.
- Client: `C:\Users\retar\AppData\Local\AICF\Client-20260919-145233-439\logs\logs_2026-09-19_14-52-34\console.log`,
  14:52:34–14:53:03 MSK; exit 0, fixture `RequestClose`.
- На каждом peer: `cases=15 failures=0`. Проверены сценарий, метки/состояния,
  вложенные параметры, Unicode/разделители, переносы, повреждённое сообщение,
  английский fallback для `de_de`, неизменность wire string и 14 полей summary.
  Поздно подключившийся клиент прочитал существующую server summary:
  `On foot / Airport Base` и `Пешком / Аэродромная база`.

Это **PASS целевой проверки локализации**, а не общий clean-runtime PASS.
Полные остановленные логи содержат stock ошибки ресурсов
`SCR_AIDangerReaction_UnsafeArea`, `SlidingTrackMaterial`, `Parent`,
duplicate `Hierarchy`, client `SCR_WidgetExportRuleRoot`, resource leaks
и две server ошибки resupply catalog при shutdown. В server log также есть
transient unauthorized-client diagnostic перед успешным подключением.
Новых AICF runtime errors, VM/null exceptions не обнаружено.

Первые попытки отдельного сервера завершились на занятом порту 2001:
`-port` и `-bindPort` не меняют адрес direct `-server` режима.
Рабочий параметр — `-addr`; существующая игровая сессия не останавливалась.
Первый работающий probe выявил буквальные `\n`; исправление подтверждено
приведённым финальным прогоном.

**NOT RUN:** ручной визуальный verdict по ширине/переносам всех элементов,
одновременные два клиента с разными языками, весь интерактивный сценарий
редактора/снабжения на каждом языке, отдельный runtime Everon/RHS, консоли,
packaged Workshop build и soak. Серверная и клиентская fixture проверяют
реальный перевод и JIP summary, но не заменяют эти gates.
