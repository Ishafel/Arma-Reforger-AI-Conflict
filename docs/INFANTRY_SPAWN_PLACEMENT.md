# Проверка места появления пехоты — 2026-09-19

Пользователь сообщил о бойце СССР внутри контейнера в стандартном северном
Everon. В этой сессии сервер зарегистрировал `Failed move` для A1 из
`<4986.53,28.5163,11953.8>`; последующая диагностика показала отсутствие
движения и доступного короткого участка маршрута. Снимки и стек сохранены в
`.codex-runtime/errors-everon-north-20260919-2000/`. Точный prefab контейнера
логами не установлен; нахождение внутри контейнера — наблюдение пользователя.

## Исправление

`AICF_GroupSpawner` прежде прибавлял фиксированный slot offset к позиции
`SCR_SpawnPoint`. Затем stock `SpawnGroupMember` добавлял formation offset и
выбирал ближайший navmesh, без проверки свободного объёма и возможности выйти
с малого изолированного полигона. Ближайшая точка не гарантировала пригодность.

Добавлен opt-in `AICF_InfantrySpawnPlacement` в точном stock boundary каждого
асинхронного member spawn. Проверяются загруженный tile, navmesh у поверхности,
отсутствие воды, свободный объём тела и достижимая точка минимум в 25 м.
При отказе выполняется ограниченный поиск по кольцам 4–32 м с 16 направлениями;
проекция может добавить до примерно 1,4 м. При отсутствии подходящего места
штатная очередь получает `false`, повторный поиск ограничен cooldown 1 с.
Непроверенного fallback нет. Существующий timeout readiness ограничивает
ожидание управляемого roster.

Перед вызовом stock метода временно смещается controller origin так, чтобы
его formation offset совпал с проверенной позицией. После вызова origin
восстанавливается; живые бойцы не перемещаются. Stock создание, callbacks,
loadout member identity и generation/readiness gates сохранены. Механизм
включается только для групп `AICF_GroupSpawner`, включая replacement и donor
пополнения. Отдельные builder/driver factories и чужие группы не изменены.

API проверены по закреплённому Script Diff `1.8.0.13`: `SCR_AIGroup.c`,
`AIPathfindingComponent.c`, `NavmeshWorldComponent.c`, `BaseWorld.c`.

Изменены:

- `AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_GroupSpawner.c`;
- новый `AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_InfantrySpawnPlacement.c`;
- новая fixture `tools/fixtures/AICF_InfantrySpawnPlacementProbe.c`;
- `docs/ARCHITECTURE.md`, `docs/TESTING.md` и этот отчёт.

## Проверки

Evidence: `.codex-runtime/spawn-clearance-20260919/`.

`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-<Name>.ps1`:

| Name | До | После |
|---|---|---|
| Stage35Static | PASS / 0 | PASS / 0 |
| Stage35RecoveryPolicy | PASS / 0 | PASS / 0 |
| InfantryRecruitmentStatic | PASS / 0 | PASS / 0 |
| Stage3Static | PASS / 0 | PASS / 0 |
| AICommanderModeStatic | FAIL / 1 | FAIL / 1, те же две AI_COMMANDER_UI_STATE |
| AILoadoutStatic | NOT RUN | PASS / 0 |

Первоначальная команда с ошибочным именем `Test-LoadoutEditorStatic.ps1`
не запускала аудит; она заменена существующим `Test-AILoadoutStatic.ps1`.

Терминальный `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent
-gproj <root> -addonsDir <dirs> -addons <graph> -logsDir <logs>
-wbModule=ScriptEditor -run -validate`: production Everon, production
EveronRHS и изолированный Everon stage с fixture — **PASS / 0**,
`Script validation successful.`. Полные аргументы и журналы:
`wb-everon*`, `wb-rhs*`, `wb-stage*`.

Runtime запуск отдельной сессией:

```powershell
./tools/Start-AICFRuntime.ps1 -Role Server -Variant EveronNorth `
  -RepositoryRoot '<repo>/.codex-runtime/spawn-clearance-20260919/stage' `
  -AdditionalArguments @('-aicfSpawnPlacementProbe','1','-addr','127.0.0.1:22209','-noThrow')
```

Сервер штатно завершился с **native exit 0**. Fixture вернула
`rejected_container=1 found_exit=1 shift_m=4.46574`, затем
`passed=1 alive=20 moving_attackers=12`. Все 12 атакующих групп сместились
по X/Z на 200,898–302,248 м за 90 секунд после `ROSTER_READY`.
Назначение HQ в этом тесте было обратным пользовательскому матчу; проверка
расположения работает для обеих фракций, а координата ошибки проверялась отдельно.

Полный остановленный server log проверен: **0 Failed move / VM Exception**.
Остались native resource/world/Hierarchy/pathfinding diagnostics и две
`SCR_BaseResupplySupportStationComponent` SCRIPT ошибки при shutdown,
известные из предыдущих baseline. Поэтому runtime не объявляется полностью
чистым. Полные файлы находятся в `server-logs/`, индекс — `server-errors.txt`,
manifest и native exit — `runtime-stock.txt`. Hashes проверенных двух production
файлов совпадают со stage (`tested-source-hashes.json`). `git diff --check` — PASS.

**NOT RUN:** RHS runtime нового placement, отдельный runtime пополнения и
многобойцового formation, новый client/JIP, визуальная проверка, длительный
soak, проверка всех баз и устранение всех возможных причин `Failed move`.
Текущий пользовательский server/client не перезапускались. Уже созданные
бойцы не исправляются задним числом; новый код применяется после перезапуска.

## Последующая пользовательская сессия

После описанных проверок пользователь запросил перезапуск. Исправление было
загружено в `Server-EveronNorth-20260919-201619-789`; клиент подключился,
server подтвердил `ROSTER_READY` и применение `INFANTRY_SPAWN_PLACEMENT`.
Позднее оба процесса остановлены по запросу пользователя. В полном сохранённом
server log этой сессии — 0 `Reason: Failed move`. Это наблюдение одного матча,
а не подтверждение всех маршрутов или отсутствия других diagnostics.
Manifest и полные логи обеих сторон:
`.codex-runtime/restart-everon-north-20260919-spawn-fix/`,
`final-server-logs/`, `final-client-logs/`. Остановка выполнена через
`Stop-Process`; её нельзя считать штатным успешным runtime exit.
