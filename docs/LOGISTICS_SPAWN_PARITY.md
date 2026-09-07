# Допуск логистической техники и штатные исключения композиции

## Изменение 2026-09-07

Предыдущий recovery зафиксирован отдельно в `10f0855`. Новый change исправляет
расхождение физического admission: OBB preview meshes включал пустоты кузова и
проверял его против всех props штатного depot. Stock `IsOccupied()` исключает
editable parent выбранного slot и его children через `FillExcludedEntities()`.
Поэтому лампа или колесо собственной композиции могли запрещать AI spawn.

`AICF_LogisticsExclusions` заново собирает ту же editable-связь при каждой
проверке. Исключения не основаны на prefab name, размере или расстоянии.
Vehicle, character и их physical children никогда не добавляются в список.
Одного `ExcludeArray` в runtime оказалось недостаточно: проход `server-3`
снова получил tire/garbage blockers. Read-only callback теперь проверяет также
physical ancestry до исключённого editable prop; внешний объект сохраняется.
При встрече Vehicle/character в цепочке callback сохраняет blocker, при слишком
глубокой цепочке также выбирает отказ. Ссылка на exclusions очищается после trace.
`IsOccupied` и его callback не вызываются: stock callback удаляет wrecks и
разыменовывает Physics без null guard.

Для внешних объектов сохранены Vehicle mask и непаддированный prefab OBB.
Для внешних объектов это более консервативный admission, чем stock фильтрация
characters, unloaded simulation и wrecks. Разные маски и bounds не позволяют
утверждать общую строгость одного admission относительно другого. Не заявляется полная
эквивалентность всех отказов штатному заказу. Изменение устраняет конкретное
отсутствие composition exclusions; не вводит точный mesh-overlap API.

Обе проверки, при reserve и перед spawn, используют свежие exclusions.
Перед проверкой сравниваются identity slot и все четыре вектора transform;
поворот slot после reserve также отменяет попытку. Surface, exact provider,
faction, fleet, общие reservations, стоимость, empty cargo и recovery сохранены.
Собственные composition props могут пересекать приблизительный OBB, но чужая
машина остаётся физическим blocker. Кандидаты ничего не удаляют.

Диагностика `VEHICLE_FOOTPRINT_BLOCKED` сохраняет прежние поля и добавляет
`spawn_slot`, `prefab`, `excluded`, `rule=SLOT_COMPOSITION_EXCLUSIONS`.

## Проверки

Evidence: `.codex-runtime/spawn-parity-20260907/` (не входит в Git).
Baseline: девять аудиторов PASS/0; LogisticsContracts 102 cases.
Candidate Workbench: Arland, Everon, RHS PASS/0, Game successfully created
и Script validation successful, SCRIPT/VM errors отсутствуют.
Финальная production-версия с ancestry callback: те же три graphs PASS/0
(`ancestry-workbench-results.json`). Все девять static audits PASS/0,
LogisticsContracts — 108 positive/negative cases; baseline failures отсутствовали.

Fixture `tools/fixtures/AICF_LogisticsSpawnParityProbe.c` выполняется только
в изолированной source-копии вместе с LogisticsRuntimeProbe. Она сравнивает
legacy OBB и новый production admission, затем проверяет внешнее препятствие,
shared reservation, occupancy перед commit, stale depot identity, настоящий
spawn и повторный spawn. Stock comparison использует копию штатных predicates
без удаления wreck и с null guard; это read-only сравнение допуска, а не заказ
игрока через UI. Случаи с wreck/null Physics не доказывают полную stock parity.

Client/JIP/visual: NOT RUN, клиент не запускается.

Промежуточные runtime результаты:

- `server-1`: четыре физические доставки M998; heavy depot не подготовился.
  Полный log audit FAIL/1: native `SCR_BaseResupplySupportStationComponent needs
  a entity catalog manager!` при завершении. Эти ошибки не объявлены baseline.
- `server-2`: запуск не состоялся — replication port занят первым процессом.
  `ServerPort` launcher не меняет bind direct-world server. Дальнейшие прогоны
  последовательные, каждый в отдельном profile.
- `server-3`: первая версия с одним `ExcludeArray` воспроизвела отказ Ural;
  собственный процесс остановлен после фиксации отказа, это FAIL воспроизведения,
  а не завершённый положительный smoke.
- `server-4`: M998 и Ural прошли по 13 проверок admission (26/26).
  Ural transport capacity=1500 создан, водитель готов. Legacy trace отвергает
  собственную lamp → chair → depot, новый trace пропускает. Однако stock read-only
  для этого же slot возвращает отказ: полная parity здесь НЕ доказана.
  Оба Ural исчерпали recovery при выезде и завершены без доставки; появление
  машины не является положительным результатом end-to-end проверки Ural.

- `server-5`: оба USSR heavy depot исчерпали 512 placement candidates; тест
  Урала не начался. Собственный процесс остановлен, сохранены command line,
  полные доступные logs и hashes. Это ограничение setup, не PASS спавна.

`server-6` использует явный `-aicfParityDirectDepotSetup 1`: только fixture
обходит строительные terrain grid, прямые exits и путь строителя после
`BeginCandidate` (bounds/live collision). Штатный heavy prefab создаётся целиком,
production surface/vehicle admission/driver/route не меняются. Это не проверка
строительства и не доказательство корректности terrain placement.
Проверка реальной смены фракции production entity и возврата прежней фракции
прошла; уничтожение depot отдельно NOT RUN. Урал и M998 прошли 28/28 admission
cases. Первый Урал получил native `STUCK` и исчерпал recovery.

## Команды и evidence

Все пути ниже относительно корня репозитория. Девять baseline/final команд:
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-<Name>.ps1`,
где Name: `Stage3Static`, `Stage35Static`, `Stage3StaticContracts`,
`Stage35RecoveryPolicy`, `Stage4Static`, `LogisticsStatic`, `LogisticsContracts`,
`ConstructionStatic`, `ConstructionContracts`. Verdict каждого PASS/0;
выводы `baseline-*.txt`, `ancestry-*.txt`, сводки `*-results.json` в evidence.

Workbench: `& .codex-runtime/spawn-parity-20260907/Run-Workbench.ps1 -Phase ancestry`
— PASS/0 для Arland/Everon/RHS. Точные native arguments, времена и пути полных
logs сохранены в `ancestry-workbench-results.json`. Дополнительная компиляция
финальной fixture Everon — PASS/0 (`direct-fixture-workbench/console.log`,
`direct-fixture-workbench-exit.txt`).

Финальный серверный запуск (`server-8`):

```powershell
& tools/Start-AICFRuntime.ps1 -Role Server -Variant Everon `
  -RepositoryRoot "$PWD/.codex-runtime/spawn-parity-20260907/source-8" `
  -ProfileRoot "$PWD/.codex-runtime/spawn-parity-20260907/server-8" `
  -AICommanderMode USSR -AdditionalArguments @(
    '-aicfLogisticsProbe','1','-aicfLogisticsProbePrepare','1',
    '-aicfLogisticsProbeDurationMs','1200000','-aicfLogisticsProbePeace','1',
    '-aicfLogisticsProbeMaintainDemand','1','-aicfLogisticsProbeRepeatSource','1',
    '-aicfLogisticsProbeDepotAtSource','1','-aicfParityDirectDepotSetup','1',
    '-aicfLogisticsProbeMinSourceCapacity','2500',
    '-aicfLogisticsProbeStopAfterUralDelivery','1')
```

Изолированная копия содержит две fixture: `AICF_LogisticsRuntimeProbe.c` и
`AICF_LogisticsSpawnParityProbe.c`; в основной production graph их нет.
Отличия подготовки RuntimeProbe сохранены в `runtime-fixture.patch`: восемь
шагов поиска за update, первоначальный USSR heavy depot, HQ fallback,
диагностика поиска, и другие search ordinals для последнего прогона.
Все 103 production `.c` совпадают с текущим деревом в snapshots 4/5/6
(`final-source-integrity.json`).

Full stopped log проверяется командой:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsLog.ps1 `
  -LogPath <полный-console.log> -RequireDelivery -RequireLedger `
  -RequirePolicy -RequireGraph -RequireSearch
```

`server-4` завершился native exit=0, но log audit FAIL/1: кроме ошибок native
resupply station, M998 cleanup выбрал `RETAINED_FAIL_CLOSED` с
`DELETE_AUTHORITY_OR_IDENTITY_REJECTED`. Fleet cap освобождён; это не считается
доказательством полного успешного cleanup. Сводка `server-4-result.json`, полные
errors `server-4-errors.json`, hashes `server-4-hashes.json`, console:
`server-4/logs/logs_2026-09-07_20-41-24/console.log` внутри evidence.

`server-6` завершён native exit=0, полный audit FAIL/1. M998 выполнил три
доставки; Ural второго поколения физически покинул запасной slot, добрался
до source и загрузил 437 supplies при discrepancy=0. Лимит 600000ms завершил
тест до доставки: released=437 не является delivered. Также зарегистрированы
`INSTIGATOR_OTHER`, fail-closed cleanup и ошибки native resupply station.
Полный console: `server-6/logs/logs_2026-09-07_20-59-48/console.log`.

В `server-7` проверки расширены до каждого prefab + exact slot (56/56 для
двух Ural, M998 и UAZ). У завершённых workers Ural зарегистрирован
`NO_SOURCE_SURPLUS_OR_MINIMUM_BATCH`; fixture source USSR имел capacity=1500.
Это общий код отказа: он сам по себе не различает состояние донора, резервы
и размер партии и не доказывает ошибку production экономики. Прогон остановлен,
положительный Ural end-to-end результат не заявляется.

Для `server-8` добавлены только параметры RuntimeProbe:
`-aicfLogisticsProbeMinSourceCapacity 2500` выбирает существующий достаточно
вместительный source, не меняя production minimum batch/reserve/cargo capacity;
`-aicfLogisticsProbeStopAfterUralDelivery 1` завершает fixture после доставки
Ural через обычный Stop и последующие 60 секунд cleanup. Верхняя длительность
1200000ms. При отсутствии параметров прежнее поведение fixture сохранено.

## Подготовка воспроизводимого отдельного graph

Следующий setup не изменяет основной addon graph. Каталог назначения должен
быть новым; перед повторным запуском выбери другое имя.

```powershell
$probeRoot = Join-Path $PWD '.codex-runtime/parity-repro/source'
if (Test-Path -LiteralPath $probeRoot) { throw 'Choose a fresh snapshot directory' }
New-Item -ItemType Directory -Path $probeRoot | Out-Null
foreach ($addon in @('AIConflictCore','AIConflictArland','AIConflictEveron')) {
  Copy-Item -LiteralPath (Join-Path $PWD $addon) -Destination $probeRoot -Recurse
}
$runtimePath = Join-Path $probeRoot 'AIConflictCore/Scripts/Game/AIConflict/Economy/AICF_LogisticsRuntimeProbe.c'
Copy-Item tools/fixtures/AICF_LogisticsRuntimeProbe.c $runtimePath
Copy-Item tools/fixtures/AICF_LogisticsSpawnParityProbe.c `
  (Join-Path $probeRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_LogisticsSpawnParityProbe.c')
$runtime = [IO.File]::ReadAllText($runtimePath)
$runtime = $runtime.Replace('AICF_ProbePlace(ussr, AICF_EConstructionType.LIGHT_DEPOT, 0);',
  'AICF_ProbePlace(ussr, AICF_EConstructionType.HEAVY_DEPOT, 8, true);')
$runtime = $runtime.Replace('AICF_ProbePlace(ussr, AICF_EConstructionType.HEAVY_DEPOT, 1);',
  'AICF_ProbePlace(ussr, AICF_EConstructionType.HEAVY_DEPOT, 12, true);')
$runtime = $runtime.Replace("`t`tAICF_ProbeAdvancePreparation();",
  "`t`tfor (int preparationStep; preparationStep < 8; preparationStep++) AICF_ProbeAdvancePreparation();")
[IO.File]::WriteAllText($runtimePath, $runtime)
```

После terminal Workbench validation запускай приведённую выше команду launcher
с новым RepositoryRoot/ProfileRoot. Клиент не требуется.

## Подтверждённая доставка Ural

`server-8`, run=`stage1-server-19616`: **84/84 admission cases PASS** на шести
комбинациях prefab + slot, включая запасной slot Урала. Три production graphs
по-прежнему совпадают с проверенной версией; повтор девяти аудиторов
`delivery-final-*.txt` — PASS/0, LogisticsContracts 108/108. Финальная fixture
скомпилирована в `source-capacity-fixture-workbench`, exit=0.

| Событие | Evidence |
|---|---|
| Heavy depot | prefab `{6676FBB212B337D6}`, depot `0x40000000000041C5`, USSR worker 1000001 |
| Альтернативный slot | `0x400000000000429E`, position `<8388.76,233.599,4753.25>` |
| Причина legacy отказа | Tire_M151A2, physical child `E_Tire_Stack_Garbage_01.et` внутри этого depot; сам child отсутствует в ExcludeArray, его parent присутствует |
| Stock comparison | stock read-only=0, AI=1; mask/bounds/physical-child semantics различаются, полной эквивалентности нет |
| Реальная машина | Ural transport `{16C1F16C9B053801}`, generation=2, entity `0x400000000000499D`, Rpl `-2147457046`, capacity=1500 |
| Выезд | Машина покинула depot, проехала к source `<7444.67,163.724,4288.7>` и обратно к destination `<8364.75,238.14,4761.05>` |
| Погрузка | `t_ms=755896`, job `L5_S1000001_G2`, amount=300, cargo=300, discrepancy=0 |
| Доставка | `t_ms=993019`, тот же job/entity/generation, `purpose=DELIVERY`, amount=300, cargo=0, discrepancy=0 |
| Ledger после остановки worker | `t_ms=1003043`, loaded=300, delivered=300, in_transit/lost/released=0, balance_delta=0, retained_lease=0, custody=0, fault=0 |

Это подтверждает конкретный положительный случай спавна и физической доставки.
Первый slot по-прежнему может оказаться плохим для выезда; в этом случае
сработали существующие recovery, retirement и выбор другого slot. Не заявляется,
что все площадки проезжаются с первой попытки. Телепортации/удаления окружения
для выполнения рейса нет. Первые неудачные попытки сохранены в полном логе.

Полный console: `.codex-runtime/spawn-parity-20260907/server-8/logs/logs_2026-09-07_21-26-17/console.log`.
Процесс завершён штатно, native exit=0. **Общий log audit — FAIL/1**, а не PASS:
зарегистрированы native `SCR_InstigatorContextData: INSTIGATOR_OTHER` и
`VEHICLE_CLEANUP_RETAINED` для другого UAZ (USSR slot 1000004, generation=3),
`DELETE_AUTHORITY_OR_IDENTITY_REJECTED`, с соответствующим `CORE_ERROR_BRIDGE`.
Эти ошибки не объявлены baseline failures. Все error records сохранены в
`server-8-errors.json`, полные log hashes — `server-8-hashes.json`, результат
аудитора — `server-8-audit.txt` и `server-8-result.json`.

У доставившего Ural подтверждён `VEHICLE_CLEANUP_CONFIRMED` при
`t_ms=1009056`, `reason=AUTHORITY_DELETE_CONFIRMED`. Его lease/custody освобождены,
ledger свёлся. Это не скрывает отдельный fail-closed cleanup УАЗа.

Точный launcher manifest — `server-8-manifest.json`. Все 103 production Core
`.c` совпадают с исходниками завершённого server-8
(`server-8-production-integrity.json`). Финальный patch подготовки RuntimeProbe
— `runtime-fixture-final.patch`; остальные условия совпадают с рецептом выше.

NOT RUN: client/JIP, ручной заказ игроком на идентичном prefab/slot/transform,
визуальная проверка, отдельное уничтожение depot между reserve и spawn,
runtime Arland/RHS. Смена реального владельца production entity вместо
уничтожения проверена. Static/compile PASS и успешная доставка не заменяют
общий runtime gate и не означают автоматическую приёмку.

## Изменённые файлы

- `AIConflictCore/Scripts/Game/AIConflict/Economy/AICF_LogisticsDepotRegistry.c` — свежие исключения конкретного stock slot, защита Vehicle/character.
- `AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_LogisticsVehicleFootprint.c` — read-only ancestry callback и сохранение внешних blockers.
- `AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleSpawner.c` — повторная проверка identity/полного transform и диагностика admission.
- `tools/Test-LogisticsStatic.ps1`, `tools/Test-LogisticsContracts.ps1` — новый контракт и negative mutations.
- `tools/fixtures/AICF_LogisticsSpawnParityProbe.c` — stock comparison и реальные negative/positive cases.
- `tools/fixtures/AICF_LogisticsRuntimeProbe.c` — опциональный выбор вместительного source и автоматический Stop после доставки Ural.
- `docs/ARCHITECTURE.md`, `docs/LOGISTICS_SPAWN_CLEARANCE.md`, `docs/LOGISTICS_SPAWN_PARITY.md` — актуальная семантика, evidence и ограничения.
