# Физическая AI-логистика: реализация и evidence

С 2026-09-09 автоматическая отправка заменена формой «Снабжение»; с 2026-09-13
новый рейс создаётся по кнопке «Отправить» после выбора источника и назначения.
Текущий контракт и проверки:
[SUPPLY_MAP_UI.md](SUPPLY_MAP_UI.md). Ниже сохранено историческое evidence
транспортной реализации, полученное при прежнем автономном dispatcher.

Дата: 2026-09-06. Исходная ревизия: `27f1b86b2af6f5d83b080b225a929267dcfcc1ad`.
ТЗ: [AI_LOGISTICS_IMPLEMENTATION_PROMPT.md](AI_LOGISTICS_IMPLEMENTATION_PROMPT.md).

## Итоговые gates

Timer credit заменён physical vehicle cargo, production-код и документация
добавлены. Реальные delivery/reuse/return доказаны на stock Everon; delivery и
положительный replicated cargo доказаны на RHS. Полная runtime матрица не зелёная.

| Gate | Verdict | Evidence / exit |
|---|---|---|
| Исходный static baseline | PASS, 11/11; baseline failures нет | `baseline-*.log`, exit 0 |
| Финальный static без Core fixture | PASS, 13/13; LogisticsContracts 37/37 | `final-static-results.json`, все exit 0 |
| Workbench Arland | PASS | `final-workbench-arland/`, exit 0, Game successfully created, SCRIPT errors 0 |
| Workbench Everon | PASS | `final-workbench-everon/`, exit 0, Game successfully created, SCRIPT errors 0 |
| Workbench RHS | PASS | `final-workbench-rhs/`, exit 0, Game successfully created, SCRIPT errors 0 |
| Stock Arland server | FAIL полного runtime gate | Stock 20: нет unload; Stock 21: preparation/прерванный запуск. Успешная vanilla Arland delivery не доказана |
| Stock Everon server / 30-minute soak | FAIL полного gate; physical trips/return/balance подтверждены | Soak 22, launcher 0, auditor 1; несколько depot, repeated trips, 800 = 600 + 200 |
| RHS server | FAIL полного gate; AFRF delivery подтверждена | RHS 3, launcher 0, auditor 1; native faction errors и protected cleanup |
| RHS client / союзный JIP data | PASS exact pair/cargo observations | Client 3 exit 0; 29 samples, 12 loaded, SCRIPT/VM errors 0 |
| Combined RHS server+client | FAIL | `rhs-3-audit.log`, exit 1 только по server errors; client checks без нарушений |
| Enemy UI / manual visual | NOT RUN | Требуется ручной verdict пользователя |

Временная копия `AICF_LogisticsRuntimeProbe.c` удалена из Core до финальных gates;
каноническая fixture остаётся только в `tools/fixtures/`. До подготовки коммита
все production `.c` совпадали по SHA256 с завершённым RHS 3:
`final-source-runtime-comparison.json`, `changed_production_sources=0`.
При staged-проверке удалены только лишние пустые строки в конце
`AICF_LogisticsService.c`; неизменность остального байтового содержимого сохранена
в `commit-whitespace-cleanup.json`. Поведение runtime не менялось.
Репозиторий не объявляется `ACCEPTED`.

## Границы проверки

Static audit, Workbench Validate и dedicated runtime являются отдельными gates.
Код возврата dedicated server сам по себе не доказывает даже успешную компиляцию:
в промежуточных прогонах ошибки компиляции попали в `error.log`/`crash.log`,
тогда как launcher напечатал `exit_code=0`. Проверяются все полные логи.

Evidence хранится локально в `.codex-runtime/logistics-20260906/` и исходных
profiles `%LOCALAPPDATA%/AICF/Logistics-20260906-*`. Generated evidence не входит
в Git. `*-launch.log` содержит команду, `AICF_RUNTIME_MANIFEST_JSON` и exit code;
`*-logs/` содержит полные `console.log`, `script.log`, `error.log`, при наличии
`crash.log`. Отфильтрованные события используются только как индекс.
Абсолютный корень сохранённого evidence:
`C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict\.codex-runtime\logistics-20260906`.
Исходные profiles находятся в `C:\Users\retar\AppData\Local\AICF`.
В dedicated logs подтверждён Reforger `1.8.0.13`, build `2026-08-21`, engine `192142`.

## Реализованные границы

`MatchController` создаёт службу и вызывает её только после `ROSTER_READY`,
на authority во время активного матча. Логистика обеих сторон не зависит от
`AICommanderMode`. `Stop()` сначала закрывает jobs/transfers и снимает subscriptions,
затем передаёт существующие leases в общий cleanup.

Registry связывает `faction + numeric slot + ordinal` с exact production root,
provider и native base. Несколько service components одного здания не создают
дубликатов ordinal. Catalog admission проверяет конкретный prefab и разрешённый
stock slot. Stock `IsOccupied()` здесь не вызывается: его callback может удалять
wrecks и обращается к отсутствующему `Physics`. Собственная read-only проверка
использует OBB конкретного prefab без расширения площадки и без исключения props
самого depot; отдельно проверяются surface и общие spatial reservations.
С 2026-09-06 доказательство свободного физического выезда больше не требуется:
[исправление logistics spawn](LOGISTICS_SPAWN_CLEARANCE.md).

Один driver создаётся через существующий одиночный roster workflow. Fleet lease
берётся из существующего `AICF_FactionFleet` с cap 10. Идентичность и фактическое
водительское место проверяются до каждого job и transfer. Единственная native
точка создания vehicle остаётся в `AICF_VehicleSpawner`; переходы фаз — в
`AICF_TransportTripController`, waypoint boundary — в `AICF_VehicleTaskHandoff`.

Planner использует directed HQ BFS существующего objective graph, hysteresis
40/80/90/80, текущие запасы и общий ledger входящих/исходящих reservations.
Отдельная road query не подменяется radio edge. Место операции должно лежать
в реальном радиусе базы. Возврат допускает собственное изолированное хранилище;
нейтральный возврат ограничен доказанным исходным physical pool и cargo batch.

Adapter проверяет базовые `DEFAULT` и vehicle `VEHICLE_LOAD`/`VEHICLE_UNLOAD`
operation endpoints, права и текущие очереди контейнеров, затем работает с exact physical leaf containers. Для баз,
не имеющих прямого container, leaves берутся из актуальной очереди `DEFAULT`
consumer после `ResourceGrid.UpdateInteractor`. Aliases одинакового набора leaves
получают общий pool, частично пересекающиеся наборы разделяют reservations
консервативно. Transfer измеряет обе стороны, компенсирует только подтверждённый
debit и хранит idempotent receipt. Неопределённая identity не превращается в loss.

Schema 2 сохраняет float amounts и равенство:

```text
loaded + external_in = delivered + returned + in_transit + lost + released + external_out
```

Ненулевые `discrepancy`, `unknown_state` и retained custody остаются видимыми.
Уменьшение supplies внутри штатного `UpdateSuppliesFireState` учитывается как
подтверждённый `lost`, включая пожар ещё не уничтоженной машины. Наблюдатель не
меняет damage behavior и отключается после завершения cargo custody.
Общий delete boundary дополнительно обходит vehicle hierarchy/attachments и
запрещает удаление surviving supplies, в том числе после release и при Stop.
Старый timer-based shipment и его enum удалены; aggregate facade обслуживает UI
и heartbeat по реальным receipts/custody. Legacy delivery CLI игнорируется
с явным `LOGISTICS_CONFIG_DEPRECATED`.

## Baseline и статические проверки

До правок были выполнены все 11 обязательных команд из ТЗ:
`Test-Stage3Static`, `Test-Stage35Static`, `Test-Stage3StaticContracts`,
`Test-Stage35RecoveryPolicy`, `Test-Stage4Static`, `Test-AICommanderModeStatic`,
`Test-MapPointOrdersStatic`, `Test-RHSIntegrationStatic`, `Test-BaseBuildersStatic`,
`Test-ConstructionStatic`, `Test-ConstructionContracts`. Все завершились с
`exit=0`; сохранённых baseline failures нет.

Формат команды:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage3Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsLog.ps1 -LogPath <полный-console.log> -RequireDelivery -RequirePolicy
```

Новые positive/negative fixtures проверяют fractional receipts, duplicate operation,
generation/ready fence, alias self-transfer, late work после Stop, nonfinite numbers,
скрытый двойной release, unmatched pair и supply conservation. Source mutations
проверяют strict config, границу threshold, receipt latch, exact pilot, allowed slot
и общий cargo delete guard. Эти проверки не являются копией planner на PowerShell.
Текущий набор содержит 37 positive/negative cases, включая запрет считать собственного
водителя посторонним, точные operation pools, resource rights, consuming state
и запрет использовать торговый BuyMultiplier как условие physical transfer,
continuation candidate preparation и пропуск несовместимого catalog entry.
Дополнительно проверяются custody fault при формально нулевом балансе и
client/server correlation по slot, generation, driver/vehicle RplId, actual capacity
и ненулевому float cargo. Отрицательные fixtures включают чужой RplId,
старую generation, NaN, только пустую машину и незавершённый client log.

## Runtime test fixture

`tools/fixtures/AICF_LogisticsRuntimeProbe.c` временно копируется в Core/Economy
только для dedicated probes. Перед финальным compile копия удаляется.
В production addon нет test CLI, preparation или изменяющей gameplay fixture.

При `aicfLogisticsProbePrepare=1` fixture создаёт native depot compositions,
назначает exact provider/faction и достраивает их без оплаты. Последняя версия
использует construction metadata и bounded поиск свободного участка. Fixture
назначает владельца доступной обычной базы, задаёт запасы до старта поездок и
логирует каждое такое воздействие. Warm-up восстанавливает эти условия до первой
загрузки, максимум 300 секунд; это не прогон естественной экономики. Дополнительный
`aicfLogisticsProbeRepeatSource=1` продолжает такую подготовку раз в 60 секунд
после warm-up; каждый refill/owner change отмечен `test_only=1`.
`aicfLogisticsProbeDepotAtSource=1` переносит начальную подготовку light depot
на собственную базу-источник. При исчерпании 512 placement attempts fixture
однократно пробует HQ; production acquisition geometry не меняется.
`aicfLogisticsProbePeace=1` устанавливает дружественные отношения FIA с US/USSR;
`aicfLogisticsProbeMaintainDemand=1` каждые 30 секунд потребляет supplies HQ,
печатая actual before/after и удалённый объём. Эти параметры существуют только
в test fixture. Подготовка
не заменяет production planner, driver/vehicle acquisition, движение или transfer.
`aicfLogisticsProbeReturn=1` после первой delivery и следующей загрузки заполняет
получателей и сохраняет место в исходном pool; production самостоятельно отменяет
неподходящий job и выбирает RETURN. Fixture наблюдает результат с deadline 600 s;
сама unload/return не выполняет. `LOGISTICS_PROBE_MOVE_FAILED` только наблюдает
native `OnMoveFailed` и не отменяет штатную GetOut activity.

Pure policy probe вызывает production `DemandOpen`, `Deficit`, `Donatable`,
strict parser и `Validate`: 15 проверок. Adapter probe вызывает production
`Transfer` на настоящих stock pools: fractional transfer, replay, обратный перенос,
canonical alias, отказ credit с компенсацией, отказ компенсации с discrepancy,
запрет операции после Stop. Снимки подготовки восстанавливаются синхронно до
следующего scheduler tick. Probe 14 подтвердил **15/15 policy и 7/7 adapter**;
это не является доказательством физической доставки.

В soak 20 также выполнено **18/18** проверок настоящего production ledger:
общие outgoing/incoming reserves, competing return, physical aliases, duplicate
worker reservation, cancel twice, TTL, generation mismatch, progress timeout,
новый token и Stop twice. Только precondition readiness замокан; stock pools и
`Reserve/Renew/Cancel/Stop` настоящие. Everon 3 подтвердил **12/12** graph fixture:
production BFS с mock ownership на directed cycle/multiple parents, минимальная
depth, tie-break, направление edges, isolation и изменение ownership. RHS 2
подтвердил **6/6** continuation/priority checks настоящих `PrepareCandidates`
и `Before`: общий budget, продолжение на следующем tick, конечность,
HQ/depth 1/depth 2 и возраст запроса. Mock snapshot дополнен повторёнными
endpoint references только внутри test worker; production inventory не меняется.

Client fixture помечает фактического driver реплицируемыми slot/generation и
пишет наблюдения exact pilot, vehicle Rpl identity, cargo и position. Наблюдение
имеет ограниченное число callbacks с удалением при destruction. Союзный/вражеский
UI и визуальная геометрия требуют отдельного доказательства; без него — `NOT RUN`.
Опциональный `aicfLogisticsClientSpawnFaction=US|USSR` делает до 12 обычных
native faction/respawn requests local player, выбирая active spawn point у HQ
и персонажа active content profile. Native authority validation и стоимость
player spawn сохраняются; player не получает управление logistics driver.
Для актуальных aggregate values observer использует штатный
`RequestSubscriptionListenerHandle` с RplId inventory component local player.
Handle освобождается при смене машины и после 30 observations. Production resource
replication не изменяется; test observer не вводит собственный RPC с cargo.

Everon client 22 выполнил native faction/respawn и 29 observations exact USSR
driver/vehicle, `pilot=1`, actual capacity 800, движение. Он читал generator без
подписки и наблюдал только cargo 0: positive-cargo JIP этим не доказан.
Everon client 23 пытался подключиться с обновлённым observer к уже работающему
server 22; native checksum validation правильно отвергла разные scripts
(`local=0x6DF1BF10FDAC29AF`, `remote=0x8948715C1B4AAD57`). Это FAIL подключения;
проверка не отключалась. Для нового observer нужны заново запущенные server/client
с одинаковым source hash.

## Подтверждённые content observations

| Profile | Faction | Depot | Entry из действующего каталога | Capacity metadata |
|---|---|---|---|---:|
| Stock Arland | US | Light | `M998_covered_long_MERDC.et` | 315 |
| Stock Arland | US | Heavy | `M923A1_transport_MERDC.et` | 1500 |
| Stock Arland | USSR | Light | `UAZ452_cargo.et` | 800 |
| Stock Arland | USSR | Heavy | `Ural4320_transport.et` | 1500 |
| RHS Arland | RHS_USAF | Light | `M998_covered_long_USAF.et` | 315 |
| RHS Arland | RHS_AFRF | Light | `UAZ469_Camo.et` / `UAZ469_Camo_uncovered.et` | 180 / 120 |

Capacity metadata прочитана из enabled supply attachments, а не из числа сидений.
Она не доказывает actual live cargo capacity или успешный spawn. Exact production
slot дополнительно проверяется при acquisition. В наблюдавшемся stock Arland graph
не было `SOURCE_BASE`; neutral-source runtime этим сценарием не покрывается.
RHS USAF light подтвердил actual capacity 315 и загрузку; RHS 3 также подтвердил
AFRF `UAZ469_Camo.et` с actual capacity 180, exact single pilot, load/delivery.
Вариант uncovered 120 пока подтверждён metadata и native catalog/slot compatibility.
Heavy AFRF placement fixture
получила unsupported geometry до поиска; это не доказательство отсутствия
подходящих готовых native heavy depot. Universal RHS truck fallback не добавлен.

Прогоны 16–18 подтвердили exact light-depot spawn и фактические capacities
**US 315 / USSR 800** с `agents=1`, `cargo=0`, exact settled pilot seat. Прогон 18
с клиентом подтвердил stock replicated `VEHICLE_LOAD` generator values для USSR:
`pilot=1`, `supplies=1`, `cargo=0`, `capacity=800`, совпадающий vehicle Rpl id.
На proxy stock container может отсутствовать; fixture читает реплицируемые
aggregate values штатного generator, а не придуманное клиентское cargo.

## Физические наблюдения 16–20

Everon 4 подтвердил два полных `load → drive → unload` одной парой. Остальные
строки сохраняют точный смысл отдельных промежуточных evidence.

| Прогон | Наблюдение | Verdict |
|---|---|---|
| Stock 16, mode US | Обе фракции: пустая машина и один seated AI; старая cleanup-проверка ошибочно считала собственного driver посторонним | FAIL; исправлено отдельным `HasForeignOccupant` |
| Stock 17, mode US + client/JIP | Обе машины физически двигались; оба водителя погибли. Полный cleanup log содержит `life_state_DEAD` | Movement/identity evidence; delivery FAIL |
| Stock 18, mode US + client | US загрузил 200: source `1000 → 800`, cargo `0 → 200`, discrepancy 0; машина погибла в пути | Load PASS; delivery FAIL |
| Stock 20, mode BOTH, WorkersPerDepot=2 | Стабильные разные ordinals; replacement сохраняет slot. US загрузил 200, доехал до HQ и остановился | Load/drive/arrival evidence |
| Stock 20 | Выгрузка отклонена ошибочной проверкой BuyMultiplier; после bounded retries сохранённый cargo передан в world pool | Release PASS; unload FAIL |
| Everon 3, mode USSR | US exact driver/capacity 315, загрузка 200 и движение с грузом; отдельные light и heavy USSR depot с разными slots | Targeted load/depot evidence; delivery не завершена до Stop |
| Everon 4, mode USSR | US slot 1000000 generation 1: load 200, drive, unload 200 в 293984 ms; повторный load/unload 200 в 524691 ms, без replacement | Physical delivery/reuse PASS; полный runtime gate FAIL только по native teardown SCRIPT errors |
| RHS 2, mode US | USAF exact single driver, actual capacity 315; два load по 315 с native base `1600 → 1285`; driver начинает GetOut до HQ | Load/identity/replacement PASS; delivery FAIL |
| RHS 2 + client 2 | Первый груз 315 released; второй 315 retained при `PLAYER_POSITION_UNKNOWN`; final balance 0, discrepancy 0 | Custody fail-closed evidence; clean terminal release и positive-cargo JIP не подтверждены |

Каждый PASS в таблице относится только к указанному наблюдению. Stock 20 завершён
штатно с exit 0 и `Game destroyed` после 1 865 368 ms. 186 samples: FPS
58.6–60.2, средний 59.919; память 1 502 959–1 589 173 kB. Конечный cargo ledger:
US loaded/released 309; USSR loaded 200, stock fire loss 1.74997, released 198.25;
in_transit и discrepancy равны 0, balance_delta 0 в рамках epsilon 0.01.
Повторный Stop завершил terminal cleanup. Общий soak gate **FAIL**: нет unload,
два native SCRIPT error при teardown. Полные logs: `stock-soak-20-logs/`,
команда/manifest: `stock-soak-20-launch.log`, verdict: `stock-soak-20-audit.log`.

Everon 3 доказал причину отказа: exact queues совпадают, права разрешены, но
stock VEHICLE_UNLOAD имеет `GetBuyMultiplier()=0`. В закреплённом
`SCR_ResourceConsumer.RequestConsumtion` этот торговый коэффициент не участвует
в физическом debit. Production guard исправлен на `IsConsuming`, права и exact
pool сохранены; Workbench 20/Everon 5 и 26 contracts проходят. Исправление требует
нового полного физического рейса; Everon 4 выполнил два таких рейса.

## Stock Everon soak 22: 30 минут, несколько depot, delivery и return

Server `everon-soak-22`: mode USSR, WorkersPerDepot=2, probe duration 1 800 000 ms,
`Prepare/Peace/MaintainDemand/RepeatSource/Return=1`. Начало полного log
15:30:34, `LOGISTICS_STOP` в 16:00:57 (`t_ms=1804667`), `Game destroyed` в
16:02:01, launcher exit 0. Подготовленный light depot USSR root
`0x40000000000041D0` дал ordinals 0/1 и slots 1000000/1000001. Обычная production
construction в 15:43:43 достроила второй light depot `0x400000000000496E`
с реальной стоимостью 150 (`supplies 1032 → 882`); registry добавил slots
1000002/1000003. Здесь второй depot создан не test fixture.

| t_ms | Slot / generation | Физическая операция | Actual readback |
|---:|---|---|---|
| 236380 | 1000000 / 1 | LOAD 200 | source 1000 → 800; cargo 0 → 200 |
| 357597 | 1000000 / 1 | DELIVERY 200 | cargo 200 → 0; HQ 60 → 260 |
| 497544 | 1000000 / 1 | LOAD 200 | source 1000 → 800; cargo 0 → 200 |
| 688550 | 1000000 / 1 | RETURN 200 | cargo 200 → 0; original pool 708 → 908, `same_pool=1` |
| 734691 | 1000000 / 1 | LOAD 200 | source 1000 → 800; cargo 0 → 200 |
| 851296 | 1000000 / 1 | DELIVERY 200 | cargo 200 → 0; HQ 100 → 300 |
| 911459 | 1000001 / 2 | LOAD 200 | source 1000 → 800; cargo 0 → 200 |
| 1020806 | 1000001 / 2 | DELIVERY 200 | cargo 200 → 0; HQ 60 → 260 |

У первой пары неизменные driver/vehicle generation на трёх рейсах, actual
capacity 800. У второго ordinal replacement увеличил generation, сохранив slot.
Итоговый ledger: loaded 800, delivered 600, returned 200; in_transit/lost/released/
external_in/external_out/discrepancy/balance_delta равны 0. Policy 15/15, adapter
7/7, ledger 18/18, graph 12/12, search 6/6 повторены в этом же stopped log.
После повторного Stop новые jobs/transfers отсутствуют.

182 performance samples: FPS 10.3–60.1, средний 58.971; память
3 556 979–3 699 533 kB. Probe измеряет только production `Update`:
1736 calls, суммарно 3162 ms, среднее 1.82 ms, максимум 49 ms, 4 workers,
41 graph nodes. Test preparation в это измерение не входит. Это измерения
конкретного прогона, не гарантия производительности на произвольной карте.

Physical trips/return/balance и длительность подтверждены, но полный **soak FAIL**
(`Test-LogisticsLog` exit 1). Полные logs содержат семь VM exceptions в native
movement/defend behavior tree (`NodeError`), один
`ORDER_REPAIR_ACCOUNTING_INVARIANT_FAILED` (`unaccounted=1`, с error bridge) и
четыре `VEHICLE_CLEANUP_RETAINED` после native GetOut/потери seated postcondition.
Причина удержания — `PLAYER_POSITION_UNKNOWN`; в конце четыре пустых leases
остались под cleanup ownership с cap HELD. Это не completed cleanup, даже при
нулевом cargo balance. Есть также отклонённое checksum-подключение client 23.
Эти новые наблюдения не объявляются историческими baseline failures.

Полные 29 460 строк и companion logs:
`C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict\.codex-runtime\logistics-20260906\everon-soak-22-logs\`.
`summary.json` содержит hashes каждого полного log, transfers, final balances
и performance. Команда/manifest: `everon-soak-22-launch.log`; verdict:
`everon-soak-22-audit.log`; source hashes: `everon-soak-22-source-sha256.csv`.
Client 22/23 сохранены полностью в `everon-client-22-logs/` и
`everon-client-23-logs/` с отдельными launch manifests.

## RHS 3 и client 3: actual AFRF delivery и положительный JIP cargo

Server mode US, WorkersPerDepot=1, test duration 600000 ms; запуск
16:02:31, repeated Stop в 16:12:47 (`t_ms=604658`), `Game destroyed` в
16:13:49, launcher exit 0. Обе стороны получили native light depot и по одному
exact driver: AFRF UAZ capacity 180, USAF M998 capacity 315. Ready AFRF
slot 1000000/generation 1 подтверждён в 36072 ms; USAF slot 1000001/generation 1
в 115512 ms. Source hashes server/client fixture совпадают.

AFRF загрузил 180 в 170904 ms (`source 1000 → 820`, `cargo 0 → 180`) и физически
доставил в HQ в 289486 ms (`cargo 180 → 0`, `HQ 80 → 260`). Та же пара повторно
загрузила 180. USAF дважды загрузил по 200; после native потери seated
postcondition первый груз 200 один раз released, replacement сохранил slot и
увеличил generation. USAF delivery в этом прогоне не состоялась.

Итоговый ledger: AFRF loaded 360, delivered 180, in_transit 180;
USAF loaded 400, released 200, in_transit 200. Возвратов и loss нет,
discrepancy/balance_delta/fault равны 0. Оставшиеся 380 supplies физически
сохранены под protected custody двух leases: после Stop clearance не смог
доказать position игрока. Это не completed cleanup и не supply drift.
RHS RETURN до deadline этого прогона не завершён; доказанный physical return
остаётся в Everon 22.

Server policy/adapter/ledger/graph/search: 15/15, 7/7, 18/18, 12/12, 6/6.
66 FPS samples: 46.2–60.0, средний 59.715; память 1 966 770–2 002 460 kB.
Production Update: 592 calls, 409 ms суммарно, максимум 30 ms, 2 workers,
9 graph nodes. Полный **server gate FAIL**, auditor exit 1: шесть известных
RHS faction init SCRIPT errors и два `VEHICLE_CLEANUP_RETAINED` с error bridges.
Новых VM exceptions в этом полном log нет.

Client launcher подтвердил живой server PID 12712, port 2001, exact CLI и
`ROSTER_READY`. Союзный JIP client вошёл в RHS_AFRF через native respawn.
В полном остановленном client log 1176 строк, **29 observations, 12 loaded**:
`slot=1000000`, `generation=1`, `driver_rpl=-2147469800`,
`vehicle_rpl=-2147470169`, `pilot=1`, `capacity=180`, цикл cargo `0 → 180 → 0`
и изменяющаяся физическая position. Observations положительного cargo
16:05:34–16:07:24 совпадают с server load/unload. Client exit 0;
SCRIPT/VM/AICF errors отсутствуют. Native UI resource leaks при teardown
сохранены в полном `error.log` и не выдаются за script compile failures.

Exact identity/capacity/loaded-client проверки общего аудитора нарушений не
выдали; его общий exit 1 обусловлен перечисленными server errors. Поэтому
**client/JIP data gate PASS**, **combined server+client runtime FAIL**,
manual visual и enemy UI leakage **NOT RUN**.
Полные logs: `rhs-3-logs/`, `rhs-client-3-logs/`; manifest/exit:
`rhs-3-launch.log`, `rhs-client-3-launch.log`; verdict `rhs-3-audit.log`;
source hashes `rhs-3-source-sha256.csv`. Все пути относительно абсолютного
evidence root, указанного выше.

## Промежуточные отказы

Ранние probes обнаружили: stock slot callback с null physics, отсутствие прямого
base container, supplies в SlotManager attachments, strong-ref и числовые ошибки
в test preparation, слишком широкие slot bounds без штатных exclusions,
непригодное размещение fixture возле скалы/деревьев и недостаточный radius road query.
Исправления не подменяют отрицательные прогоны положительными.

Stock teardown в нескольких остановленных runs записал два SCRIPT error от
`SCR_BaseResupplySupportStationComponent` об отсутствии entity catalog manager.
Log auditor сохраняет их как FAIL; `exit=0` не отменяет ошибку. Причина и отношение
к baseline проверены по сохранённым полным logs до этой задачи:
`C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict\.codex-runtime\Scenario-Stock-Direct-20260829-234106\logs\logs_2026-08-29_23-41-06\console.log`
содержит тот же текст на строках 177–178. Это историческое сравнение, отдельный
runtime baseline в начале текущей реализации не запускался.

RHS init содержит шесть `SCR_Faction ... not a valid SCR_Faction` для `US`,
`USSR`, `RHS_ION`. Точный набор подтверждён также в историческом полном
`C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict\.codex-runtime\Scenario-RHS-Direct-20260829-234237\logs\logs_2026-08-29_23-42-38\console.log`
(строки 247–252, teardown 270–271). Ранее независимый baseline без builder service
описан в [BASE_BUILDERS_VALIDATION.md](BASE_BUILDERS_VALIDATION.md).
Installed RHS/faction resources не менялись; аудитор продолжает возвращать FAIL.

Stock 17/18 с client без spawned player character также достигал existing
`VEHICLE_CLEANUP_RETAINED` с `PLAYER_POSITION_UNKNOWN`: cap удерживается, unsafe
delete не выполняется. Эти ошибки не скрываются из общего runtime verdict.

Попытка Everon 1 с `-bindPort 2002` в direct `-server` режиме всё равно пыталась
слушать 2001 и завершилась `Unable to start replication`. Следующие targeted
runs выполняются последовательно на свободном 2001. Справочник startup parameters
описывает `bindPort` как override server config; это не доказало его применимость
к данному direct-world Diag запуску: [Bohemia startup parameters](https://community.bistudio.com/wiki/Arma_Reforger%3AStartup_Parameters).

Soak 19 остановлен после VM exception в test-only snapshot restoration: после
смены владельца состав pool отличался от старого снимка. Snapshot теперь снимается
после fresh `Resolve` с того же exact pool. Сохранены полные crash logs и команда
остановки exact PID/profile; этот запуск не засчитывается как 30-минутный soak.

Batch `pass4-*`: 12 аудиторов exit 0; `AICommanderModeStatic` exit 1 из-за временной
второй `modded SCR_GameModeCampaign` в fixture. После удаления копии batch
`pass5-production-*` подтвердил все 13 аудиторов с exit 0. Это не baseline failure.

Stock soak 21 остановлен после 649642 ms: fixture исчерпала 512 попыток light
US depot placement с `PHYSICAL_OBSTRUCTION`, а готовый USSR depot не получил
пригодного donor. Сохранены exact PID/profile stop evidence и полные
`stock-soak-21-logs/`; launcher exit -1. Auditor exit 1, в том числе
`FULL_STOPPED_LOG_REQUIRED`, `PHYSICAL_DELIVERY_NOT_OBSERVED`, `DURATION_TOO_SHORT`.
Этот прерванный запуск не засчитывается как soak.

## Объём изменений и effective defaults

Новые production files под `AIConflictCore/Scripts/Game/AIConflict/`:

- `Config/AICF_LogisticsConfig.c`.
- `Economy/AICF_LogisticsService.c`, `AICF_LogisticsPlanner.c`,
  `AICF_LogisticsDepotRegistry.c`, `AICF_LogisticsJob.c`,
  `AICF_LogisticsResourcePool.c`, `AICF_LogisticsLedger.c`,
  `AICF_LogisticsDamageAccounting.c`.
- `Vehicles/AICF_LogisticsAcquisitionFlow.c`, `AICF_LogisticsVehicleFootprint.c`.

Расширены существующие владельцы: `AICF_MatchController`, `AICF_EconomySystem`,
`AICF_FactionFleet`, `AICF_VehicleCoordinator`, `AICF_TransportTripController`,
`AICF_VehicleSpawner`, `AICF_VehicleTaskHandoff`, `AICF_VehicleTransitFlow`,
`AICF_VehicleDismountFlow`, `AICF_VehicleCleanupManager`,
`AICF_VehicleWaypointFactory`. Изменены Core/RHS content profiles и
`AICF_Stage4Config`; `AICF_SupplyDeliverySystem` стал aggregate facade,
`AICF_SupplyShipment.c` и старый shipment enum удалены.

Обновлены `.gitignore`, README, ARCHITECTURE, SERVER_SETUP, TESTING,
`Test-Stage4Static.ps1`, `Test-Stage4Log.ps1`. Добавлены этот отчёт,
`Test-LogisticsStatic.ps1`, `Test-LogisticsContracts.ps1`, `Test-LogisticsLog.ps1`,
`tools/fixtures/logistics/` и test-only runtime fixture. Исходное ТЗ не изменено.
Project GUID, mission headers, world/layout/prefab resources и installed addons
не менялись.

Defaults: request строго ниже 40%, target 80%, donate строго выше 90%, keep 80%;
neutral SOURCE reserve 0, owned SOURCE reserve 500, minimum dispatch 50,
MaxCargoPerTrip 0 (ограничение actual capacity). WorkersPerDepot 1, разрешено 1–4;
auxiliary cost 0, прежний shared fleet cap 10. Planner 10 s, poll 1 s,
reservation TTL 30 s, replacement/blocked retry 60 s, три return searches,
idle retire 120 s, arrival 15 m, stationary 0.5 m/s непрерывно 3 s.
Spawn timeout 30 s, leg timeout 600 s, progress timeout 60 s, две route retries,
registry budget 4, candidate budget 16, resource epsilon 0.01.
Полная CLI-таблица и validation:
[SERVER_SETUP.md](SERVER_SETUP.md#параметры-физической-логистики).

Уточнение 2026-09-14: приведённые выше WorkersPerDepot и shared fleet cap
описывают исторический прогон. Для ручных перевозок оба лимита машин сняты;
новые workers выделяются по заявкам, бюджет AI сохранён. Текущая политика и
проверки — [SUPPLY_MAP_UI.md](SUPPLY_MAP_UI.md), [TESTING.md](TESTING.md).

## Команды и расположение evidence

Все команды выполняются из
`C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict`. Полный список 36
runtime launch manifests с executable, массивом exact CLI arguments, profile и
строкой native exit хранится в
`.codex-runtime/logistics-20260906/runtime-manifest-index.json`.
Фактический `CLI Params` остаётся в каждом полном server/client console.log;
launcher перед client записывает `SERVER_READY` с process ID и port 2001.

Ключевые запуски текущего physical implementation:

```powershell
& tools/Start-AICFRuntime.ps1 -Role Server -Variant Everon -AICommanderMode USSR `
  -ProfileRoot "$env:LOCALAPPDATA/AICF/Logistics-20260906-everon-soak-22" `
  -AdditionalArguments @('-aicfLogisticsProbe','1','-aicfLogisticsProbePrepare','1',
    '-aicfLogisticsProbeDurationMs','1800000','-aicfLogisticsProbePeace','1',
    '-aicfLogisticsProbeMaintainDemand','1','-aicfLogisticsProbeRepeatSource','1',
    '-aicfLogisticsProbeReturn','1','-aicfLogisticsWorkersPerDepot','2')

& tools/Start-AICFRuntime.ps1 -Role Server -Variant RHS -AICommanderMode US `
  -ProfileRoot "$env:LOCALAPPDATA/AICF/Logistics-20260906-rhs-3" `
  -AdditionalArguments @('-aicfLogisticsProbe','1','-aicfLogisticsProbePrepare','1',
    '-aicfLogisticsProbeDurationMs','600000','-aicfLogisticsProbePeace','1',
    '-aicfLogisticsProbeMaintainDemand','1','-aicfLogisticsProbeRepeatSource','1',
    '-aicfLogisticsProbeReturn','1')

& tools/Start-AICFRuntime.ps1 -Role Client -Variant RHS -AICommanderMode US `
  -ServerProfileRoot "$env:LOCALAPPDATA/AICF/Logistics-20260906-rhs-3" `
  -ProfileRoot "$env:LOCALAPPDATA/AICF/Logistics-20260906-rhs-client-3" `
  -AdditionalArguments @('-aicfLogisticsClientProbe','1',
    '-aicfLogisticsClientSpawnFaction','USSR')

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsLog.ps1 `
  -LogPath .codex-runtime/logistics-20260906/everon-soak-22-logs/console.log `
  -RequireDelivery -RequirePolicy -RequireLedger -RequireGraph -RequireSearch `
  -MinimumDurationMs 1800000

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LogisticsLog.ps1 `
  -LogPath .codex-runtime/logistics-20260906/rhs-3-logs/console.log `
  -ClientLogPath .codex-runtime/logistics-20260906/rhs-client-3-logs/console.log `
  -RequireDelivery -RequirePolicy -RequireLedger -RequireGraph -RequireSearch `
  -RequireLoadedClient
```

Launch stdout/stderr сохранён через `Tee-Object -FilePath <name>-launch.log`;
auditor stdout/stderr — в `<name>-audit.log`. Эти перенаправления не изменяют CLI
native process. Полные logs каждого ключевого запуска имеют общий абсолютный
префикс `C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict\.codex-runtime\logistics-20260906\`
и следующие подкаталоги: `stock-soak-20-logs`, `stock-soak-21-logs`,
`everon-probe-4-logs`, `everon-soak-22-logs`, `everon-client-22-logs`,
`everon-client-23-logs`, `rhs-probe-2-logs`, `rhs-client-2-logs`, `rhs-3-logs`,
`rhs-client-3-logs`. В каждом хранится полный console и все присутствовавшие
companion `.log`; соответствующий `summary.json` содержит SHA256.

Финальная проверка без Core fixture запускается сохранённым terminal script:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .codex-runtime/logistics-20260906/Run-FinalValidation.ps1 -Phase Static
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .codex-runtime/logistics-20260906/Run-FinalValidation.ps1 -Phase Workbench
git -c safe.directory=C:/Users/retar/IdeaProjects/Arma-Reforger-AI-Conflict diff --check
```

Script отклоняет оставленную Core fixture. Static phase вызывает каждый из 11
baseline auditors и два новых через `powershell.exe -NoProfile -ExecutionPolicy
Bypass -File tools/Test-<Name>.ps1`. Workbench phase вызывает именно
`ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent -gproj <root>/addon.gproj
-addonsDir <native,repo[,RHS]> -addons <graph> -logsDir <absolute-evidence>
-wbModule=ScriptEditor -run -validate` последовательно для Arland, Everon, RHS.
Exact раскрытые arguments, start/end и exit code записываются в
`final-static-results.json`/`final-workbench-results.json`. Полные Workbench logs
расположены в `final-workbench-arland/`, `final-workbench-everon/`,
`final-workbench-rhs/`; наличие `Game successfully created` проверяется отдельно
от exit code и SCRIPT errors.

Финальные static команды имеют общий prefix
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/`:

| Файл команды | Exit / verdict |
|---|---|
| `Test-Stage3Static.ps1` | 0 / PASS |
| `Test-Stage35Static.ps1` | 0 / PASS |
| `Test-Stage3StaticContracts.ps1` | 0 / PASS |
| `Test-Stage35RecoveryPolicy.ps1` | 0 / PASS |
| `Test-Stage4Static.ps1` | 0 / PASS |
| `Test-AICommanderModeStatic.ps1` | 0 / PASS |
| `Test-MapPointOrdersStatic.ps1` | 0 / PASS |
| `Test-RHSIntegrationStatic.ps1` | 0 / PASS |
| `Test-BaseBuildersStatic.ps1` | 0 / PASS |
| `Test-ConstructionStatic.ps1` | 0 / PASS |
| `Test-ConstructionContracts.ps1` | 0 / PASS |
| `Test-LogisticsStatic.ps1` | 0 / PASS |
| `Test-LogisticsContracts.ps1` | 0 / PASS, 37 cases |

Отдельный `git diff --check` записан в `final-diff-check.log`; generated logs,
resource databases и тестовые profiles не добавляются в Git.

При подготовке коммита `git diff --cached --check` дополнительно проверил новые
файлы и обнаружил `new blank line at EOF` в `AICF_LogisticsService.c`. Удалены
только завершающие пустые строки. После этого повторены `Test-LogisticsStatic.ps1`,
`Test-LogisticsContracts.ps1` и терминальные Workbench gates всех трёх проектов:
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File
.codex-runtime/logistics-20260906/Run-CommitValidation.ps1 -Phase Workbench`.
Evidence этих проверок имеет prefix `commit-`; исходные `final-*` logs сохранены.
Оба аудитора — PASS, exit 0 (37 logistics cases); Workbench Arland/Everon/RHS —
PASS, exit 0, `Game successfully created`, SCRIPT errors 0.

## Покрытие матрицы и ограничения

| Контракт ТЗ | Доказательство и предел |
|---|---|
| Один AI, empty vehicle, native depot/catalog/slot, стабильные ordinals и replacement | Реальные stock/RHS USAF readiness; stock WorkersPerDepot=2. Полная live матрица всех heavy prefab не завершена |
| Physical load → drive → unload, reuse и RETURN | Everon 4: два delivery одной парой; Everon 22: три delivery и actual return в original pool |
| Policy thresholds, deficits, donor reserve, strict config | Production Enforce 15/15; static/contracts. Все invalid CLI комбинации отдельными server launches — NOT RUN |
| Directed HQ BFS, cycles, multiple parents, isolation/revision | Production graph fixture 12/12; полевые capture/rebuild на каждой фазе — NOT RUN |
| Search budget, continuation, HQ/depth/age priority | Production planner fixture 6/6; отдельный искусственный graph большого размера — NOT RUN |
| Shared physical pools, incoming/outgoing/return reservations, TTL, generation/token, Stop twice | Production ledger fixture 18/18; гонка двух реальных машин за neutral SOURCE — NOT RUN |
| Fractional transfer, duplicate receipt, exact alias, credit/compensation failures | Production adapter на stock pools 7/7; resource rights/exact queues подтверждены. Полевая partial unload после изменения capacity — NOT RUN |
| Surviving cargo, death/fire loss и release | Stock 18/20: actual load, death, подтверждённый stock fire loss, однократный release; отсутствие timer refund |
| Fail-closed cleanup при unknown player position | RHS 2/Stock client probes: retained lease/cargo видимы. Это не clean terminal cleanup |
| Прямое вмешательство игрока в pilot/cargo и world-pool pressure | Guards и отрицательные contracts; полный targeted runtime этих сочетаний — NOT RUN |
| Idle retirement, offline/demolished/rebuilt depot, contested safety, cap fairness | Реализованы; все комбинации отдельными полевыми сценариями — NOT RUN |
| Client/JIP exact pair и actual cargo | RHS client 3: 29 observations, 12 с cargo 180, actual capacity 180, одна exact pair, цикл 0 → 180 → 0. Союзный JIP data evidence; enemy UI/visual отдельно |
| UI summary для союзника/противника и визуальный driver/vehicle/cargo | NOT RUN: требуется ручной verdict пользователя, GUI automation не выполнялась |

Эти ограничения не заменяются static PASS. Сохранённые native errors продолжают
делать полный runtime gate отрицательным. Статус `ACCEPTED` не присваивается.
