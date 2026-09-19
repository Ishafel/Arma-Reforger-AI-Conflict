# Скорость поиска площадок: 2026-09-19

Исходный HEAD: `8ac8014b9a6618f1388333a88627fbfe84c54c21`, с сохранёнными
пользовательскими изменениями. Evidence:
`.codex-runtime/construction-speed-20260919/`. Все запуски — terminal-only,
через `tools/Start-AICFRuntime.ps1`, отдельный порт `2011`, отдельные source
copies и profiles. Пользовательский server/client не перезапускались.

## Причина

Исходный пользовательский log сохранён как `user-snapshot.log`. В нём нет
shutdown marker; это снимок наблюдавшегося матча, а не остановленный runtime
gate. От первого решения HQ СССР до размещения light depot прошло около
1040 секунд; собственная работа строителя заняла ещё 64 секунды. У больших
казарм последний успешный order искал площадку 114.5 секунды, а достройка
заняла 109.5 секунды. Между первой потребностью и этим order происходили
другие поиски, deadline, cooldown и переключение типов.

В последовательном изолированном baseline два первых HQ orders потратили
6846 и 4433 единицы query budget, исследовав только 5 и 23 трансформации.
За 15 минут ни одно здание HQ не было размещено. Геометрия больших казарм
подготовилась за 73.2/83.3 секунды wall time; CPU metadata США — 225 мс.
Geometry уже кэшировалась по prefab, поэтому повторное построение metadata
не объясняло многоминутные повторные циклы.

Профилирование промежуточной версии: у первого HQ США path занимал
5546 из 5838 списанных единиц квоты; terrain — 224, candidate physics/bounds —
67, inventory — 1. CPU поиска — 152 мс на 140.7 секунды order, включая
20 секунд metadata. `budget_wait_windows=39` — число секундных окон, в
которых была отказана квота; это **не** CPU time и не точная длительность
всего ожидания. Дополнительное ожидание создают секундный scheduler и
ограниченные batches. Уменьшение повторных A* queries само по себе не
устранило deadline.

## Реализация

- Сначала bounds и дешёвые access guards, затем physics.
  При нехватке полной квоты initial live-check сохраняется тот же candidate,
  без нового `GetSurfaceY` и повторного BeginCandidate.
- A* отбрасывает закрытые/неулучшаемые узлы и запрещённые отрезки до native
  queries. Уже спроецированная позиция узла используется в рамках того же
  пути; каждое новое принятое ребро всё равно проверяется `RayTrace`.
- После короткой порции пути кандидат сохраняется в
  `AICF_ConstructionCandidate`. До восьми checkpoints позволяют проверить
  другие площадки до исчерпания всех 4096 path queries одним трудным
  кандидатом. Restore сохраняет transform, полный terrain envelope,
  выбранные выезды, spawn/start variant, endpoints, A* и уже потраченный
  candidate path budget. Чужие queries не засчитываются этому кандидату,
  но и его прежний расход не обнуляется. Из сохранённых путей первым
  продолжается наиболее приблизившийся к рабочему endpoint; при равенстве
  используется FIFO. Квота между базами остаётся round-robin.
- Незавершённый order может продолжиться после прежнего cooldown,
  сохранив token, identity и checkpoints. Не более трёх окон по прежнему
  deadline: с defaults максимум 360 секунд активных окон и две паузы по
  60 секунд, плюс отдельная metadata preparation. Лимит 256 новых
  трансформаций действует на **весь** order. После окончательного отказа
  cursor идёт вперёд, без повторного восстановления последнего кандидата
  с нуля. Это также сохраняет приоритет текущих стартовых казарм до
  ограниченного завершения поиска.
- Commit/completion делят прежние 96 queries с поиском. FIFO claim
  резервирует полную синхронную проверку ближайшего ожидающего final check;
  для commit резерв включает также предшествующий inventory query.
  Именно владелец первого claim может использовать эту единицу, даже когда
  конкурирующий поиск исчерпал весь нерезервированный остаток. Активный
  commit обновляет срок claim в общем lifecycle проходе независимо от
  посещения scheduler; выбранная площадка не уходит в search cooldown.
  Остаток доступен поиску. Claim освобождается при обслуживании/отмене и
  имеет срок жизни для удалённого потребителя. Слишком маленькая CLI-квота
  даёт fail-closed `LIVE_QUERY_LIMIT_TOO_SMALL`.
- Остаток общего 8-мс slice передаётся в terrain/exits/path. Синхронную
  окончательную проверку и один native call нельзя прервать посередине;
  runtime max tick остаётся отдельным измерением.

Footprint, margin, water/slope, physics, roads, exits, stock services,
vehicle reservations, exact provider/base/faction identity, economy и
completion guards сохраняются. Перед commit/completion повторно проверяются
также актуальные building/world bounds. Gameplay entities для примерки не
создаются. Metadata и config limits не менялись.

Два дополнительных варианта — длинные шаги и multi-source A* — проверялись
в изолированных промежуточных запусках, но не устранили первый deadline и
**не входят** в итоговые исходники. Их полные логи и exact-profile forced
stop evidence сохранены; они не являются завершёнными runtime gates.

## Измерения и ограничения

`tools/Measure-ConstructionSearch.ps1` читает полный log и сохраняет
`orders.json`, `events.csv`, `summary.json`. Отдельно выводятся metadata,
search, decision→placement и placement→completion. `queries` — списанные
единицы общей квоты, включая консервативные precharges существующих
проверок; это не количество площадок и не точный счётчик всех engine API
calls. `candidates` считает новые трансформации; восстановление checkpoint
его не увеличивает. `CONSTRUCTION_SEARCH_COST` — накопительные счётчики,
повторные записи одного order нельзя суммировать.

Baseline и итоговый after используют stock северный Everon, `BOTH`, 15 минут,
одинаковые defaults, refill supplies и одинаковую пару HQ: США — авиабаза,
СССР — госпиталь. Fixture фиксирует только распределение выбранной пары до
stock initialization. Это обратное распределение по сравнению с исходным
пользовательским матчем; сравнивать их как идентичные матчи нельзя. Combat,
rosters и вторичные базы остаются динамическими.

Baseline: native exit 0, `CONSTRUCTION_PROBE_DONE`, полные остановленные
`before-profile-2/logs/`, копия `before-console.log`. Construction log audit
с completion — PASS, builders с tool use — PASS; 27 orders, 9 placements,
8 completions вне HQ, 6 completion waits. Max construction tick — 310 мс
на первом холодном решении; max query window — 96. Отдельно сохранены 54
resource/world/entity/material E diagnostics, без SCRIPT E/F и ENGINE F.
PASS этих двух анализаторов не означает отсутствие engine diagnostics.

Первый сопоставимый after (до уточнения резерва inventory): `priority-profile`, native exit 0, штатный
`CONSTRUCTION_PROBE_DONE` и `Game destroyed.`, полный `priority-console.log`.
На авиабазе США размещены и завершены три стартовых здания. В baseline на
обоих HQ не было ни размещений, ни завершений за те же 15 минут.

| Авиабаза США, after | Metadata, с | Поиск после metadata, с | Решение → размещение, с | Размещение → завершение, с | Новые площадки | Query budget до размещения |
|---|---:|---:|---:|---:|---:|---:|
| SMALL_BARRACKS | 25.920 | 43.192 | 69.117 | 57.113 | 21 | 1770 |
| LIGHT_DEPOT | 6.993 | 73.130 | 80.128 | 46.279 | 35 | 3330 |
| LARGE_BARRACKS | 63.357 | 97.445 | 160.808 | 34.161 | 75 | 3574 |
| HEAVY_DEPOT | 18.063 | Не завершён | Не размещён | — | 101 | 6462 к остановке |

Выбор площадки предшествовал размещению на 5–6 мс. От первого решения HQ
до размещения трёх зданий прошло 69.1 / 206.4 / 413.5 секунды соответственно.
Сравнение первого размещения с отсутствием его в 15-минутном baseline даёт
нижнюю границу ускорения более 12 раз; точный коэффициент неизвестен,
поскольку baseline закончился без результата HQ. Для heavy depot ускорение
этим прогоном **не доказано**. На HQ СССР у госпиталя малые казармы получили
`NO_SAFE_SITE` после двух продолжений, 97 площадок и 12709 queries; последующий
light depot не завершил поиск к остановке. Это ограниченный отказ поиска,
а не доказательство геометрической невозможности строительства на всей базе.

| Весь сопоставимый прогон | Before | After |
|---|---:|---:|
| Orders с решением | 27 | 18 |
| Максимум одновременно pending orders, включая паузы | 7 | 8 |
| Размещено / завершено | 9 / 8 | 8 / 8 |
| Из них размещено на HQ | 0 | 3 |
| Новые candidate attempts | 627 | 1298 |
| Повторные индексы кандидатов между orders | 2 | 0 |
| Списанные queries orders | 62923 | 61667 |
| Окончательные NO_SAFE_SITE | 10 | 2 |
| Паузы с сохранением состояния | 0 | 14 |
| Completion waits | 6 | 5 |
| Max query window | 96 | 96 |
| Средний FPS, 90 samples | 59.936 | 59.910 |
| Max construction tick, мс | 310 | 512 |
| Construction ticks >20 / >50 мс | 2 / 1 | 3 / 1 |
| Max engine frame, мс | 335.1 | 539.3 |

Всего завершений не стало больше: выигрыш этого прогона — ранняя стартовая
инфраструктура HQ и больше обследованных площадок при той же общей квоте.
Поиск конкурировал с заказами на захваченных базах, а completion — с поиском.

Нагрузка не стала равной нулю: оба запуска имеют холодный spike первого
решения. В after это 512 мс при одной списанной query, ещё до начала поиска
площадки; последующие ticks >20 мс — 33 и 23 мс. Непрерывных зависаний поиск
не показал, но улучшение максимального холодного frame **не подтверждено**.
FPS включает остальные подсистемы и динамический бой; это одно
последовательное сравнение, без статистической оценки дисперсии.

На первых трёх успешных HQ orders до выбора площадки path расходовал
571 / 497 / 426 queries; terrain — 896 / 1176 / 2704; exits — 0 / 1178 / 0;
candidate checks — 275 / 447 / 416. Число окон с отказом квоты — 11 / 9 / 34.
В `priority-profile` последний `CONSTRUCTION_SEARCH_COST` успешного order записан перед commit,
поэтому его phase counters не включают последующий commit/completion.
Общий `queries` order включает commit, но legacy counter не включает
временный completion-check order. Эти числа нельзя выдавать за полный
счётчик native engine вызовов. Completion waits отражены отдельно; новые
FIFO и checkpoint контракты прошли в native runtime: 14/14 и 6/6.
В окончательных исходниках дополнительный cost snapshot пишется при
завершении; анализатор отдельно отмечает наличие наблюдённого completion
cost и не подменяет отсутствие этого измерения нулевым расходом.

Оба обычных after log audits — PASS. Расширенный
`-RequireAllTypes -RequireAllFactions` — FAIL: нет completed ARMORY обоих
сторон, HEAVY_DEPOT обоих сторон и LARGE_BARRACKS СССР. Этот gate не ослаблен.
Stock coverage ARMORY и ненайденные площадки не являются успешным выполнением
матрицы создания всех типов. В полном stopped log сохранены 38 E diagnostics:
ENTITY 14, MATERIAL 2, RESOURCES 14, WORLD 8; SCRIPT E/F и ENGINE F отсутствуют.

Зеркальный прогон `mirror-profile` на `priority-source` (до уточнения inventory)
проверил исходное распределение: СССР на
авиабазе, США у госпиталя. Он также завершился штатно за 15 минут, exit 0;
полный log — `mirror-console.log`. На авиабазе завершены все четыре
стартовых типа:

| Авиабаза СССР | Metadata, с | Поиск после metadata, с | Решение → размещение, с | Размещение → завершение, с | Площадки | Queries до размещения |
|---|---:|---:|---:|---:|---:|---:|
| SMALL_BARRACKS | 13.900 | 111.228 | 125.132 | 52.112 | 17 | 5059 |
| LIGHT_DEPOT | 5.016 | 99.262 | 105.281 | 50.213 | 35 | 4241 |
| LARGE_BARRACKS | 36.097 | 90.361 | 126.462 | 29.113 | 75 | 3770 |
| HEAVY_DEPOT | 19.013 | 217.864 | 237.882 | 48.143 | 83 | 4780 |

От первой потребности HQ до готовности LIGHT_DEPOT / LARGE_BARRACKS /
HEAVY_DEPOT прошло 5:33 / 8:08 / 12:54; в исходном пользовательском снимке —
18:24 / 25:09 / 26:33. Это наблюдательное сопоставление, а не идентичная пара
before/after: исходный матч уже шёл с другими динамическими условиями.
Builder/economy код и скорость строительства не менялись; время после
размещения включает достижение рабочего места.

У госпиталя США малые казармы также получили ограниченный `NO_SAFE_SITE`
(118 площадок, 14207 queries), light depot не выбран к остановке. Весь
mirror: 17 orders, 9 placements и 9 completions, 1156 candidates,
58950 legacy order queries, 0 повторных индексов, 2 окончательных
`NO_SAFE_SITE`, 10 пауз, 6 completion waits, max query window 96.
Средний FPS 59.917; единственный construction tick >20 мс — холодные 491 мс.

**Mirror runtime audits — FAIL**, несмотря на завершённые здания: в полном
log две SCRIPT E штатных `SCR_AISoundHandling` / `SCR_AICommsHandler`
(`REPORT_CONTACT`, отсутствует entity/position, 22:12:01). Источник текста
проверен в pinned Script Diff 1.8.0.13, `scripts/Game/AI/Talk/SCR_AISoundHandling.c:258`.
Эти ошибки не исключались из анализаторов и не исправлялись в установленной
игре. Другие E diagnostics: ENTITY 14, MATERIAL 2, RESOURCES 14, WORLD 8.
Таким образом, завершения и времена наблюдались, но чистый runtime gate
этого зеркального запуска не пройден.

### Окончательные исходники: verified

`verified-profile` / `verified-console.log`: 15 минут, native exit 0,
штатные `CONSTRUCTION_PROBE_DONE` и `Game destroyed.`. SHA256 production
construction-файлов совпадают с runtime snapshot; см. `final-source-parity.json`
и `verified-source-hashes.csv`. HQ совпадают с baseline: США — авиабаза,
СССР — госпиталь. Включены окончательный inventory reserve и final cost log.

| Авиабаза США, verified | Metadata, с | Поиск после metadata, с | Решение → размещение, с | Размещение → завершение, с | Площадки | Queries order / completion |
|---|---:|---:|---:|---:|---:|---:|
| SMALL_BARRACKS | 19.919 | 101.314 | 122.250 | 56.113 | 21 | 5378 / 26 |
| LIGHT_DEPOT | 5.996 | 84.231 | 91.248 | 25.096 | 33 | 3074 / 30 |
| LARGE_BARRACKS | 50.133 | 198.860 | 248.998 | 51.195 | 83 | 5575 / 26 |
| HEAVY_DEPOT | 18.134 | Не завершён | Не размещён | — | 87 | 4943 / 0 |

Поиск больших казарм включает одну прежнюю паузу 60 секунд. Выбор площадки
предшествовал размещению на 1.017 / 1.021 / 0.005 секунды. От первой
потребности HQ до размещения — 2:02 / 4:30 / 9:04. В baseline все эти
размещения отсутствуют к 15 минутам: для первого размещения подтверждена
нижняя граница ускорения более 7 раз. Ранний `priority` дал 69 секунд;
разброс до 122 секунд в `verified` сохранён, а не скрыт выбором лучшего опыта.
Для heavy США срок не установлен. У госпиталя СССР малые казармы получили
`NO_SAFE_SITE` после 97 площадок и 12458 queries, light depot не найден
к остановке после 136 площадок / 3691 queries.

Весь verified: 17 orders, 8 placements / 7 completions (один secondary layout
не достроен к остановке), до 7 одновременно pending orders, 1315 candidate
attempts, 0 повторных индексов, 61039 legacy order queries плюс отдельно
наблюдённые 190 completion queries семи завершений. Окончательных
`NO_SAFE_SITE` — 2, пауз — 12, completion waits — 1; max query window — 96.
Для сравнения baseline: 627 attempts, 2 повторных индекса, 62923 legacy
queries, 10 `NO_SAFE_SITE`, 6 completion waits. Простого увеличения лимита
queries или новых кандидатов за update нет; каждый order ограничен 256
новыми трансформациями, до 8 checkpoints и 3 окнами поиска.

FPS: 90 samples, средний 59.926 против 59.936 baseline. Max construction
tick 405 мс против 310; оба — первое холодное решение. В verified ещё два
ticks >20 мс: 22 и 24 мс. Max engine frame 432.4 мс против 335.1. Нет
подтверждения, что холодный spike устранён; отсутствие длительных повторных
зависаний не следует превращать в обещание нулевых пауз.

Native contracts: FIFO **14/14**, checkpoint **6/6**, inventory reservation
и сохранение живого claim **7/7**. Последний контракт расходует весь
нерезервированный остаток конкурентным поиском и всё равно завершает
inventory + live batch ровно в общей квоте 96.

**Оба обычных verified runtime audits — FAIL / 1** по единственному
классу engine/script errors. После `CONSTRUCTION_PROBE_DONE`, непосредственно
перед `Game destroyed.`, в 22:32:29 выведены две SCRIPT E:
`'SCR_BaseResupplySupportStationComponent' needs a entity catalog manager!`.
Текст принадлежит `SCR_BaseItemSupportStationComponent.InitValidSetup()`
в pinned Script Diff (`scripts/Game/Components/SupportStation/SCR_BaseItemSupportStationComponent.c:34`).
Это наблюдение при teardown; причинная независимость от изменённого
тайминга строительства не доказана. Ошибки не подавлялись и не добавлялись
в allowlist. Чистый полный runtime gate окончательной версии остаётся
**не пройден**, хотя placement/payment/builder проверки других нарушений
не сообщили. E diagnostics также включают ENTITY 14, MATERIAL 2,
RESOURCES 14, WORLD 8. Расширенная матрица всех типов/фракций тоже FAIL:
кроме SCRIPT E, не завершены ARMORY/HEAVY обеих сторон и LIGHT/LARGE СССР.

### Заведомо тесная зона

`tight-profile` / `tight-console.log`: отдельные 3 минуты на окончательном
`verified-source`, native exit 0 и полный stopped log. Fixture сужает
`GetBuildingRadius()` обоих stock HQ до 1 м; полный footprint заведомо не
помещается. Это искусственно заданный отрицательный случай bounds, а не
утверждение, что естественная территория госпиталя глобально непригодна.

Обе HQ завершили поиск `NO_SAFE_SITE`, по 240 candidate transforms и 241
query на order. От metadata ready до отказа — 120.279 / 120.282 секунды.
Все отказы: `OUTSIDE_BUILDING_OR_WORLD_BOUNDS`; ни одной выбранной площадки,
размещения, оплаты или паузы continuation. Max query window — 4, единственный
construction tick >20 мс — холодные 305 мс. Construction log audit —
**PASS / 0**; `check-tight.ps1` — **PASS / 0**, все 10 условий true.
Полный log содержит прежние 38 ENTITY/MATERIAL/RESOURCES/WORLD E diagnostics,
без SCRIPT E/F, ENGINE F и VM exception. Manifests и полные profile logs
сохранены; естественный hospital case отдельно остаётся ограниченным
неуспешным поиском с неизвестной глобальной пригодностью.

## Команды и gates

Baseline до изменений: `ConstructionStatic`, `ConstructionContracts`,
`BaseBuildersStatic`, `Stage4Static` — PASS / 0. `AICommanderModeStatic` —
сохранённый FAIL / 1, два прежних `AI_COMMANDER_UI_STATE`.

На окончательных исходниках те же четыре профильных проверки — PASS / 0;
`ConstructionContracts` проверил 16 log inputs и positive/16 negative static
inputs. `AICommanderModeStatic` сохранил ровно те же два failures.
Workbench production Arland / Everon / RHS — PASS / 0,
`Script validation successful.`, SCRIPT E/F и ENGINE F отсутствуют.
Полные логи `production-*-20260919-2216*`, exact args JSON и verdict summary
`production-final-validation.json` сохранены. Resource leaks / deprecated API
warnings Workbench не скрыты; native exit 0 сам по себе не использовался как
доказательство компиляции.

```powershell
pwsh -NoProfile -File tools/Test-ConstructionStatic.ps1
pwsh -NoProfile -File tools/Test-ConstructionContracts.ps1 -EvidenceRoot .codex-runtime/construction-speed-20260919/contracts-verified
pwsh -NoProfile -File tools/Test-BaseBuildersStatic.ps1
pwsh -NoProfile -File tools/Test-Stage4Static.ps1
pwsh -NoProfile -File tools/Test-AICommanderModeStatic.ps1
pwsh -NoProfile -File tools/Measure-ConstructionSearch.ps1 -LogPath <stopped-console.log> -OutputDirectory <metrics-directory>
pwsh -NoProfile -File tools/Test-ConstructionLog.ps1 -LogPath <stopped-console.log> -ExpectedMode BOTH -RequireCompletion
pwsh -NoProfile -File tools/Test-BaseBuildersLog.ps1 -LogPath <stopped-console.log> -RequireToolUse
pwsh -NoProfile -File .codex-runtime/construction-speed-20260919/compile-production.ps1
pwsh -NoProfile -File .codex-runtime/construction-speed-20260919/run-verified.ps1
pwsh -NoProfile -File .codex-runtime/construction-speed-20260919/run-tight.ps1
pwsh -NoProfile -File .codex-runtime/construction-speed-20260919/check-tight.ps1
```

| Gate / команда | Verdict |
|---|---|
| `Test-ConstructionStatic.ps1`, `Test-ConstructionContracts.ps1`, `Test-BaseBuildersStatic.ps1`, `Test-Stage4Static.ps1` до/после | PASS / 0 |
| `Test-AICommanderModeStatic.ps1` до/после | FAIL / 1, те же два `AI_COMMANDER_UI_STATE` |
| `compile-production.ps1`, production Arland/Everon/RHS | PASS / 0, validation successful во всех трёх |
| `compile-verified-source.ps1`, native fixtures | PASS / 0 |
| Canonical runtime launchers before/priority/mirror/verified/tight | native exit 0, manifests и stopped logs сохранены |
| `Test-ConstructionLog.ps1 -RequireCompletion` и `Test-BaseBuildersLog.ps1 -RequireToolUse`, before/priority | PASS / 0 |
| Те же audits, mirror | FAIL / 1: две SCRIPT E AI radio contact |
| Те же audits, verified | FAIL / 1: две SCRIPT E support station при teardown |
| `Test-ConstructionLog.ps1 -RequireAllTypes -RequireAllFactions`, after runs | FAIL / 1: неполная матрица; mirror/verified также SCRIPT E |
| `Test-ConstructionLog.ps1` без completion requirement, tight | PASS / 0 |
| `check-tight.ps1` | PASS / 0, 10/10 отрицательных условий |
| `git diff --check` | PASS / 0 |

Exact native commands/versions/source hashes/manifests находятся в evidence.
`run-before.ps1`, `run-priority.ps1`, `run-mirror.ps1`, `run-verified.ps1`,
`run-tight.ps1` используют canonical launcher.
`compile-verified-source.ps1` проверяет isolated fixtures; `compile-production.ps1`
проверяет production Arland/Everon/RHS graphs через документированный Diag CLI.
Fixtures копируются только в isolated source, в production Core их нет.

## Изменённые файлы

Пути construction-файлов ниже относительны
`AIConflictCore/Scripts/Game/AIConflict/Construction/`.

| Файл | Изменение |
|---|---|
| `AICF_ConstructionCandidate.c` (новый) | Checkpoint geometry/path и сохранение потраченного path budget |
| `AICF_ConstructionOrder.c` | Состояние продолжения, ограниченная очередь кандидатов, раздельные cost counters |
| `AICF_ConstructionPath.c` | Дешёвый отсев до queries, reuse navmesh projection, приоритет по достигнутому расстоянию |
| `AICF_ConstructionPlanner.c` | Продолжение кандидатов и orders, использование оставшегося slice, lifecycle claim и final cost snapshot |
| `AICF_ConstructionSiteSearch.c` | Продолжение initial check, общий slice, FIFO reserve commit/completion вместе с inventory |
| `tools/Measure-ConstructionSearch.ps1` (новый) | Времена этапов, queries, повторные индексы и наблюдаемость completion cost |
| `tools/Test-ConstructionStatic.ps1` | Контракты сохранения прогресса, ограничений и live reservation |
| `tools/Test-ConstructionContracts.ps1` | Дополнительные негативные static fixtures |
| `tools/fixtures/AICF_ConstructionBudgetProbe.c` (новый) | Native budget/checkpoint/commit contracts, HQ assignment и tight bounds только в isolated source |
| `docs/CONSTRUCTION_VALIDATION.md` | Ссылка на результаты и явный конечный предел continuation |
| `docs/CONSTRUCTION_SPEED_20260919.md` (новый) | Причина, измерения, команды, verdicts и ограничения |

Несвязанные пользовательские изменения сохранены. Установленные world,
mission и игровые файлы не изменялись; экспериментальные source copies,
profiles, manifests и полные logs остаются в игнорируемом evidence-каталоге.

**NOT RUN:** client/JIP, ручная визуальная проверка footprint/проходов,
RHS runtime и отдельные runtime-инъекции ownership/player placement во время
сохранённого checkpoint. Их не заменяют static audits и Workbench compile.
Универсальная пригодность всех площадок не установлена. Ускорение измерено,
но чистый полный runtime gate окончательной версии не пройден; статус
ACCEPTED не присваивается.
