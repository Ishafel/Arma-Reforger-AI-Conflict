# Получение отряда, техника и длительный прогон

`AICF_VehicleSoakProbe.c` проверяет настоящий первый managed slot (`numeric_slot=0`)
за `US` или `USSR`. Это fixture только из `tests/fixtures`, который runner
добавляет в отдельную копию исходников. В production он не устанавливается.

```powershell
& tools/Invoke-AICFVehicleSoak.ps1 `
  -EvidenceRoot .codex-runtime/soak-us-<unique> -Faction US `
  -DurationSeconds 1800 -ServerPort 2510 -SnapshotWorkingTree -ProofVersion 3

& tools/Watch-AICFVehicleSoak.ps1 `
  -EvidenceRoot .codex-runtime/soak-us-<unique>
```

Для СССР используй `-Faction USSR` и отдельный порт/profile. Runner вызывает
только canonical `Start-AICFRuntime.ps1`; Workbench Validate запускается из
терминала. По умолчанию источник — точный `SourceRef` (`HEAD`), без рабочих
изменений. `-SnapshotWorkingTree` явно включает production diff/untracked.
`source-manifest.json` содержит SHA-256 каждого исходного файла. Общий `StageRoot`
должен иметь исходный manifest; hash проверяются до запуска и после остановки.
Менять используемый stage нельзя. `-ProofVersion` по умолчанию2 сохраняет старые
runner calls; для текущего боевого probe явно передавай3. Анализатор копируется
в evidence до запуска, его SHA-256 фиксируется в source manifest и повторно
проверяется перед разбором полных остановленных логов. Длительность допускается 1..3600s;
запрошенные пользователем 30 минут означают `1800`, короткий debug не заменяет их.
`-MountedHoldSeconds 120` увеличивает ожидание в exact seats с обычных10s до120s
для отдельной регрессии watchdog; CLI `aicfVehicleSoakMountedHoldMs`, clamp10..180s.
Receipt `MOUNTED_HOLD` подтверждает непрерывное нахождение исходных пяти на местах.

## Ordinary war observation4

`-ProofVersion 4` добавляет только stage fixture
`AICF_VehicleSoakRecoveryObservationProbe.c` и замораживает отдельный аудитор
`Test-VehicleSoakObservationLog.ps1`. Старые Proof3 исходники, auditor receipts
и FAIL не пересчитываются. Обе новые стороны требуют свежего полного1800s run.
Команда аналогична примеру выше с `-ProofVersion 4`; optional
`-LooseWeaponObserver` добавляет read-only ledger dropped оружия, без controlled
baseline drop. Он нужен для диагностики native SVD out-of-world assertion.

У observation4 два явно различных результата: `ORIGINAL_FULL_ROUTE` требует
прежний физический маршрут исходных бойцов; `EXPLAINED_NATIVE_DAMAGE_RECOVERY`
подтверждает законную боевую потерю и восстановление, **не** полный original route.
Вторая ветка допускается только по native on-fire/неподвижности exact lease car
и Rpl/generation, независимо подтверждённым FALLBACK и native health snapshot.
Reason без actual damage не принимается. Все пять originals сохраняют actor/Rpl
identity, native life и полное отсутствие vehicle/seat/transition. Исчезновение
разрешено лишь после наблюдённого native DEAD; неизвестное состояние даёт FAIL.

Cleanup должен подтвердить exact car absence, cap release и завершение не позже
существующего protected-clearance terminal deadline. Далее нужны conscious
original survivors либо ровно следующая generation с5новыми distinct actors,
native SPAWN_BOUND/readiness и настоящим ticket/supply commit. Текущий foot
waypoint обязан оставаться current, in queue и у прежней target/revision.
Прогресс≥10m измеряется от одного и того же живого лидера после ready snapshot;
promotion начинает новый survivor отсчёт. Независимый GROUP_TRACE подтверждает
фактические leader position, current/owned waypoint и target. Подмена generation,
car, actors, фиктивное damage, просроченный/отсутствующий cleanup, spawn timeout
и unaccounted repair остаются FAIL. Fixture не лечит, не воскрешает и не меняет
позиции или native budget.

Duration, liveness, stopped exit, source/CLI provenance и все sibling fatal/resource
logs остаются отдельными строгими gates. В частности console без SCRIPT(E)
не отменяет `Assertion failed` в соседнем crash.log. Отдельные controlled route
cases обеих сторон нужны для full-route доказательства независимо от ordinary
war результата. Профильный контракт `Test-VehicleSoakObservationContracts.ps1`
проверяет source mutants, поддельные receipts и полный auditor с повреждением
duration/liveness/provenance/stopped/fatal/resource gates.

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
   distance<=dismount radius+10m), retained manual высадку и прибытие исходных
   боеспособных бойцов пешком в35m от той же цели/revision. Для нового явного
   `casualty_policy=CAPTURED_ORIGINAL_SURVIVORS` природная боевая потеря не требует
   воскрешения: все пять захваченных actor identities сохраняются, каждый
   существующий nonfit original тоже обязан физически покинуть машину/bounds.
   Текущий рецепт явно
   использует TRANSPORT/300; вооружённый Humvee в этом прогоне тоже транспорт.
6. Наблюдать до полного срока, в том числе после обнаруженного FAIL. Замена
   группы и fallback запрещают успешный маршрут; сами потери не доказывают
   ни успех, ни дефект поведения. Нужны все physical survivor receipts.
   Ранний конец матча — ABORTED, а не сокращённый DONE.

Обе фракции ведут обычный бой (`AICommanderMode BOTH`), damage и отношения
не изменяются; `truce=0 invulnerability=0 teleport=0` фиксируются в START.
С proof_version2 поле `teleport=0` явно имеет `teleport_scope=FIXTURE_ONLY`:
fixture не переносит бойцов, но новый production short recovery может выполнить
разрешённую пользователем близкую коррекцию. Это отдельно объявлено как
`production_short_recovery=1`; прогон не доказывает отсутствие таких коррекций.
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

Функциональное окно ограничено достоверными START/DONE. Штатный retained
`COORDINATOR_STOP` после DONE исключается только при совпадении исходной
operation/faction/slot/group/trip/lease/vehicle/entity/Rpl и native stop receipt;
неопределённая sibling ошибка, чужая identity и fallback до DONE остаются FAIL.
Fatal/resource gates всегда читают полные stopped logs, включая teardown.
У calm hold оба реальных интервала (`t_ms` и `elapsed_ms`) обязаны достичь
configured minimum; различие каждого с `held_ms` допускается лишь до50мс
logger latency. Этот допуск не уменьшает игровой hold и не продлевает timeout.
Log contracts содержат153controls, включая реальные6мс, короткий любой из
интервалов, большое расхождение, pre-DONE fallback и post-DONE fatal/resource.
Corrected re-audit сохраняется отдельно: исходный frozen auditor и его FAIL
не заменяются исправленным результатом.

## Retained manual lifecycle: proof_version2

После прибытия ручной пресет сохраняет оплаченную машину в
`TACTICAL_DEPLOYMENT`. Поэтому новый probe не ждёт `COMPLETE` и не печатает
прежний `DISMOUNTED normal_complete=1`. `TRIP_RETAINED_FOOT` требует текущий
active trip, тот же lease/entity, актуальный preset, `IsOrderRestored`, отсутствие
reboarding и две последовательные физические проверки всех исходных пяти:
никто не находится в машине/compartment transition или расширенных на1.5м bounds.
Пять `MEMBER_FOOT` receipts сохраняют исходные position/EntityID. Никаких новых
изменений production, transfer, покупок или двигательных действий probe не делает.

Анализатор по умолчанию требует `-ProofVersion 2` и явный START `proof_version=2`.
Независимые production receipts должны подтвердить `TRANSIT→TACTICAL_DEPLOYMENT`
с причиной `DESTINATION_REACHED` и успешный `ORDER_RESTORE_RESULT` для исходной
операции. Затем требуются все прежние paid-recruit/occupant/drive/unchanged-target/
foot-arrival доказательства. Ранняя высадка из-за угрозы, потери, настоящий fallback,
неготовый roster или отсутствие движения не получают PASS от новой формы marker.

Исторические `COMPLETE` логи разбираются только с явным `-ProofVersion 1`:
сохранены прежние native DISEMBARK_COMPLETE/HANDOFF→COMPLETE и отрицательные
controls. Новый proof не переоценивает старые FAIL задним числом. Duration,
liveness, полный останов, fatal и resource gates остаются независимыми;
строгий resource FAIL сохраняет общий FAIL даже при выполненной игровой цепочке.

## Обычный бой: proof_version3

Новые правила пользователя разрешают Transport высаживаться при угрозе. Поэтому
только для нового `-ProofVersion 3` допускаются два раздельных результата:

- `DIRECT_VEHICLE_ARRIVAL` сохраняет исходное требование физического прибытия
  машины в dismount radius+10м, ≥100м движения и≥5 velocity polls.
- `COMBAT_INTERRUPTED_COMBINED_ROUTE` требует фактический production переход
  `TRANSIT→TACTICAL_DEPLOYMENT reason=LOCAL_THREAT` после назначения исходной
  цели, затем причинный `COMBAT_INTERRUPTION` той же операции/машины/intent.
  Машина должна реально проехать≥100м с≥5 velocity polls; все исходные fit
  должны физически дойти пешком в35м от прежней цели. Расстояние машины
  сохраняется как наблюдение: этот результат не доказывает её прибытие≤150м.

Обе категории требуют прежние paid recruitment, исходный roster, единственную
покупку, current lease, restored order, полный срок1800с и строгие native gates.
`alive` использует `AICF_GroupRuntime.CountAliveAgents`: учитываются только
characters с `ECharacterLifeState.ALIVE`. `INCAPACITATED` в этот счётчик не входит.
Поэтому `CASUALTY observed_change=ALIVE_COUNT_DECREASE cause=UNDETERMINED`
не доказывает смерть: штатная помощь может вернуть прежнего actor вALIVE без
создания нового бойца. Причина снижения не приписывается выстрелу, падению или
коррекции позиции без отдельного native evidence.
Вышедший из-за боя отряд не объявляется прибывшим в момент выхода. Probe ждёт
настоящего foot arrival либо возврата TRANSIT к прежней цели. Нет truce, damage
protection, лечения, воскрешения или продления длительности.

Угроза во время начального ожидания в машине подтверждается собственным native
TACTICAL deployment, а не произвольной потерей места. После возврата исходных
пяти и окончания угрозы непрерывное спокойное ожидание начинается заново.
`MOUNTED_COMBAT_WAIT`/`MOUNTED_HOLD_RESUMED` сохраняют причинность; `MOUNTED_HOLD`
проверяется также по реальным engine clocks. Потеря/замена бойца, необъяснённая
высадка, отсутствие движения или настоящий bounded fallback остаются FAIL.

Исторические proof1/2 разбираются с явной соответствующей версией. Изменение
критерия не превращает их PREMATURE_TRANSIT_EXIT или иной FAIL в PASS.

Свежие результаты и известные ограничения — в [TESTING.md](TESTING.md).

`soak-US-v3`/`soak-USSR-v3` в evidence `vehicle-behavior-20261028` используют
одинаковые 310 source files и frozen Proof3 auditor. Они стартовали до добавления
`AICF_RecruitmentMoveFailure`: последующий короткий выход из STUCK на подходе к
казарме не входит в эти snapshots. Новый controlled recruitment probe проверяет
свою физическую цепочку отдельно; он не заменяет новый ordinary1800s gate.
Старые proof3 без `casualty_policy` сохраняют прежний strict all-five-ALIVE oracle
и не переоцениваются в PASS. Новые START с явной policy требуют по пять
`MEMBER_FOOT` и свежих `MEMBER_ARRIVAL`: original EntityID/позиция, life,
conscious/fit, current membership, actual target distance и physical detach.
Fit означает ALIVE и conscious; нужен хотя бы один такой original. Все fit
должны быть текущими участниками и реально прибыть≤35м, остальные исходные
incapacitated/dead actors остаются отдельно проверенными снаружи compartment,
transition и расширенных bounds. Исчезнувший identity, replacement, новый
живой участник, тело в машине или отсутствие фактического прибытия запрещают PASS.
Initial paid recruitment, исходные пять, exact-five boarding/calm hold, lease,
intent, restored order, duration, accounting и strict native gates не ослаблены.
`Test-VehicleSoakSurvivorContracts.ps1` выполняет extracted predicate и source
negative controls; SoakLog contracts отдельно отвергают mounted incapacitated,
replacement, fabricated arrival и неполный исходный платный состав.

Очередь preset boarding выдаёт только один вход за раз и ждёт любые actual
transitions, включая смену места. Stalled owned action после20s без прогресса
может owner-safe уступить ещё не обслуженным бойцам. Retry выбирается только
среди готовых actor/seat пар, сначала с меньшим retry count; foreign reservation
и locked seat не удерживают очередь других готовых мест. Максимум2 retry и общий
срок25s×max(3, assigned) не продлеваются. `VEHICLE_PRESET_SEAT_YIELDED` отличает
уступку от `VEHICLE_PRESET_SEAT_RETRY`. Сам по себе unit contract не подтверждает
физическую возможность входа через конкретную дверь: нужен native receipt.

После GC permission GroupTrace также содержит native garbage tracking/lifetime/remaining и current permission. Pending Insert до native callback не является возобновлённым tracking. Наблюдение нужно до возможного исчезновения retained машины; исчезновение UAZ в v3 не объявляется доказанным GC без causal receipt. Старые v3 snapshots не содержат нового GC guard.
