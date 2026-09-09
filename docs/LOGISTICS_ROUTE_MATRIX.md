# Everon: тест нескольких направлений, 2026-09-08

Запущена отдельная серверная сессия с клиентом. Test-only fixture моделирует
захват баз, начальные/возобновляемые запасы и готовые depot compositions.
Вождение, создание машин и водителей, admission, грузовые операции и recovery
исполняются обычным production кодом. Все 105 production `.c` совпали по SHA-256
с текущей рабочей копией; изменения маркеров предыдущей задачи включены.

## Подготовленные точки

| Сторона | Назначение | База | Начальные припасы / вместимость |
| --- | --- | --- | --- |
| US | Источник 1, light depot | Coastal Base Morton | 1885 / 1885 |
| US | Источник 2, heavy depot | Simon's Wood | 1000 / 1000 |
| US | Получатель 1 | Morton Valley | 200 / 1000 |
| US | Получатель 2 | Figari | 200 / 1000 |
| USSR | Источник 1, light depot | Airbase Saint-Philippe | 6500 / 6500 |
| USSR | Источник 2, heavy depot | Saint-Philippe | 3550 / 3550 |
| USSR | Получатель 1 | Military Depot | 200 / 1000 |
| USSR | Получатель 2 | Meaux | 200 / 1000 |

Базы выбраны автоматически по расстоянию до HQ, наличию provider/resource pool
и дорожной доступности в обе стороны. World coordinates не зашиты в fixture.
В 21:03:51 для всех восьми точек подтверждены `owner_matches=1` и HQ depth
`1/2/2/2` у US, `1/1/2/3` у USSR. HQ и известные connectors поддерживаются
на 80%, источники пополняются раз в минуту. Получатели сбрасываются до 20%
только после заполнения обоих до 79%: это сценарий потребления, не доказательство
двух доставок, поскольку штатная генерация supplies продолжает работать.
Штатные AI commanders и строительство тоже продолжают работать; они могут
создавать дополнительные потребности и захватывать другие базы.

`Peace` делает дружественными US, USSR и FIA. Два workers на depot настроены
через существующий CLI; caps не отключены. Геометрия размещения самих depot
проходит штатный search: четыре подготовленных источника не означают четыре
немедленно построенных depot. В первом наблюдении light depot обеих сторон
построены, heavy depot ещё проходят поиск площадки.

## Первое наблюдение, до 21:10

- Обе стороны прошли `ROSTER_READY`: по 10 армейских групп.
- US: первые две попытки создания водителя завершились `SPAWN_IDENTITY_LOST`,
  рядом в логе зафиксирован `SCR_InstigatorContextData: INSTIGATOR_OTHER` во время
  physics simulation. Конкретная причина потери identity ещё не доказана.
  Следующие попытки создали работоспособные машины.
- US: `L3_S1000000_G2` загрузил 315 supplies в Coastal Base Morton в 21:06:43.
  В 21:10:01 та же машина разгрузила **288** в Figari: запас получателя
  `512 → 800`, груз `315 → 27`, `discrepancy=0`. Остаток сохранён физически.
- USSR: два УАЗ-452 с отдельными водителями получили разные задания:
  Airbase Saint-Philippe → Military Depot (516) и → Meaux (512).
  Физическое движение к погрузке подтверждено последовательными positions
  и `LOGISTICS_ROUTE_SAMPLE`.
- Оба советских рейса завершились до погрузки у одного native gate target
  `0x0000000000005084`: `DRIVER_INTERACTION_NATIVE_ACTION_FAILED` и
  `DRIVER_INTERACTION_NATIVE_CHAIN_LOST`. Время начала interaction — 21:07:54
  и 21:08:08, завершения — 21:08:13 и 21:08:21. Это воспроизведение сбоя,
  не успешная доставка. Служба остаётся активной и выполняет replacement.

## Запуск и evidence

Активный evidence root:
`.codex-runtime/everon-route-matrix-20260908-205348/`.
`source/` содержит замороженную копию трёх addons и одну
`AICF_LogisticsRuntimeProbe.c` в Core/Economy. Оба процесса используют эту копию.

```powershell
$e = Get-Content '.codex-runtime/active-route-matrix.txt'
& tools/Start-AICFRuntime.ps1 -Role Server -Variant Everon `
  -RepositoryRoot "$e/source" -AICommanderMode BOTH `
  -ProfileRoot "$env:LOCALAPPDATA/AICF/Server-Everon-Matrix-20260908-210231" `
  -AdditionalArguments @(
    '-aicfLogisticsProbe','1', '-aicfLogisticsProbePrepare','1',
    '-aicfLogisticsProbeRouteMatrix','1', '-aicfLogisticsProbePeace','1',
    '-aicfLogisticsProbeDepotAtSource','1', '-aicfLogisticsProbeRepeatSource','1',
    '-aicfLogisticsProbeMaintainDemand','1',
    '-aicfLogisticsProbeDurationMs','3600000', '-aicfLogisticsWorkersPerDepot','2')

& tools/Start-AICFRuntime.ps1 -Role Client -Variant Everon `
  -RepositoryRoot "$e/source" -AICommanderMode BOTH `
  -ServerProfileRoot "$env:LOCALAPPDATA/AICF/Server-Everon-Matrix-20260908-210231" `
  -ProfileRoot "$env:LOCALAPPDATA/AICF/Client-Everon-Matrix-20260908-210335"
```

Эти profiles уже использованы; повторный запуск требует новых имён.
Client launcher подтвердил exact server CLI, process `13760`, port `2001` и
`ROSTER_READY`; client process — `8480` на момент запуска. Manifest JSON
сохранены в `server-manifest.json` и `client-manifest.json`, stdout —
`server-launch-2.txt`, `client-launch-2.txt`. Полные текущие logs находятся в
server/client profiles; `live-snapshot-*` — полные промежуточные копии,
которые не считаются остановленными логами. `latest-live-snapshot.txt` указывает
последнюю такую копию.

Первая попытка подготовки требовала четыре базы уже внутри исходной radio
component. Она нашла 3/1 и не подготовила матрицу. Её server/client остановлены;
полные логи и launch metadata сохранены в `attempt-1/`. Исправленная fixture
захватывает ближайшие дорожные базы, затем наблюдает реальный обновлённый graph.
Результаты первой неудачной попытки не объявляются PASS.

## Проверки и границы результата

Изменены `tools/fixtures/AICF_LogisticsRuntimeProbe.c`, `docs/TESTING.md` и этот
документ. Production `.c` в этой задаче не менялись.

До изменения: `Test-LogisticsStatic.ps1`, `Test-LogisticsContracts.ps1` (110 cases),
`Test-LogisticsMapMarkersStatic.ps1` — PASS, exit 0; baseline failures нет.
После изменения повторены `Test-LogisticsStatic.ps1` и
`Test-LogisticsMapMarkersStatic.ps1` — PASS, exit 0. Команда каждого audit:
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/<name>`.

Терминальный `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent
-gproj <source/AIConflictEveron/addon.gproj> -addonsDir <stock,source>
-addons 9178E5822AFE48EA,B52C5F6AEDBF423E,A4B2E62595F645A4
-logsDir <evidence/workbench-2> -wbModule=ScriptEditor -run -validate`:
PASS, exit 0, `Script validation successful`, SCRIPT E/F — 0.
Полные Workbench logs и stdout/exit сохранены. Arland/RHS runtime в этой задаче
не запускались; для их production код не менялся.

Сервер и клиент оставлены для ручного наблюдения. Fixture ограничена одним
часом от старта службы, затем вызывает Stop и оставляет минуту на cleanup.
Финальный аудит полного остановленного server/client log — **NOT RUN**, пока
сессия продолжается. Вид карты/hover, все сочетания source/destination, heavy
vehicles, полный круг обеих сторон и JIP остаются непроверенными. Сессия с
подтверждёнными failures не объявляется ACCEPTED.
