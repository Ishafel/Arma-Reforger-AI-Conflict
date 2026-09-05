# Поиск строительной площадки: уточнение от 2026-09-05

Продолжение реализации `AI_COMMANDER_CONSTRUCTION_PROMPT.md` после
пользовательского server log `Server-20260905-144206-530`.
Исходные незакоммиченные construction changes сохранены; пользовательский
prompt, другие domains и project GUID не изменялись.

## Причины отказов и изменения

Первый сохранённый срез пользовательского лога содержит девять `NO_SAFE_SITE`,
один `COMMIT_REVALIDATION_FAILED` и два завершения (small/light US). Лог в момент
обращения продолжал записываться, поэтому этот срез используется для диагностики,
а не как остановленный acceptance run. По разрешению пользователя процессы
server 12196 и client 13468 завершены; окончательный файл также сохранён.

В исходном поиске были следующие ограничения:

- Те же 64 кандидата повторялись для каждого нового заказа; ориентация была
  связана с азимутом, а начальный радиус составлял `25 + half diagonal`.
- Один общий AABB включал оба двадцатиметровых выезда всех depot slots.
  Вся эта площадь, включая выезды и запас 4 метра, проверялась как фундамент
  с допустимым перепадом всего 0.4 метра; дорога под выездом запрещалась.
- `TraceFlags.WORLD` включал terrain в физическую проверку выше 0.45 метра
  относительно origin, хотя отдельная terrain grid уже проверяла рельеф.
- Любой service/spawn controller из broad phase блокировал площадку без
  проверки фактической точки. Большие bounds таких entities не равны занятому
  зданием объёму. Новый runtime явно показал такие ложные пересечения.

Теперь используются следующие правила:

| Область | Поведение |
|---|---|
| Кандидаты | 128 разных центров до повторного обхода; все восемь orientations представлены с первых ticks. Основные точки расположены за габаритом нового здания вокруг HQ; каждая четвёртая исследует полный radius. После восьми обходов продолжается последовательность новых центров |
| Продолжение | Отдельный cursor каждой базы/типа сохраняется после отказа и размещения; незаконченная из-за deadline проверка текущей позиции повторяется полностью в следующем order, исчерпание query budget не теряет кандидата |
| Ограничения | До 96 попыток на order, прежние deadline 120 секунд, cadence/cooldown 60 секунд, 4 кандидата/tick и 96 queries/window |
| Footprint | Полная геометрия composition/outline и реальные spawn envelopes; запас 1 метр |
| Terrain | Локальная сетка 3 метра по footprint без пустых углов world AABB и без коридора; перепад до 0.8 метра |
| Физика | Ориентированный `TraceOBB` против entities, включая камни, деревья, здания, персонажей и технику; terrain/water проверяются отдельно |
| Служебные точки | Фактическая spawn/service position с защитой подхода 2 метра; у vehicle slot — настоящий envelope из component metadata; unfinished compositions и pending reservations продолжают блокировать пересечения |
| Дорога | Broad phase по общему объёму, narrow phase по полным дочерним meshes с учётом поворота; пустой угол общего AABB сам по себе не закрывает дорогу |
| Выезд depot | Каждый vehicle slot получает один свободный 20-метровый коридор из двух направлений; проверяются будущая геометрия самого depot, существующие препятствия, world/provider bounds и reservations |
| Terrain выезда | Сетка 3 метра, перепад до 2 метров, разность соседних высот не более 25% расстояния; вода запрещена, дорога допустима |
| Commit/completion | Повторная physical validation footprint и выбранных коридоров; reservations учитывают оба вида объёмов также для AICF vehicle spawning |
| Рабочая точка | Endpoint за footprint и вне выбранных выездов с запасом 2 метра; при пересечении выбирается следующая сторона здания |

Полная стоимость, reserve, один debit, rollback, provider identity, authority,
worker/tool progress и штатная service activation не менялись.
Ограничения остаются консервативными; `NO_SAFE_SITE` допустим и не означает,
что исчерпаны все возможные ручные placement transforms.

API проверены в закреплённом Script Diff `1.8.0.13`:
`Core/generated/World/TraceFlags.c`, `TraceOBB.c`, `TraceParam.c`,
`Game/Components/Spawner/SCR_EntitySpawnerSlotComponent.c` и
`SCR_CampaignBuildingPlacingObstructionEditorComponent.c`.
В `TraceFlags` значение `WORLD` означает именно terrain; `ENTS` сохраняет
физическую проверку static/dynamic entities.

## Диагностика

- `CONSTRUCTION_SITE_REJECTED`: первый пример каждой причины в order,
  позиция, yaw и prefab конкретного препятствия.
- `CONSTRUCTION_SEARCH_SUMMARY`: `search_offset`, `exits`, `terrain_delta`,
  `broadphase_ignored` и счётчики `rejected_<reason>` для всех отказов.
- `CONSTRUCTION_BROADPHASE_IGNORED`: пример служебного controller, чей broad-phase
  bounds пересёк query, но фактическая точка с защитой подхода осталась снаружи.
- `CONSTRUCTION_EXIT_METADATA`: оба направления каждого реального vehicle slot
  и пересечения с будущими meshes самого depot.

Существующие event names/fields сохранены. Примеры ограничены одним на причину
внутри order; полные логи остаются источником runtime verdict.

## Evidence и проверки

Ignored evidence: `.codex-runtime/construction-sites-20260905/`.
Сохранены начальные версии пяти production файлов, `before-*.txt`, `after-*.txt`,
срезы пользовательского лога, Workbench logs и launcher manifests.

Baseline до правки:

| Команда (`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/...`) | Verdict |
|---|---|
| Test-ConstructionStatic.ps1 | PASS |
| Test-ConstructionContracts.ps1 | PASS, 13 log inputs и positive/negative static inputs |
| Test-BaseBuildersStatic.ps1 | PASS |
| Test-AICommanderModeStatic.ps1 | PASS |
| Test-Stage4Static.ps1 | PASS |
| Test-Stage35RecoveryPolicy.ps1 | PASS |
| Test-RHSIntegrationStatic.ps1 | PASS |
| Test-MapPointOrdersStatic.ps1 | PASS |
| Test-Stage3Static.ps1 | FAIL: STAGE3_PROGRESS_EVIDENCE, STAGE3_BOUNDED_PROTECTED_CLEARANCE, STAGE3_MARKER_STATE |
| Test-Stage35Static.ps1 | FAIL: STAGE35_MEANINGFUL_TASK_PROOF, STAGE35_BOUNDED_PROTECTED_CLEARANCE |

Старые аудиты не ослаблялись. ConstructionStatic дополнительно требует
продолжение cursor, отдельный exit validation/reservation, narrow phase
служебных точек/дорог и диагностические events.

Workbench выполнялся терминальной командой из `DEVELOPMENT.md`:

```powershell
& 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe' `
  -noThrow -wbsilent -gproj "$PWD\AIConflictArland\addon.gproj" `
  -addonsDir "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger\addons,$PWD" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E' `
  -logsDir "$PWD\.codex-runtime\construction-sites-20260905\wb-delivery-final-arland" `
  -wbModule=ScriptEditor -run -validate | Out-Null
```

Для Everon/RHS используются root project и полный graph из `DEVELOPMENT.md`.
Первый sandbox-запуск `wb-probe-1` скомпилировал Game, но не прошёл Validate
из-за недоступного Steam API, native exit -1. Следующие терминальные запуски
с доступом к Steam прошли Validate; итоговые gates перечисляются ниже.

Runtime запускался только canonical launcher с временной копией штатной
test-only fixture в Core:

```powershell
& .\tools\Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -AdditionalArguments @('-aicfConstructionProbe','1',
    '-aicfConstructionProbeRefill','1','-aicfConstructionProbeMs','360000',
    '-aicfConstructionProbeType','4','-aicfConstructionProbeRepeatType','4',
    '-aicfRequirePlayerForResult','0') |
  Tee-Object '.codex-runtime/construction-sites-20260905/runtime-stock-5-launch.txt'
```

Fixture обеспечивает supplies и первым проверяет heavy; поиск, оплата и
worker completion остаются production. Это не чистое количественное A/B
сравнение с пользовательской игровой сессией: различаются supplies и порядок
типов. Цена, debit, geometry и worker gates не обходятся.

`runtime-stock-1` завершился до `ROSTER_READY`: порт 2001 был занят старым
сервером. `-bindPort 2002` в этом local `-server` запуске не изменил фактический
порт; exact CLI и отказ RPL сохранены. Этот run не является construction test.

`runtime-stock-2` / `Server-20260905-151414-330` проверял первую итерацию:
small US у исходной базы достроен инструментом, supplies `1000 → 750`, props
`58 → 129`, service ONLINE. `Test-ConstructionLog -RequireCompletion` — PASS,
20 orders, 1 placement, 1 completion. Широкие false positives у служебных точек
обнаружены этим прогоном и уточнены в следующей итерации.

`runtime-stock-3` / `Server-20260905-152500-538`: два small US достроены
инструментом, `Test-ConstructionLog -RequireCompletion` — PASS (16 orders,
2 placements/completions). В логе есть фактические broad-phase false positives:
например, controller supplies spawn point находился снаружи footprint вместе
с двухметровым подходом, но участвовал в широком query. В этом run обнаружена
потеря кандидата на deadline после успешного footprint/exit sampling; исправлена
повторной полной проверкой такой позиции в следующем order.

`runtime-stock-4` / `Server-20260905-153538-820`: focused повторение heavy через
test-only `-aicfConstructionProbeRepeatType 4`. US heavy прошёл поиск за
23 кандидата/22 секунды, два выезда проверены; `1000 → 700` supplies,
`49 → 149` props, completion инструментом и `STOCK_SERVICE_ONLINE` в 15:38:53.
Затем на той же базе достроен small: `1600 → 1350`, `149 → 220` props.
Это захваченная база `0x200000000000010E`, не исходный HQ. У исходных HQ
наблюдались повторные searches с `search_offset=96` и последующими значениями;
heavy completion там этим run не доказан.
`Test-ConstructionLog -RequireCompletion` — PASS (16 orders, 2 placements,
2 completions, 2 types); native exit 0. 598 ticks / 600413 ms,
max tick 94 ms, max queries/window 80. Supplies спустя пять секунд `701`
соответствуют штатному росту пула; props остаются ровно 149.

Последний endpoint guard отдельно проверяется runtime fixture шестью native
вызовами production `WorkClearOfExits`: отсутствие коридора, точка внутри,
точка в защитном отступе, свободная точка, затем внутренний/внешний варианты
после поворота и переноса. Marker: `CONSTRUCTION_ENDPOINT_CONTRACT passed=6 total=6`.

`runtime-stock-5` / `Server-20260905-154614-805` использует окончательный код,
тот же focused heavy setup, duration 360000 ms. Шесть endpoint cases прошли.
В этом world стартовые стороны поменялись местами; на захваченной базе
`0x200000000000010E` построен уже USSR heavy. Поиск: 23 кандидата / 21 секунда,
два выезда, terrain delta 0.172 м. Оплата `1000 → 700`, props `49 → 151`;
`BUILDER_COMPLETED` и `STOCK_SERVICE_ONLINE` в 15:49:26 подтверждают реальную
работу и окончательную активацию. Это дополняет US heavy из предыдущего run.
`Test-ConstructionLog -RequireCompletion` — PASS (11 orders, 1 placement,
1 completion), native exit 0; `Game destroyed.` в 15:52:51. 360 ticks /
360982 ms, max tick 77 ms, max queries/window 94 при лимите 96. Через пять
секунд props остаются 151, supplies 701 соответствуют штатному росту пула.

Проверки первой доработки (до повторного сообщения о проблеме США):

| Gate | Verdict / evidence |
|---|---|
| ConstructionStatic / ConstructionContracts | PASS, exit 0; 13 log inputs и positive/negative static inputs |
| Шесть исходных зелёных аудитов | PASS, exit 0 |
| Stage3 / Stage35 | Те же 3 + 2 исходных failures, exit 1; `baseline-comparison.json` |
| Workbench Arland / Everon / RHS без fixture | PASS, `Script validation successful`, native exit 0; `wb-delivery-final-*` |
| Endpoint native contracts | PASS, 6/6, runtime-stock-5 |
| Финальный остановленный Stock runtime | PASS, `Test-ConstructionLog -RequireCompletion`, exit 0; `runtime-stock-5-audit.txt` |
| `git diff --check` | PASS |
| RHS / Everon runtime новой геометрии | NOT RUN; их Workbench compile выполнен |

Последний server завершился штатно. Server/client процессов после проверки
нет; временная runtime fixture удалена из Core. Существующий пользовательский
Workbench не останавливался. Commit не создавался.

Полные остановленные server logs и engine indices сохраняются отдельно;
существующие world/resource diagnostics не удаляются из evidence.
Все supply/props и performance числа относятся к конкретным profiles, не
являются гарантией времени native вызова или чистым A/B benchmark.

## Изменённые файлы этого продолжения

Пути ниже относительно корня репозитория:

- `AIConflictCore/Scripts/Game/AIConflict/Config/AICF_ConstructionConfig.c`
- `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_ConstructionOrder.c`
- `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_ConstructionMetadata.c`
- `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_ConstructionSiteSearch.c`
- `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_ConstructionPlanner.c`
- `tools/Test-ConstructionStatic.ps1`
- `tools/fixtures/AICF_ConstructionRuntimeProbe.c`
- `docs/CONSTRUCTION_VALIDATION.md`
- `docs/CONSTRUCTION_SITE_SEARCH_VALIDATION.md`

Полная matrix всех пяти типов × factions × maps, client/JIP, движущийся blocker
и ручная проверка проезда/входов остаются отдельными gates; successful search
и Workbench не заменяют их. Работа не объявляется `ACCEPTED`.

## Подготовка 0.1.10

Повторная игровая сессия `Server-20260905-155833-212` подтвердила проблему:
первые три поиска США завершились `NO_SAFE_SITE`, в то время как СССР
достроил малые казармы и лёгкий depot. В первом поиске США из 96 кандидатов
54 отклонены физической проверкой, 27 — препятствием/резервом, 7 — рельефом,
5 — дорогой, 3 — водой. Это не подтверждение отсутствия ручных мест.

Последующая доработка, включённая в коммит:

- Готовый composition controller больше не блокирует весь свой общий bounds.
  Его реальные meshes, service points и spawn envelopes по-прежнему защищены;
  незавершённый layout сохраняет резерв будущей геометрии.
- Поиск использует 128 различных центров и все восемь поворотов с начала
  последовательности, сохраняя cursor каждой базы/типа и прежние budgets.
- Physics проверяет дочерние mesh/spawn объёмы вместо одного сплошного
  параллелепипеда всей composition. Metadata объединяет их до 24 OBB;
  объединение только расширяет bounds и не исключает части prefab.
  Нижний порог geometry остаётся 0.3 м; рельеф проверяется отдельно.
- До синхронной live validation проверяется наличие всей квоты queries;
  учитываются только выполненные запросы. Нехватка квоты при commit оставляет
  готовую площадку для повторной полной проверки на следующем tick.
- Test-only fixture получила подробный индекс отказов и пять native contracts
  распределения кандидатов. В Core её копии перед коммитом нет.

Два промежуточных остановленных Stock прогона:
`Server-20260905-161122-169` и `Server-20260905-161818-753`.
Они выполнялись **до** последнего перехода к дочерним collision volumes.
Первый прошёл `Test-ConstructionLog.ps1 -ExpectedMode BOTH -RequireCompletion`
(exit 0: 6 orders, 2 placed, 1 completed), но completion был у СССР.
Во втором native contracts нового распределения дали 5/5; первый поиск США
снова не разместил здание. Эти результаты не закрывают исходную проблему США.

Финальные команды и evidence сохранены в
`.codex-runtime/construction-us-20260905/precommit/`:

| Команда / gate | Verdict |
|---|---|
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionStatic.ps1` | PASS, exit 0 |
| `Test-ConstructionContracts.ps1` с теми же PowerShell flags | PASS, exit 0; 13 log inputs и positive/negative static inputs |
| `Test-BaseBuildersStatic.ps1`, `Test-AICommanderModeStatic.ps1`, `Test-Stage4Static.ps1`, `Test-Stage35RecoveryPolicy.ps1`, `Test-RHSIntegrationStatic.ps1`, `Test-MapPointOrdersStatic.ps1` | PASS, exit 0 |
| `Test-Stage3Static.ps1` | Сохранены `STAGE3_PROGRESS_EVIDENCE`, `STAGE3_BOUNDED_PROTECTED_CLEARANCE`, `STAGE3_MARKER_STATE`; exit 1 |
| `Test-Stage35Static.ps1` | Сохранены `STAGE35_MEANINGFUL_TASK_PROOF`, `STAGE35_BOUNDED_PROTECTED_CLEARANCE`; exit 1 |
| Терминальный `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent ... -wbModule=ScriptEditor -run -validate` по трём командам из `DEVELOPMENT.md` | Arland/Everon/RHS: PASS без fixture, native exit 0, `Script validation successful`, script/VM errors отсутствуют; `workbench.json`, `wb-*/console.log` |
| Runtime последней геометрии, производительность объединения volumes, строительство США на стартовой базе | NOT RUN после последнего изменения |
| Player placement без дубля, все пять типов, client/JIP, ручная проверка входов/выездов | NOT RUN для последнего изменения |

Коммит и Git tag выполняются по отдельному указанию пользователя. Они не
означают приёмку runtime или публикацию Workshop addon. Запущенные позднее
пользователем server/client/Workbench при подготовке коммита не останавливались.
