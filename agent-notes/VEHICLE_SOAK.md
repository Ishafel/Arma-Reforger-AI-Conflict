# Получение отряда, техника и длительный прогон

`AICF_VehicleSoakProbe.c` проверяет настоящий первый managed slot (`numeric_slot=0`)
за `US` или `USSR`. Это fixture только из `tests/fixtures`, который runner
добавляет в отдельную копию исходников. В production он не устанавливается.

```powershell
& tools/Invoke-AICFVehicleSoak.ps1 `
  -EvidenceRoot .codex-runtime/soak-us-<unique> -Faction US `
  -DurationSeconds 1800 -ServerPort 2510 -SnapshotWorkingTree

& tools/Watch-AICFVehicleSoak.ps1 `
  -EvidenceRoot .codex-runtime/soak-us-<unique>
```

Для СССР используй `-Faction USSR` и отдельный порт/profile. Runner вызывает
только canonical `Start-AICFRuntime.ps1`; Workbench Validate запускается из
терминала. По умолчанию источник — точный `SourceRef` (`HEAD`), без рабочих
изменений. `-SnapshotWorkingTree` явно включает production diff/untracked.
`source-manifest.json` содержит SHA-256 каждого исходного файла. Общий `StageRoot`
должен иметь исходный manifest; hash проверяются до запуска и после остановки.
Менять используемый stage нельзя. Длительность допускается 1..3600s;
запрошенные пользователем 30 минут означают `1800`, короткий debug не заменяет их.
`-MountedHoldSeconds 120` увеличивает ожидание в exact seats с обычных10s до120s
для отдельной регрессии watchdog; CLI `aicfVehicleSoakMountedHoldMs`, clamp10..180s.
Receipt `MOUNTED_HOLD` подтверждает непрерывное нахождение исходных пяти на местах.

## Проверяемая последовательность

1. Дождаться настоящего READY отряда, сохранить group/entity и spawn generation.
   Назначить desired size5 и удержание spawn point собственного штаба.
2. Запросить обычный платный найм через `RequestPlayerRecruitment`. Отсутствие
   безопасной казармы означает ожидание её штатного строительства и quiet period.
   Здания, припасы и солдаты не создаются обходным способом.
3. Зафиксировать исходные пять actor identities. Выбрать доступный своему faction
   пятиместный UAZ469 или M1025/M2HB и назначить всех на точные native seats,
   включая обязательного стрелка Humvee. Выдача использует обычную экономику.
4. Подтвердить пять settled occupants в той же машине, затем назначить удалённую
   road/navmesh точку. Координаты и имена баз в Core не зашиваются.
5. Требовать настоящий доезд (displacement>=100m, >=5 velocity polls,
   distance<=dismount radius+10m), normal COMPLETE высадки и прибытие всех пяти
   исходных бойцов пешком в35m от той же цели/revision.
6. Наблюдать до полного срока, в том числе после обнаруженного FAIL. Замена
   группы, fallback и потери не превращаются в успешное прохождение маршрута.
   Ранний конец матча — ABORTED, а не сокращённый DONE.

Обе фракции ведут обычный бой (`AICommanderMode BOTH`), damage и отношения
не изменяются; `truce=0 invulnerability=0 teleport=0` фиксируются в START.
Native VehicleCombatActivity может законно высадить пассажиров до цели. Такой
прогон подтверждает наблюдение боя, но не успешную транспортную цепочку. Для
изолированной регрессии доезда есть отдельные cases в
[VEHICLE_SCENARIOS.md](VEHICLE_SCENARIOS.md); их мирные условия указываются отдельно.

Fixture вызывает authoritative обработчик команды после слоя player RPC.
Он проверяет обычный найм, roster, экономику и transport lifecycle; владение
реального подключённого игрока, RPC и визуальное состояние интерфейса этим
прогоном не проверяются.

## Evidence и анализ

Watcher читает полный растущий console log, сохраняет `analysis-live.json` и
`analysis-events.jsonl`. Он не меняет сервер, отряд или исходники stage. Индекс
ошибок служит для быстрого разбора; окончательный verdict требует всех полных
остановленных server logs и native exit.
При перезапуске watcher загружает прежние unique alerts. Краткое чтение active log
не сбрасывает offset, а временная блокировка `analysis-live.json` не обрывает
чтение console. Старые индексы ранних запусков могут содержать дубликаты;
количество alerts не означает количество уникальных ошибок игры.
Соседний `crash.log` проверяется на каждом poll: native assertion может остановить
console и оставить process живым. Такой случай немедленно получает `NATIVE_CRASH`
с причиной, без автоматического воздействия на процесс. Краткие IO locks индекса
alerts также повторяются; после неудачи полный console читается повторно.
Незаписанный alert публикуется как `pending_alert_write` и запрещает успешный
выход watcher даже при `Game destroyed.`; истечение окна остаётся exit2.

`Test-VehicleSoakLog.ps1` разделяет длительность/heartbeat/нормальный останов,
функциональную цепочку, SCRIPT/VM failures и native resource errors. Отсутствие
падения не означает успешный доезд. `timing.json` хранит независимые UTC wall
timestamps; `native-exit.txt`, `launcher.txt` и исходные manifests сохраняются.
`audit-summary.json` и полный `audit.txt` содержат причины FAIL.

Свежие результаты и известные ограничения — в [TESTING.md](TESTING.md).

Очередь preset boarding выдаёт только один вход за раз и ждёт любые actual
transitions, включая смену места. Stalled owned action после20s без прогресса
может owner-safe уступить ещё не обслуженным бойцам. Retry выбирается только
среди готовых actor/seat пар, сначала с меньшим retry count; foreign reservation
и locked seat не удерживают очередь других готовых мест. Максимум2 retry и общий
срок25s×max(3, assigned) не продлеваются. `VEHICLE_PRESET_SEAT_YIELDED` отличает
уступку от `VEHICLE_PRESET_SEAT_RETRY`. Сам по себе unit contract не подтверждает
физическую возможность входа через конкретную дверь: нужен native receipt.
