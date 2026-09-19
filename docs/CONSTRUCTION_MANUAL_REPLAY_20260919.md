# Почему бот пропустил две пользовательские площадки

Проверка Stock Arland, 2026-09-19, Game/Server/Tools `1.8.0.13`.
HEAD `8cdc67a93e2611ad498a3ae02f696c1bf8e8f034`, с ранее выполненными
исправлениями в dirty tree. Задача этого шага — установить причины поиска
на двух конкретных местах, а не объявить всю construction matrix успешной.

## Исходные факты

Пользователь поставил две большие казармы СССР на северном HQ.
Prefab: `{D44C687485600B72}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_LivingArea_L_Conflict_USSR_01.et`.

| Место | Position | Yaw/pitch/roll | Фактическая рабочая точка |
|---|---|---|---|
| 1, дальше от палатки | `<1281.96,9.58667,3268.84>` | `<0,1.78991,-4.46499>` | `<1272.45,8.38841,3276.36>` |
| 2, ближе к палатке | `<1222.1,0.939056,3268.57>` | `<0,0,-0.895174>` | `<1231.61,0.998901,3276.07>` |

Provider: `<1158.09,1.34825,3279.28>`. Строитель появился около
`<1160.66,1.15625,3278.93>`, добрался до первого проекта и затем до второго.
Оба `BUILDER_COMPLETED` содержат `tool_active=1 item_using=1`:
`t_ms=154693` и `196881`. Исходные server/client profiles и запись player
placement приведены в [matrix report](CONSTRUCTION_MATRIX_20260919.md).

Автономный order в этой же сессии отменился с `PLAYER_PLACEMENT` до начала
кандидатов. Следовательно, нельзя утверждать, что бот в исходной сессии
проверил именно эти две точки и отказал. Для причинного сравнения нужен replay.

## Метод

Изолированная копия тех же production исходников плюс
`tools/fixtures/AICF_ConstructionReplayProbe.c`. Все 128 production `.c`
Core совпадают по SHA-256. Fixture подавляет новые construction orders,
но оставляет штатные roster, world, physics и navmesh. Она не размещает
здание, не списывает supplies и не ослабляет production guards.

Для каждого transform независимо вызываются production bounds, `LiveClear`,
`AccessClear`, terrain/exit этап `Step` и `ValidatePath`, затем `LiveClear`
с измеренным диапазоном высот. Проверки продолжаются после первого отказа
**только для диагностики**. Это не разрешение placement при проваленном guard.
Отдельно вычисляется ближайший из первых 256 production кандидатов.

Каждая точка проверяется с записанным наклоном и с yaw-only transform бота,
у которого root Y берётся из `GetSurfaceY`. Дополнительно ограниченный
navmesh алгоритм проверяется на фактическом endpoint рабочего, без запрета
входа в общий прямоугольник будущей композиции.

Мир загружен заново, построек пользователя в нём нет. Nearest master provider
в replay находится в 0.727 м от записанного: распределение HQ между фракциями
и объекты палатки могут различаться. Для генерации кандидатов финальный replay
использует именно записанный provider origin. Это воспроизведение статического
места и prefab, не побитовое восстановление всей игровой сессии.

Evidence: `.codex-runtime/construction-replay-20260919/`.
`play-server-stopped.log`, `play-client-stopped.log`, `play-stop.json` сохраняют
исходную игровую сессию, остановленную по identity процессов для нового мира.
Эта принудительная остановка сама по себе не является runtime PASS.

## Результаты и причины

Границы радиуса 150 м и доступ к остальным сервисам проходят для обеих точек.
Причина не в том, что площадки находятся за пределами базы.

**Место 2:** на записанном transform проходят physics и terrain, включая
последующую проверку после измерения высот. `ValidatePath` возвращает
`WORKER_ENDPOINT_UNREACHABLE`. Это ложный вывод о пригодности всей площадки:
фактический строитель выполнил работу на принятом stock проекте.

`ValidatePath` не выполняет полноценный поиск пути. Оно проверяет четыре
точки снаружи общего world AABB композиции; к каждой допускается прямой
navmesh ray или два луча через одну из 24 точек в 8/16/24 м от старта.
Неудача этой ограниченной проверки трактуется как недостижимость. Кроме
того, `SegmentIntersects` запрещает путь через весь будущий прямоугольник,
включая пустые места между объектами. Реальный builder использует stock
перемещение и, для player layout, endpoint у контура проекта. В обоих
рассматриваемых случаях его endpoint находится внутри общего metadata AABB.

**Место 1:** дополнительно отклоняется `PHYSICAL_OBSTRUCTION` с prefab
`Granite_Cluster_Large_01.et` и `SLOPE_OR_FOUNDATION_GAP`. Сам факт успешной
ручной постановки не доказывает отсутствия пересечения каждого mesh с камнем:
это расхождение строгих автоматических критериев с player admission, а не
основание отключать все проверки rocks/slope. Также возникает отказ пути.

**Выбор кандидатов:** 256 попыток — всего 64 разных центра с четырьмя
поворотами. Выборка оставляет большие непроверенные промежутки. Для первого
и второго места ближайшие корни находятся в **34.7725 м** и **8.38586 м**
от ручных позиций (финальный replay, исходный provider origin);
повороты повторяют центры и не закрывают эти промежутки. Поэтому увеличение
числа поворотов само по себе эти площадки не находит.

| Case | Место / transform | Bounds / access | Physics до / после terrain | Terrain | Path |
|---|---|---|---|---|---|
| 0 | 1, ручной наклон | PASS / PASS | Large granite / Large granite | FAIL, sample 171 | FAIL, cursor 100 |
| 1 | 1, горизонтальный бот | PASS / PASS | Large granite / Large granite | FAIL, sample 171 | FAIL, cursor 100 |
| 2 | 2, ручной наклон | PASS / PASS | PASS / PASS | PASS, 182 samples | FAIL, cursor 100 |
| 3 | 2, горизонтальный бот | PASS / PASS | PASS / Small granite | PASS, 182 samples | FAIL, cursor 100 |

`Large granite` — `Granite_Cluster_Large_01.et`; `Small granite` —
`GraniteSurfaceStone_01.et`. У горизонтального transform второй площадки
после расширения physics объёма по диапазону высот появляется дополнительный
отказ на маленьком камне. Его нельзя сводить только к отказу пути.
Сохранённый диапазон terrain у места 1 — 7.26375…13.5457 м (ручной наклон)
и 7.25345…13.552 м (горизонтальный); у места 2 — 0.75…1.2838/1.28394 м.
Отказ первой площадки относится к локальному slope guard, а не к сравнению
этого общего диапазона с порогом 0.8 м.

Отдельный тест фактических рабочих точек во всех четырёх cases дал
`start_on_mesh=1 end_on_mesh=1 direct=0 detours=0`: даже без запрета пересекать
будущий AABB ограниченный алгоритм не находит путь. Начальные navmesh
проекции — `<1152,1.19235,3280.09>` и `<1154.26,1.29935,3280.09>`;
проекции endpoints — `<1272,8.41565,3276.36>` и
`<1231.61,1.30203,3276.07>`. Это подтверждает ограниченность проверки,
но не воспроизводит побитово маршрут живого строителя: новый мир, проекция
старта и исходная позиция перед вторым проектом отличаются.

Диагностическое покрытие — **4/4 cases, завершено**, `CONSTRUCTION_REPLAY_DONE`
и native exit **0**, штатное закрытие в `2026-09-19T12:32:29+03:00`.
Проверен полный остановленный `replay-final-console.log`, также сохранены
`replay-final-error.log` и `replay-final-script.log`. В console остаётся
17 строк E/F: 8 WORLD, 4 ENTITY, 3 RESOURCES и 2 SCRIPT при shutdown
(`SCR_BaseResupplySupportStationComponent`). VM exceptions — 0.
Поэтому весь runtime не объявляется чистым PASS; результат этого запуска —
воспроизводимые отказы проверок на конкретных transforms.

## Команды и gates

Полные аргументы находятся в `replay-manifest.json` и
`replay-final-manifest.json`. Запуск только через canonical launcher:

```powershell
$evidence = Join-Path $PWD '.codex-runtime/construction-replay-20260919'
& .\tools\Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -RepositoryRoot (Join-Path $evidence 'runtime-source') -AICommanderMode BOTH `
  -AdditionalArguments @(
    '-aicfConstructionReplay','1',
    '-aicfConstructionReplayProvider','1158.09 1.34825 3279.28',
    '-aicfConstructionReplayPosition0','1281.96 9.58667 3268.84',
    '-aicfConstructionReplayAngles0','0 1.78991 -4.46499',
    '-aicfConstructionReplayPosition1','1222.1 0.939056 3268.57',
    '-aicfConstructionReplayAngles1','0 0 -0.895174',
    '-aicfConstructionReplayWork0','1272.45 8.38841 3276.36',
    '-aicfConstructionReplayWork1','1231.61 0.998901 3276.07')
```

Static baseline: ConstructionStatic, ConstructionContracts (16 log inputs),
BaseBuildersStatic, Stage4Static — **PASS / 0**. AICommanderModeStatic:
**FAIL / 1**, сохранённый `AI_COMMANDER_UI_STATE`.
После изменений те же результаты (`after-summary.json`), команды:
`pwsh -NoProfile -File tools/Test-ConstructionStatic.ps1`,
`Test-ConstructionContracts.ps1`, `Test-BaseBuildersStatic.ps1`,
`Test-Stage4Static.ps1`, `Test-AICommanderModeStatic.ps1`.
Терминальный Workbench с fixture — **PASS / 0** (`wb-replay`,
`wb-final-retry`); между ними `wb-final` сохранил неудачную компиляцию
test-only выражения `%`, исправленную явным integer intermediate.
Workbench запускался по `docs/DEVELOPMENT.md` через
`ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent -gproj <runtime-source/AIConflictArland/addon.gproj> -addonsDir <runtime-source>,<Server/addons> -profile <evidence/wb-final-retry> -wbModule=ScriptEditor -run -validate | Out-Null`.

После replay снова запущены обычные server/client через
`Start-AICFRuntime.ps1 -Role Server|Client -Variant Stock`, из прежнего
`manual-source`, с `aicfConstructionProbe=0`. Profiles:
`Server-20260919-123617-844` / `Client-20260919-123711-765`.
Manifests и launcher logs сохранены как `restored-*-manifest.json` и
`restored-*-launch.txt`. Client launcher подтвердил точный server CLI,
живой PID 2808, port 2001 и `ROSTER_READY`; client PID 20544 подключился.
Это новый мир; пользовательские площадки сохранены в evidence исходной
сессии. Активные logs не являются финальным runtime/client-JIP verdict.

Изменения этого шага:

- `tools/fixtures/AICF_ConstructionReplayProbe.c` — новая диагностическая fixture;
- `tools/Test-ConstructionStatic.ps1` — запрет её включения в production;
- `docs/CONSTRUCTION_MANUAL_REPLAY_20260919.md` — метод и полный результат;
- `docs/TESTING.md`, `docs/CONSTRUCTION_VALIDATION.md`,
  `docs/CONSTRUCTION_MATRIX_20260919.md` — актуальный статус и ссылки на replay.

Новый gameplay фикс пока не применялся; выявленные ограничения сохраняются.
Полная строительная matrix, visual mesh/rock intersection и новый
construction client/JIP — **NOT RUN** в этом replay. Работа не `ACCEPTED`.
