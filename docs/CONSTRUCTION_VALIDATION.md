# Автономное строительство: контракт и проверка

Реализация от 2026-09-05, исходный HEAD
`87bbdfe9b995276118bc44888d9ba3dd7e9d0a05`. Пользовательский
`AI_COMMANDER_CONSTRUCTION_PROMPT.md` сохранён без изменений. Статические,
Workbench, server, client/JIP и ручные проверки считаются отдельными gates.

Production domain реализован. Финальные static/contracts и Workbench проходят;
пять исходных Stage3/3.5 failures сохранены. Runtime подтверждает отдельные
оплаты, реальные worker completions и exact rollback, но полная приёмочная
matrix не закрыта. Подробные verdict и оставшиеся проверки приведены ниже.

После повторного сообщения о проблеме США изменены распределение кандидатов
и физическая проверка дочерних объёмов. Runtime evidence ниже относится к
предыдущим итерациям и не подтверждает последнюю геометрию. Текущий статус
зафиксирован в разделе «Подготовка 0.1.10»
[отчёта поиска площадок](CONSTRUCTION_SITE_SEARCH_VALIDATION.md).

## Поведение и владельцы

Порядок: `SMALL_BARRACKS → ARMORY → LIGHT_DEPOT → LARGE_BARRACKS → HEAVY_DEPOT`.
Commander выбирает тип только своей AI-controlled стороны; planner работает
на server/master в действующем match после `ROSTER_READY`. Player-commanded
сторона сохраняет прежнюю достройку layouts, но не получает автономных заказов.
Snapshot учитывает stock compositions и services в полном provider radius,
включая объекты карты/игроков. `TRAIT_SERVICE` исключает mortar emplacement,
который также имеет `SERVICE_ARMORY`. Для living area дополнительно проверяется
slot size: большие казармы покрывают потребность в малых, малые не закрывают
большие. Unknown living tier блокирует дублирование fail-closed. Stock base
хранит список services, отдельного взаимного запрета small/large не найдено.
Бонус стоимости respawn у stock base определяется наличием `BARRACKS` service
и не складывается. Заказ персонала и техники остаётся stock functionality.

Один pending/accepted order на базу. Валидный unfinished проект игрока блокирует
новый AI order. `token`, immutable base/provider IDs, faction и transform
проверяются перед commit; capture/placement/removal events отменяют pending
intent сразу. Rate limit от этого не сбрасывается. Stop снимает subscriptions
и освобождает intent/site reservations; принятый stock root остаётся в мире.
Принятый receipt не разрешает завершить перемещённый layout на непроверенном месте.

`AICF_ConstructionPlanner` владеет очередью; `AICF_ConstructionSiteSearch` —
геометрией; `AICF_StockConstructionAdapter` — spawn/stock primitives;
`AICF_EconomySystem` — admission/transaction/rollback. В MatchController добавлено
только lifecycle orchestration. Worker, его readiness, инструмент, animation,
generation и числовой вспомогательный slot остаются в BaseBuilder domain.

## Подтверждённый allowlist

Источник: `GetPlaceablePrefabs()`, `GetCompositionId()` и
`SCR_EditableEntityUIInfo.GetEntityAndChildrenBudgetCost()` на установленном
`1.8.0.13`; RHS `0.16.5150`. Префикс stock paths:
`PrefabsEditable/Auto/Compositions/Slotted/`; RHS:
`PrefabsEditable/Auto/ConflictRHS/`. GUID и полный path находятся в
`AICF_ContentProfile.GetConstructionPrefab` / RHS override.

| Профиль / faction | Тип | GUID | Path после префикса | CAMPAIGN / PROPS |
|---|---|---|---|---:|
| Stock US | small | DB7F01C68EACBEC1 | SlotFlatSmall/E_LivingArea_S_Conflict_US_01.et | 250 / 71 |
| Stock US | armory | 1BED8F81D2FA2137 | SlotFlatSmall/E_AmmoStorage_S_US_01.et | 325 / 41 |
| Stock US | light | A0FEA3E718F5B605 | SlotFlatSmall/E_VehicleMaintenance_S_Conflict_US_01.et | 150 / 35 |
| Stock US | large | 3C2CFF03FF61BE7A | SlotFlatLarge/E_LivingArea_L_Conflict_US_01.et | 475 / 310 |
| Stock US | heavy | 7AD06D9CF2672AC5 | SlotFlatMedium/E_VehicleMaintenance_M_Conflict_US_01.et | 300 / 100 |
| Stock USSR | small | BE3988DF6E3CD0B1 | SlotFlatSmall/E_LivingArea_S_Conflict_USSR_01.et | 250 / 62 |
| Stock USSR | armory | CD9004C6229D900B | SlotFlatSmall/E_AmmoStorage_S_USSR_01.et | 325 / 41 |
| Stock USSR | light | 5ACECF41C167567D | SlotFlatSmall/E_VehicleMaintenance_S_Conflict_USSR_01.et | 150 / 39 |
| Stock USSR | large | D44C687485600B72 | SlotFlatLarge/E_LivingArea_L_Conflict_USSR_01.et | 475 / 289 |
| Stock USSR | heavy | 6676FBB212B337D6 | SlotFlatMedium/E_VehicleMaintenance_M_Conflict_USSR_01.et | 300 / 102 |
| RHS_USAF | small | 28E24277E65162C9 | E_LivingArea_S_USMC_01.et | 275 / 84 |
| RHS_USAF | armory | BABA872EAB372FC6 | E_AmmoStorage_S_USMC_01.et | 325 / 41 |
| RHS_USAF | light | C4A1043FB488799F | E_VehicleMaintenance_S_USMC_01.et | 250 / 37 |
| RHS_USAF | large | 71B0DC7DA57FDC3F | E_LivingArea_L_USMC_01.et | 475 / 492 |
| RHS_USAF | heavy | 0915F1A0EF535ADA | E_VehicleMaintenance_M_USMC_01.et | 350 / 136 |
| RHS_AFRF | small | 6D49271024887478 | E_LivingArea_S_AFRF_01.et | 275 / 64 |
| RHS_AFRF | armory | FF11E24969EE3977 | E_AmmoStorage_S_AFRF_01.et | 325 / 41 |
| RHS_AFRF | light | 810A615876516F2E | E_VehicleMaintenance_S_AFRF_01.et | 250 / 39 |
| RHS_AFRF | large | 341BB91A67A6CA8E | E_LivingArea_L_AFRF_01.et | 475 / 466 |
| RHS_AFRF | heavy | 4CBE94C72D8A4C6B | E_VehicleMaintenance_M_AFRF_01.et | 350 / 136 |

Это наблюдённые цены, не constants оплаты. Каждый commit повторно читает
runtime metadata. У всех allowlist entries подтверждены faction key/label,
`TRAIT_SERVICE` и соответствующий service label; для двух казарм различается
slot size. RHS USMC filename обслуживает runtime `RHS_USAF`, не новую фракцию.
Нет stock fallback при отсутствующем RHS entry. В metadata дополнительно
обязателен реальный `SCR_ServicePointComponent` нужного `SCR_EServicePointType`
в полном tree. Услуга часто находится в linked child и отсутствует на root
до достройки; проверять обязательный root service было бы ошибкой.

У RHS allowlist root может сохранить унаследованную stock affiliation, хотя
registry label уже RHS. Только RHS content profile исправляет такую affiliation
на ожидаемую runtime faction до оплаты. Требуются точное совпадение prefab с
allowlist и прежняя stock faction либо отсутствие faction; чужая runtime faction
отклоняется. В Core нет RHS paths или такой коррекции.

## Геометрия и нагрузка

Уточнение поиска после пользовательского runtime от 14:42 описано в
[CONSTRUCTION_SITE_SEARCH_VALIDATION.md](CONSTRUCTION_SITE_SEARCH_VALIDATION.md).
Исторические runtime результаты ниже относятся к первоначальной реализации.

Полные mesh world bounds учитывают hierarchy, linked children, scale/orientation
и stock outline. `IGNORE_PREFAB` раскрывается повторно для дочерних источников:
stock helper не передаёт этот flag в рекурсивные вызовы. Preview имеет material;
без material engine не создаёт meshes. `Resource` удерживается до окончания
сбора bounds. Предварительно сжатые preview bounds из раннего catalog probe
не используются как доказательство полной геометрии.

Для depot фактические `SCR_EntitySpawnerSlotComponent` bounds входят в footprint,
а 20-метровый коридор выбирается отдельно из двух направлений каждого vehicle
slot. Footprint с запасом 1 метр и выбранные коридоры должны помещаться в
provider/world bounds. Terrain фундамента проверяется локальной сеткой 3 метра,
перепад ограничен 0.8 метра. У коридора ограничены перепад 2 метра и изменение
высоты между соседними samples (до 25% расстояния). Вода, physics obstacles,
layouts/services, road segments под зданием и перекрытие подходов отклоняют site.
Дорога под свободным выездом depot допустима. Проверки
консервативны: возможен `NO_SAFE_SITE` у тесной/неровной базы. Новая геометрия
повторно проверяется перед spawn и последним progress increment. AICF vehicle
reservations и construction AABB взаимно исключаются.

Worker endpoint проверяется на infantry navmesh. Streaming имеет до десяти
ожиданий; затем конечный поиск прямого либо двухсегментного пути к четырём
сторонам footprint. Runtime calibration `zero_ray=1` подтвердила, что native
`AIPathfindingComponent.RayTrace` сообщает свободный путь; прямой отказ не
заменяется предположением о достижимости. Отрезки обходят будущий footprint.

| Config / CLI | Default | Ограничение |
|---|---:|---|
| aicfConstructionDecisionMs | 60000 | 60000..3600000 |
| aicfConstructionCooldownMs | 60000 | 60000..3600000 |
| aicfConstructionDeadlineMs | 120000 | 1000..120000 |
| aicfConstructionCandidatesPerTick | 4 | 1..4, общий scheduler tick |
| aicfConstructionQueriesPerTick | 96 | 16..128, общий секундный window |
| aicfConstructionAttempts | 96 | 4..128 на order |
| aicfConstructionReserveGroups | 1 | 0..100 |
| aicfConstructionMetadataEntriesPerTick | 24 | 1..32 |
| aicfConstructionSliceMs | 8 | 1..16, cooperative batch budget |

Initial due times распределяются по минуте в стабильном порядке base IDs.
Round-robin cursor сохраняется; terrain/navmesh продолжаются с cursor на
следующем tick. Не более одного нового layout за scheduler tick. Geometric
metadata кэшируется по exact prefab; inventory сканируется только при решении
и commit, не на каждом секундном tick.
Создание preview meshes распределено по ticks, до 24 entries за batch.
Transient preview nodes не содержат gameplay components; Stop удаляет и
незаконченный preview. Сбор source tree ограничен 2048 entries / depth 32.
Candidate, terrain, navmesh и preview loops прекращают batch по временному
лимиту. Этот лимит проверяется между native calls и не может прервать один
долгий physics/resource call; измерение максимального tick остаётся отдельной
runtime проверкой, число queries само по себе не доказывает отсутствие пика.
Поиск использует детерминированную последовательность позиций в радиусе provider
и восемь orientations; отдельный cursor базы/типа продолжается после отказа.
`CONSTRUCTION_SEARCH_SUMMARY` выводит причины всех отклонённых кандидатов;
`CONSTRUCTION_SITE_REJECTED` — первый пример каждой причины с blocker prefab.

## Транзакция stock supplies

Закреплённые источники: `SCR_CampaignBuildingProviderComponent`,
`SCR_CampaignBuildingManagerComponent.OnEntityCoreBudgetUpdated`,
`SCR_EditableEntityCore.UpdateBudgetForEntity/QueueBudgetChange`,
`SCR_ResourceConsumer.RequestAvailability/RequestConsumtion`,
`SCR_ResourceEncapsulator` и `SCR_ResourceContainer` в Script Diff `1.8.0.13`.

Quote читает provider/master limits, чужие accumulated obligations и фактический
resource pool. `IsThereEnoughBudgetToSpawn` не используется: это мутирующий API.
Остаток после оплаты не ниже replacement cost из Stage4 config × reserve groups;
для source также не ниже `GetSourceReserveSupplies`. Уже оплаченные deployment
reservations повторно не вычитаются. Tickets не затрагиваются.

Reservation существует только внутри синхронного commit. Economy сохраняет
exact leaf containers вложенного resource pool (до 128, глубина до четырёх),
owner identities и provider props. DELETE-on-empty containers отклоняются до
debit: их удаление не позволяет гарантировать exact rollback. Stock manager
получает runtime `CAMPAIGN` cost единожды. В `1.8.0.13` этот building path не
применяет `GetBuyMultiplier`, а `RequestConsumtion` принимает уже заданное
значение; дополнительное умножение было бы неверным.

Поздний callback editor core продолжает global budget accounting, но не
повторяет supply/prop side effect для exact AI receipt. Баланс после stock
debit проверяется до принятия root. При частичном отказе Economy восстанавливает
только уменьшенные exact containers/props, затем adapter удаляет собственный
непринятый root; его stock refund подавлен. Обычный последующий демонтаж
принятого root сохраняет stock refund percentage. Owner identity проверяется
снова до rollback; возврат новому владельцу запрещён.

## Evidence и команды

Локальное evidence: `.codex-runtime/construction-20260905/` (ignored).
Каждый `*-launch.txt` содержит полный argv и `AICF_RUNTIME_MANIFEST_JSON` с
точным fresh profile под `C:\Users\retar\AppData\Local\AICF`. В profile сохраняются
полные `console.log`, `script.log` и остальные engine logs. Выборки events
используются только как индекс. Catalog evidence: `catalog-stock-launch.txt`,
`catalog-rhs-launch.txt`; оба завершились с native exit 0.

Baseline до правки — `before-*.txt`, результаты после — `after-*.txt`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-BaseBuildersStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-AICommanderModeStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage4Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage3Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage35Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage35RecoveryPolicy.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-RHSIntegrationStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-MapPointOrdersStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionContracts.ps1 -EvidenceRoot .codex-runtime/construction-20260905/contracts
```

Новые static/contracts: PASS, exit 0. Baseline сравнение: BaseBuilders,
AICommanderMode, Stage4, RecoveryPolicy, RHS и MapPointOrders — PASS, exit 0.
Stage3 — exit 1, сохранены `STAGE3_PROGRESS_EVIDENCE`,
`STAGE3_BOUNDED_PROTECTED_CLEARANCE`, `STAGE3_MARKER_STATE`.
Stage35 — exit 1, сохранены `STAGE35_MEANINGFUL_TASK_PROOF`,
`STAGE35_BOUNDED_PROTECTED_CLEARANCE`. Существующие проверки не ослаблены.

Workbench без runtime fixture:

| Вариант | Evidence directory | Verdict |
|---|---|---|
| Stock Arland | wb-delivery-arland | PASS, `Script validation successful`, native exit 0 |
| Stock Everon | wb-delivery-everon | PASS, `Script validation successful`, native exit 0 |
| Arland RHS | wb-delivery-rhs | PASS, `Script validation successful`, native exit 0 |

Команда каждого прогона — terminal invocation из `DEVELOPMENT.md`:

```powershell
& 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe' `
  -noThrow -wbsilent -gproj "$PWD\AIConflictArland\addon.gproj" `
  -addonsDir "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger\addons,$PWD" `
  -addons '9178E5822AFE48EA,B52C5F6AEDBF423E' `
  -logsDir "$PWD\.codex-runtime\construction-20260905\wb-delivery-arland" `
  -wbModule=ScriptEditor -run -validate | Out-Null
```

Everon заменяет project на `AIConflictEveron` и integration GUID на
`A4B2E62595F645A4`; RHS использует `AIConflictArlandRHS`, дополнительный
RHS addons root и graph
`9178E5822AFE48EA,B52C5F6AEDBF423E,1337C0DE5DABBEEF,BADC0DEDABBEDA5E,595F2BF2F44836FB,9F88011DA22B471C`.
Pipeline `Out-Null` необходим для ожидания GUI-subsystem executable из terminal;
одного redirect недостаточно. Exit сохраняется отдельно в `*-exit.txt`.

## Повторение runtime

Test-only fixture хранится в `tools/fixtures/AICF_ConstructionRuntimeProbe.c`.
Для runtime временно скопировать его в Core `Construction/`, выполнить
Workbench compile, запустить через canonical launcher и после остановки
удалить только эту копию. `Test-ConstructionStatic` намеренно отвергает fixture
в production tree. Fixture наполняет supplies и наблюдает production path;
бесплатного placement, instant completion и ослабления geometry gates нет.

```powershell
& .\tools\Start-AICFRuntime.ps1 -Role Server -Variant RHS `
  -RhsAddonsRoot 'C:\Users\retar\OneDrive\Документы\My Games\ArmaReforger\addons' `
  -AdditionalArguments @('-aicfConstructionProbe','1',
    '-aicfConstructionProbeRefill','1','-aicfConstructionProbeMs','600000',
    '-aicfRequirePlayerForResult','0') |
  Tee-Object '.codex-runtime/construction-20260905/runtime-rhs-3-launch.txt'
```

`-Variant Stock|Everon|RHS` выбирает world; `-AICommanderMode US|USSR|BOTH`
передаётся отдельным параметром launcher. Дополнительные test-only switches:
`aicfConstructionProbeType 0..4` меняет начальный cursor, `aicfConstructionProbeSupplies`
задаёт supply target, `aicfConstructionProbeFault partial_debit` вводит лишний
половинный debit внутри открытой transaction. Recovery остаётся production.
`CONSTRUCTION_DEFERRED_PROBE` через пять секунд повторно читает actual props и
supplies; отличие supplies нужно сопоставлять с concurrent economy events.

Клиент запускается только с точным `-ServerProfileRoot` из server manifest;
launcher проверяет fresh CLI, process и `ROSTER_READY`. Switch
`aicfConstructionClientProbe 1` включает test-only replication observations,
без управления GUI. Копия fixture и addon graph должны совпадать у server/client.
Для подключения без autodeploy использовалось
`-AdditionalArguments @('-aicfConstructionClientProbe','1')`.
RHS runs с `-autodeployFaction RHS_USAF` аварийно завершались, в том числе на
исходном HEAD. Сам marker связывает token; provider/layout/service
читаются из stock entities. Marker не является доказательством их корректной
репликации без соответствующих observations.

Ручные visual/animation/placement verdict остаются **NOT RUN**; GUI и screenshots
не используются. Runtime evidence и ограничения перечислены далее.

## Наблюдённые runtime результаты

Profiles ниже находятся под `C:\Users\retar\AppData\Local\AICF`; полный путь
конкретного `console.log` определяется manifest и каталогом `logs/logs_*`.
Ни один FAIL не переименован в PASS из-за успешных отдельных AICF events.

| Evidence / profile | Наблюдение | Gate |
|---|---|---|
| runtime-stock-9 / Server-20260905-124044-029 | До последних исправлений: четыре оплаченных completion (US small ×2, armory ×2), работа инструментом и online services | FAIL: pending orders не закрыты до остановки старой fixture; её Stop исправлен |
| runtime-rhs-3 / Server-RHS-20260905-132154-193 | US small: 275 supplies, 84 props; light depot: 250 supplies, 37 props. Оба достроены инструментом, service ONLINE. 596 ticks, max window 76 queries, max tick 204 ms; native exit 0 | FAIL: шесть RHS faction init SCRIPT(E) и два shutdown resupply SCRIPT(E); transaction/worker/Stop checks прошли |
| runtime-rhs-2 / Server-RHS-20260905-131707-686 + Client-RHS-20260905-132027-929 | Клиент подключился после roster; server crash до первого placement, native exit -1073741819 | FAIL: client/JIP |
| baseline-rhs-server / Server-RHS-20260905-133256-887 + Client-RHS-20260905-133321-516 | Изолированный исходный HEAD, без construction/fixture; native access violation при создании подключившегося игрока, exit -1073741819 | Сетевой blocker воспроизводится до изменений |
| runtime-stock-fault / Server-20260905-133618-648 | Введён half debit; supplies 1000 и props 58 полностью восстановлены. Затем VM exception при выборе удаляемого root строителем | FAIL; исправлено снятием exact failed root с очереди и отказом непринятому receipt |
| runtime-stock-fault-2 / Server-20260905-134110-855 | US mode, 181 ticks, max tick 43 ms, 68 queries/window. Нет заказов USSR и VM exception; нет safe site, fault не достигнут; native exit 0 | FAIL общего log gate: два shutdown resupply SCRIPT(E); rollback этим прогоном не доказан |
| runtime-stock-fault-3 / Server-20260905-134624-281 | BOTH, сначала type cursor 1; существующие armories пропущены. USSR light depot прошёл site/structural gates; fault ввёл 75 лишних supplies debit поверх stock cost 150. Полный rollback до 1000 supplies / 49 props, через 5 секунд тот же баланс и root_present=0. 300 ticks, max tick 87 ms, 76 queries/window; native exit 0 | PASS log audit, включая deferred props и отсутствие VM errors; это отказной сценарий, completion не запрашивался |

Everon `runtime-everon-4` / `Server-Everon-20260905-135229-298`: USSR mode,
cursor начинает с large, 539 ticks / 540260 ms, max tick 82 ms, max queries 16
за window. Large/heavy/small поиск дал `NO_SAFE_SITE`; placement не достигнут.
Native exit 0; общий log audit FAIL из-за двух shutdown resupply SCRIPT(E).
World load также содержит stock resource/material и navmesh tile errors;
они сохранены в отдельном полном индексе и не приписываются construction search.
Заказов US в этом прогоне нет.

Заключительный stock run `runtime-stock-heavy` /
`Server-20260905-140433-992`, BOTH, первый type 4, supply target 800:
heavy admission при `800 = cost 300 + reserve 500` разрешён, безопасный heavy
site не найден. Достроены small barracks USSR: supplies `807 → 557`, props
`49 → 111`. Через пять секунд props остаются 111; supplies 558 соответствуют
росту пула, а не повторному debit. 539 ticks, max tick 148 ms, 68 queries/window,
native exit 0. `-RequireCompletion` FAIL только по двум shutdown resupply
SCRIPT(E). Полный engine index также содержит network authorization diagnostic
при подключении. Клиент `Client-20260905-140859-770` без autodeploy подключился,
работал три минуты и завершился с exit 0; script CRC обеих сторон `16068907`.
Конкретный construction root клиентом не наблюдался: construction JIP не доказан.

Заключительный RHS run `runtime-rhs-4` /
`Server-RHS-20260905-141947-119` на последнем production коде:
small US `1000 → 725`, props `49 → 133`; light US `1400 → 1150`, props
`133 → 170`. Оба actual stock services ONLINE после worker/tool completion.
Deferred props совпадают; supplies за пять секунд возрастают на 2 и 1.
539 ticks, 540790 ms, max tick 130 ms, max queries/window 80, native exit 0.
`Test-ConstructionLog -RequireCompletion` — FAIL (`CONSTRUCTION_ENGINE_ERROR`:
шесть faction-init и два shutdown resupply SCRIPT(E)); с `-RequireAllTypes`
добавляется `CONSTRUCTION_MISSING_TYPES`. Ни один analyzer не ослаблялся.

RHS client `Client-RHS-20260905-142351-271` без autodeploy подключился, работал
три минуты и завершился с exit 0; script CRC `f85447ef` одинаков с server.
Fixture явно добавляла marker в custom `RplSave/RplLoad`, но без развёрнутого
персонажа этот root не попал в client stream: нет server save marker и client
observations. Это подтверждает connection, но не construction JIP. На client
shutdown зарегистрированы SCRIPT(E) о leaked RHS radial-menu instances,
ScriptInvoker и Color; client gate также не считается PASS.

После прогона временная копия fixture удалена из Core. Финальные Workbench
logs: `wb-delivery-*`; все три native exit 0. Итоговое сравнение static baseline
сохранено в `baseline-comparison.json`, compile summary — в
`workbench-delivery-summary.json`. Source HEAD не изменён, commit не создавался.

У `runtime-rhs-3` max tick включает синхронное создание gameplay composition;
параллельно на этом же компьютере выполнялся Workbench. Это диагностическое
измерение, а не чистый benchmark и не гарантия 8 ms. Списки всех engine errors
сохранены в `*-engine-errors.txt`; полные logs остаются источником verdict.

Rollback исправление сохраняет обычный player path: `UnregisterFailedConstruction`
удаляет только exact непринятый/неоплаченный AI receipt из `m_aPlaced`.
`IsTargetValid` и `HasUnfinishedWork` отклоняют такой receipt, пока stock registry
ещё содержит удаляемую composition. Owner дополнительно сохраняется и проверяется
перед `SCR_Faction.GetEntityFaction`. Это закрывает обнаруженный same-tick
callback/lifecycle случай, без переноса экономических обязанностей строителю.

## Незакрытая часть приёмочной матрицы

| Проверка | Текущий предел evidence |
|---|---|
| Все пять типов × обе стороны × карты, completion и штатный заказ personnel/vehicles | Полная matrix не закрыта. Есть construction completion для small/armory на раннем stock и small/light на RHS. Large/heavy допускаются registry/metadata и compile, поиск запускался; фактический completion отдельно не доказан |
| Exact `cost + reserve`, supply shortage, конкурентная player placement/reinforcement transaction | Heavy admission на точной границе 800 подтверждён; exact-boundary payment, управляемые shortage/concurrency fixtures — NOT RUN |
| Частичный debit / отложенный callback / orphan failed root | PASS runtime-stock-fault-3: exact rollback и deferred balance; первый дефект очереди исправлен |
| Несколько минут, policy BOTH/US/USSR, NO_SAFE_SITE и Stop pending orders | Проверяются полными соответствующими logs; это отдельный gate от completion |
| Prepared occupied base, narrow passage, depot exit, moving blocker непосредственно перед completion | Есть natural geometry отказы и completion wait на раннем stock; отдельные контролируемые fixtures каждого препятствия — NOT RUN |
| Потеря/повторный захват, удаление provider, player unfinished project, moving accepted layout, stale request replay | Guards и subscriptions проверены статически; отдельная управляемая lifecycle matrix — NOT RUN |
| Player-commanded сторона достраивает player layout | Существующий executor сохранён; отдельный construction regression runtime — NOT RUN |
| Allied/enemy client/JIP до и после completion, worker replication | Stock/RHS connection без autodeploy подтверждены; RHS autodeploy crash есть на baseline. Construction roots не попали в stream неподключённого к персонажу клиента; JIP matrix не закрыта |
| Визуальная анимация, входы/фундаменты, проезд техники и длительный soak | NOT RUN; требуется ручной visual verdict и отдельный soak |

Работа не объявляется `ACCEPTED`. Static PASS, Workbench PASS и успешный
отказной runtime не заменяют отсутствующие строки matrix.

## Изменённые файлы

Все paths относительно корня репозитория; пользовательский prompt не входит
в изменения реализации.

| Файл | Назначение |
|---|---|
| AIConflictCore/Scripts/Game/AIConflict/Config/AICF_ConstructionConfig.c | Cadence, quotas, deadline и reserve |
| AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_ConstructionOrder.c | Identity, receipt и diagnostics |
| AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_ConstructionMetadata.c | Allowlist metadata, полный prefab/outline tree и amortized bounds |
| AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_ConstructionSiteSearch.c | Terrain, physics, roads, navmesh и completion clearance |
| AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_ConstructionPlanner.c | Queue, coverage, fairness, revalidation и Stop |
| AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_StockConstructionAdapter.c | Unfinished stock placement, provider JIP и budget callback boundary |
| AIConflictCore/Scripts/Game/AIConflict/Economy/AICF_ConstructionEconomy.c | Admission, stock debit и exact rollback |
| AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_BaseBuilderService.c | Validated queue hook, rejected-root cleanup, checked work endpoint |
| AIConflictCore/Scripts/Game/AIConflict/Command/AICF_AICommander.c | Порядок типов и owned infantry pathfinding |
| AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c | Start/Update/Stop orchestration |
| AIConflictCore/Scripts/Game/AIConflict/Content/AICF_ContentProfile.c | Stock exact mappings |
| AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Content/AICF_RHSContentProfile.c | RHS exact mappings и bounded affiliation preparation |
| AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleSpawner.c | Взаимное исключение занятых construction/vehicle sites |
| tools/Test-ConstructionStatic.ps1 | Статические контракты и запрет production fixture |
| tools/Test-ConstructionLog.ps1 | Анализ полного остановленного server log |
| tools/Test-ConstructionContracts.ps1 | 13 synthetic log inputs и positive/negative static input |
| tools/fixtures/AICF_ConstructionCatalogProbe.c | Read-only catalog labels/budgets |
| tools/fixtures/AICF_ConstructionRuntimeProbe.c | Test-only supply/fault setup, deferred observations и client probe |
| README.md | Пользовательское поведение и ссылка на gates |
| docs/ARCHITECTURE.md | Владельцы нового domain |
| docs/TESTING.md | Команды и construction matrix |
| docs/CONSTRUCTION_VALIDATION.md | Mappings, evidence, verdict и ограничения |
| .gitignore | Исключение generated construction evidence |
