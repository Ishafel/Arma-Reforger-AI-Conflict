# Полные сценарии техники и диагностика групп

## Короткая коррекция manual staging до покупки

`AICF_VehicleManualStagingRecovery` вызывается AcquisitionFlow до отмены второго
собственного `SCR_AIMoveIndividuallyBehavior`, когда обе штатные попытки уже
исчерпаны, native action ещё выбран/исполняется и actor не двигался более0.5м
за существующий5s audit interval. Это узкий путь для единственного outlier при
остальных staged original участниках. Старый длинный relocation для manual
по-прежнему запрещён. Право новой короткой видимой коррекции следует из новых
вводных пользователя; прежняя blanket формулировка запрета устарела.

До и после поиска повторяются current trip/request/plan/reservation, assignment/
intent/base graph revision, preset/spawn generation, group/entity/Rpl, exact
selected/executed native action и полный исходный fit foot roster. Lease и
vehicle ещё отсутствуют, debit не выполнен. Разрешена одна actual3D коррекция
2–8м с перепадом<=1м и native projection<=0.5м через общий сухой свободный
connected-navmesh `InfantrySpawnPlacement.IsUsable`; projected destination ещё
раз проверяется перед Teleport. Используется прежний `MarkStagingMemberRelocated`
до асинхронной мутации. Попытки, deadline, waypoint/phase и health не меняются.
Покупка продолжает требовать последующее физическое all-member staging/hold.

После двух исчерпанных member attempts и единственного штатного waypoint reissue
manual approach завершается typed `SPAWN_STAGING_ACTIONS_EXHAUSTED`, если ни один
реальный waypoint не queued/current, собственные staging actions больше не
selected/executed, а физический rally ещё не собран. Plan-owned immutable original
roster/Rpl fence и текущие request/preset/generation/assignment/base graph/reservation
проверяются перед таким исходом. Новый retry не создаётся; обычный Controller
освобождает plan и восстанавливает infantry order. Root taskless oracle не менялся.
Остаточное native движение после `ACTION_OWNERSHIP_LOST` не объявляется живым task.

Новый targeted fixture `AICF_ManualStagingProbe.c` использует BehaviorProbe и
BehaviorSettingsProbe как scaffold, но запускается только с
`-aicfManualStagingProbe 1 -aicfVehicleBehaviorFaction USSR`, без
`aicfVehicleBehaviorProbe`. Собственный roster5 и test-only native movement
block проверяют две обычные попытки, stale negatives без мутаций, actual short
Teleport и поздний physical rally всех5 до покупки/дебета. Максимум180s;
movement control явно возвращается перед cleanup. Это controlled native proof,
не повтор исторического Soak и не normal war. Свежий native verdict следует
фиксировать отдельно; static contracts его не заменяют.
Опциональный `-aicfManualStagingExhaustionProbe 1` оставляет outlier в40м, где
коррекция<=8м не может довести его до25м rally. Первый exact waypoint снимается
в первом Maintain при настоящем reissue counter0. После двух штатных member
attempts fixture проверяет живой повторный waypoint и снимает его. Если native
уже снял повторный waypoint, временный test-only native waypoint доказывает veto
при queued task; он не становится plan waypoint и снимается перед удалением.
Receipt каждого случая одноразовый и не связывает native counter с собственным
счётчиком fixture. Затем проверяются настоящий ранний fallback, plan release,
infantry restore, неизменные deadline/health и отсутствие покупки. Исходный
exhaustion-v1 FAIL71/2 с таким ошибочным связыванием счётчиков сохранён отдельно;
он не превращается в PASS после исправления fixture.
Изолированный `m_VBState` не входит в штатные Root faction arrays. После Handoff
он проходит настоящий `ProcessPendingOrderRecovery` на configured reliability
cadence до трёх stable polls и исходного durability window. Fixture ждёт в пределах
первоначального handoff deadline и максимум30s, не сбрасывает pending/clock и
подтверждает exact current/queued infantry waypoint вместе с Root meaningful task.
Exhaustion-v2 FAIL12/1 был преждевременной проверкой `!HasPendingOrderRecovery`
через1s после успешного Handoff receipt; исходный FAIL сохранён отдельно.

## Разрешённая короткая коррекция native AI

Новые вводные пользователя от 28.10.2026 разрешают принудительную посадку на
точные места и высадку даже рядом с наблюдающим игроком. `AICF_VehicleShortRecovery`
проверяет authority, текущие group/spawn/preset/lease/entity/Rpl identities,
сознательного живого AI и отсутствие чужой reservation/occupant. Посадка
перемещает бойца не более чем на25м до actual compartment position, перепад
высоты не более5м; машина должна иметь скорость<=1м/с. Helper не перемещает
машину, не лечит и не воскрешает. Права на human possession не меняются.
`TryClearIncapacitatedSeat` отдельно снимает физическую связь только исходного
captured погибшего/бессознательного AI, повторяя current identities и direct/main
player veto. Native no-door ragdoll exit не меняет life state. Candidate на замену
назначается владельцем crew policy только после readback освобождённого места.

Preset boarding применяет не более одной такой коррекции на token после
собственного native GetIn FAILED/COMPLETED без посадки, исчезновения action либо
5с без spatial progress текущего action. При известном native failure нет
дополнительного ожидания12/20с. До correction снимается собственный action и
повторяются guards; после неё нужны прежние два physical settled readback polls.
Окончательный FAIL по исходному boarding/trip deadline сохраняется.

В DismountFlow доказанный hull ghost после полного logical egress допускает
короткую relocation сразу, а exact native GetOut FAILED — немедленный force-exit.
Player proximity/LOS и combat больше не запрещают эту высадку; сохраняются exact
AI/occupant, safe terrain/water, скорость, max displacement25м и отдельные finite
бюджеты3 logical +3 physical на phase. Native fallback без известного failure
по-прежнему проходит bounded normal escalation. Эти разрешения не дают cleanup
права удалить занятое людьми ТС. Контракт: `Test-VehicleShortRecoveryContracts.ps1`.

Новый native runtime этой коррекции требуется отдельно: прежние результаты
ниже относятся к прежним permissions и не доказывают обновлённое поведение.

`Invoke-AICFVehicleBehavior.ps1 -Cases transport,combat -Faction USSR` собирает
отдельный immutable stage с `AICF_VehicleBehaviorProbe.c` и settings probe.
Это `CONTROLLED_NATIVE_THREAT`: свой fresh roster, оплаченная машина, перемирие,
настоящие `SCR_AIThreatSystem.SetThreatValues/Update` и Kill исходного водителя.
Для transport используются5 бойцов UAZ; combat —3 UAZ PKM или5 US Humvee.
Этот scaffold не проверяет оплаченный найм первого обычного отряда или войну.

Oracle читает actual compartments до/после threat, весь ожидаемый десант вне
машины, исходный экипаж Combat на своих местах до Kill, отсутствие нового
живого Pilot15с продолжающейся угрозы и физическую замену исходным выжившим
после неё. Затем нужны exact survivors в том же lease/entity, одна покупка,
живой исходный roster без замены IDs, погибший водитель без воскрешения,
отсутствие прироста здоровья у отдельно слегка повреждённого выжившего и
реальные10м движения той же машины по новому приказу дальше300м.
В transport выполняется второй пеший эпизод: новая native угроза сохраняется
при выдаче дальнего приказа. Требуются actual physical reboard/movement той же
машины и `HasTransportCombatOverride` для exact нового intent при продолжающемся
`HasLocalCombat`; receipt — `DISTANT_ORDER_COMBAT_OVERRIDE`. В combat эта ветвь
не применяется: его маршрут проверяется после окончания угрозы.
Повреждение fixture только уменьшает здоровье; лечение и raw teleport отсутствуют.
Settings probe выполняется один раз. Общий timeout600с, Stop снимает observer,
Finish завершает собственный coordinator/roster и вызывает RequestClose.
Проверки этой fixture считаются новым PASS только по полному остановленному
native log; resource/JIP blockers остаются отдельными gates.

BehaviorFlow разрешает captured места через `AICF_VehiclePreset.CollectCaptured`:
raw native compartments сопоставляются с каждым сохранённым ключом ровно один
раз, в порядке recipe. Временный `IsCompartmentAccessible=false`, который штатный
AI использует при `AllocateCompartment`, больше не означает исчезновения места.
Отсутствующие/дублированные ключи, чужая машина/иерархия/manager и новое доступное
место вне recipe отклоняются. Недоступные места, исключённые исходным каталогом,
не добавляются в recipe. Каталожный Collect и permission checks перед native GetIn
остаются прежними; resolver не меняет accessibility или reservations.

При actual mismatch bounded diagnostics пишут raw compartments всех обнаруженных
managers (до256 hierarchy nodes/64 seat rows), owner IDs/Rpl, flags/occupants,
captured keys и текущие native allocations; alias `GetAllocatedCompartments`
читается без изменений. Native fixture отдельно опускает accessibility одного
реального места, подтверждает потерю его старым Collect и сохранение exact keys
новым resolver, затем немедленно восстанавливает исходный flag и проверяет
неизменного occupant: `NATIVE_ALLOCATION_NOT_TOPOLOGY_CHANGE`.
Это causal проверка контракта. Она не доказывает причину исторического organic
Soak-US-v1 отказа20:07:41: raw flags в его точке отказа не были записаны.
Offline `Test-VehicleCapturedTopologyContracts.ps1` исполняет actual resolver
через native-object adapter и проверяет отрицательные structural cases.

Cleanup отдельно корректирует уже логически вышедшего `INCAPACITATED`, который
остался внутри расширенных bounds машины. При приёме release owner захватывает
текущий native roster в `AICF_VehicleCleanupRecoveryRoster`: group/slot generation,
actor/agent/entity/Rpl и vehicle/lease generations. Перед перемещением нужны те
же текущие group/member/control identities; diagnostic `ActorHistory` не даёт прав.
Коррекция допускает только detached actor без compartment pointer/transitions,
медленную машину<=0.25м/с, свободную сухую площадку и displacement<=25м. Поза
меняется без лечения, воскрешения, изменения life state или выхода из другого ТС.
Присутствие наблюдающего игрока разрешено; human possession, чужие occupants,
player transitions и неизвестный safety scan сохраняют veto. Используются прежние
общие3 exact-recovery attempts, cadence и terminal deadline, без дополнительного
бюджета. После перемещения прежние fresh scans и5с стабильного clearance нужны
для release/delete.

Опциональный `-aicfCleanupIncapProbe 1` в behavior fixture продолжает успешный
сценарий отдельным controlled fault: настоящий Stop/BeginStopLease создаёт job,
штатный helper высаживает survivors; затем fixture переносит одного исходного
бойца не более25м внутрь hull и применяет native `ForceUnconsciousness(0)`.
Следующий native readback обязан доказать INCAP, отсутствие logical связи и hull
blocker до вызова Cleanup.UpdateFleet. Затем нужны поздний physical clear,
неизменные life/health/resilience, attempts1..3/неизменный deadline и полный
release после стабильной проверки. Это setup дефекта, а не normal-war evidence.
Native `native-incap-v3-diagnostic` завершён:30behavior+23settings targetedPASS,
native0/fullstopped, actual physical clear~5.15м/health unchanged/fullrelease;
strict resourcesFAIL. Версии и evidence сведены в [TESTING.md](TESTING.md). Контракт
`Test-CleanupIncapacitatedHullContracts.ps1` проверяет384 комбинации actual
production predicate и authority/identity/budget/native-proof guards.

`Invoke-AICFVehicleScenarios.ps1 -Cases repeat5 -Faction US` проверяет две
поездки native US Humvee на той же оплаченной машине; `USSR` — default с УАЗом. US доступен только
для repeat5. Модель выбирается из собственного FACTION catalog по реальной
топологии five-seat M1025 M2HB и положительной стоимости. Фракция неизменна;
order/recovery/observer/Update/Stop используют один actual owner. Analyzer
проверяет exact faction, model, один operation/lease/машину/покупку и те же пять
actor IDs. US не совпадает с префиксом USSR.

Для нового контракта сохранённого транспорта используется явный opt-in:
`Invoke-AICFVehicleScenarios.ps1 -EvidenceRoot <новый каталог> -Cases repeat5
-Faction US -RetainedBehavior` (аналогично `USSR`). Runner допускает только
isolated non-combat repeat5, передаёт native
`-aicfVehicleScenarioRetainedBehavior 1` и auditor `-RetainedBehavior`.
Marker `VEHICLE_SCENARIO_POLICY version=4 policy=RETAINED_TRANSPORT` обязателен
для новой repeat fixture (v2 сохраняется как отдельный historical evidence):
новую ветку нельзя принять по старым логам или без трёх явных opt-in.
Исторический standalone auditor без `-RetainedBehavior` сохраняет
`HANDOFF→COMPLETE`, новую машину и вторую оплату; это отдельный прежний контракт,
не oracle для retained transport. Текущий runner default `CURRENT_RETAINED`
сам явно выбирает retained repeat5; `-RetainedBehavior` остаётся явным alias
для изолированного цикла и обязателен при `-ProtectionScope HIERARCHY`.

Retained branch явно выбирает `TRANSPORT/returnDistance=300`: initial exact5,
30s mounted wait, настоящий route1, `TACTICAL_DEPLOYMENT` и физический выход
всех пяти вне hull с текущим native infantry waypoint к исходной цели.
Все пять исходных actor IDs должны дойти <=35m. Новый настоящий point intent
дальше300m вызывает physical exact-seat reboard на тот же trip/lease/entity/Rpl
и route2 с теми же проверками движения и полного пешего прибытия. Повторного
admission нет; production purchase должен быть ровно один, как и native spawn.
`RETAINED_IDENTITY/SEAT/DISMOUNT/RETURN/FINAL` receipts связывают обе поездки;
runtime marker не заменяет native transitions, actual occupancy и positions.
Остаются исходные20min, >=100m/5moving polls, recovery0 и <=35m; fixture не
телепортирует, не меняет production clocks/budgets и не объявляет пешее arrival
по action state. Используется прежний изолированный transport truce/damage
protection: это targeted route evidence, не доказательство normal war.
`Test-RetainedVehicleScenarioContracts.ps1` проверяет исходные permission
guards и отрицательные policy/identity/geometry/debit/legacy controls.
Final native обеих фракций выполнен:26checks/0fail каждый, WB0/native0/stopped,
corrected targeted audit0, strict resources28FAIL; подробные границы ниже.

Отдельный стенд повторного цикла допускает `-RetainedBehavior
-ProtectionScope HIERARCHY` только для isolated non-combat repeat5. Это
`CONTROLLED_RETAINED_CYCLE_ONLY`: не normal war и не исправление повреждения.
До первого движения fixture один раз отключает damage handling на actual root
и всех unique child DamageManager из native `GetAllHitZonesInHierarchy` у пяти
исходных бойцов и оплаченного ТС. Повторный сбор дерева проверяет ownership,
полноту и enabled_after=0; receipts пяти тел и машины должны предшествовать
первому boarding. Дальше выполняется только read-only проверка текущего дерева;
нового отключения, `SetHealth`, лечения или воскрешения нет. Wheel damage,
который обходит эту защиту, по-прежнему приводит к FAIL по native mobility.
`ROOT_ONLY` остаётся явным прежним default этой отдельной проверки.

Retained root-only v1 US и USSR завершились native0, но оба functional FAIL:
первый route/all5<=35m и same-asset exact reboard прошли; на втором маршруте
native сообщил `VEHICLE_MOVEMENT_DESTROYED`, movement_damage=1/wheels=0.
Root handling был отключён, child zones оставались enabled21/21 US и44/44 USSR;
физическая причина UNKNOWN. Тот же trip/entity/Rpl/gens сохранился до typed
fallback и реального восстановления пешего приказа. Старое fixture имя
`RETAINED_IDENTITY_LOST` включало terminal veto и не доказывало смену identity;
новый diagnostic сначала пишет точную terminal reason. Полные v1 FAIL сохранены
в `.codex-runtime/vehicle-behavior-20261028/retained-repeat-<faction>-v1`,
разбор — `retained-repeat-v1-readonly-review`. Первые HIERARCHY-v1 дали реальные
bounded reboardtimeouts; здоровая ходовая и отключённые child zones подтверждены,
точный CanSeat veto не измерен. Эти FAIL сохранены отдельно.

Последние `retained-repeat-US-current-v2`/`USSR-current-v2`: WB0/native0/stopped,
каждый26checks/0fail, два полных реальных маршрута, repeated exact5slots,
all5foot≤35м, same trip/lease/entity/Rpl/roster и одна покупка. Это controlled
truce/HIERARCHY proof. Original frozen audit1 ошибочно считал VSFinish Stop
после strictFINAL боевым отказом. Отдельный corrected auditorD68BB88B...BE7AC74
даёт targeted0 обоих, source/native drift0;87controls отклоняют ранний,
чужой/malformed/nonSTOP/fatal failure. Только exact matching Stop state+latch
после строгого physical FINAL доDONE допускается как fixture cleanup. Full
fatal/resource/stopped gates сохраняются; strict resources28FAIL обоих.
Evidence: `retained-current-v2-corrected-audit/report.json` рядом с root REPORT.

Текущий runner по умолчанию использует `-ReceiptPolicy CURRENT_RETAINED`.
`uaz5/ural5/armed3/solo1/retarget5` явно передают native
`-aicfVehicleScenarioCurrentBehavior 1` и marker version3/CURRENT_RETAINED;
repeat5 использует retained marker version4. Single cases завершают один
настоящий маршрут, physical deployment и полный пеший подход, сохранив исходный
trip/lease/ТС и одну оплату. Для version3 обязательны independent original roster
receipts до boarding и соответствие каждого native seat/body этому roster.
`retarget5` сохраняет две настоящие цели и неизменность последней до arrival.
Phase TACTICAL при `LOCAL_THREAT` не является arrival: тот же leg ждёт обычного
resume и настоящего `DESTINATION_REACHED` с исходной геометрией. Общий20min
не меняется, terminal по-прежнему FAIL. Test fixture не очищает threat и не
подавляет production behavior policy.

У controlled non-combat current fixtures все пары из actual native
`GetFactionsList` получают truce до boarding, включая FIA, если она присутствует.
Для каждой пары пишется двусторонний native readback, затем полный summary.
Неизвестная faction, неполная/hostile pair или последующее изменение actual
friendship/count дают FAIL; после setup выполняется только read-only проверка.
Version4 требует эти receipts, version2 historical logs читаются отдельно без
подмены старого результата. `CONTROLLED_RETAINED_CYCLE_ONLY` не подтверждает
normal war. Если actual `LOCAL_THREAT` возникает при сохранённой полной truce,
его источник остаётся UNKNOWN без отдельного native evidence.

Standalone `Test-VehicleScenarioLog.ps1` по-прежнему имеет historical default
`LEGACY_COMPLETE`: он требует прежние COMPLETE/new asset/debits. Current logs
нельзя принять как historical и наоборот; current singles анализируются только
с `-ReceiptPolicy CURRENT_RETAINED`, repeat — с явным `-RetainedBehavior`.
Current runner отклоняет обычный native `LEGACY_COMPLETE` до запуска, чтобы не
ждать невозможного COMPLETE20min. Исключение — отдельный
`-DismountBudgetProbe`, который выбирает явно controlled legacy dismount branch.
Current single cases и новый budget adapter требуют свежих terminal WB/runtime
gates; source/log contracts их не заменяют.

## Повреждение ходовой и завершённый fallback

`Invoke-AICFTransportDamageFallback.ps1 -EvidenceRoot <новый каталог> -ServerPort 2580`
выполняет отдельный `CONTROLLED_FAULT_INJECTION` с native колёсными hitzones.
После exact5, оплаченной машины, ожидания30s и настоящего движения >=15m/3polls
fixture разрушает четыре колеса через `SCR_WheelHitZone.SetHealthScaled(0)` и
штатный `UpdateVehicleState`. Проверяются фактические movement damage1/CanMove0,
typed `VEHICLE_MOVEMENT_DESTROYED` до crew recovery, выход и пешее прибытие всех
исходных пяти к неизменной цели <=35m. Те же trip/intent должны сохраняться
не менее30s без новой покупки; затем отдельный explicit admission branch даёт
новый operation/vehicle/debit. Это проверка внутреннего admission boundary,
а не повторного UI Apply: одинаковый Apply остаётся идемпотентным.

`AICF_TransportDamageFallbackProbe.c` включается только своим runner поверх
нейтрального false-by-default hook в ScenarioProbe. Production counter/deadline
не заполняются fixture. Общий20min срок и RequestClose прежние. Строгий анализатор
читает полные остановленные sibling logs, исходные IDs, реальные transitions и
покупки; native resource errors остаются отдельным FAIL. Targeted functional
PASS не означает normal arrival или чистый общий runtime.

Продолжение issue #78. Прежний TransitProbe проверяет начало движения и высадку
после retarget к машине. Он не подтверждает доезд до исходной дальней цели.
Для этого служит `AICF_VehicleScenarioProbe.c` и отдельный строгий анализатор.

## Повторяемый запуск

Только Windows terminal, без клиента, GUI, screenshots и установленных изменений:

```powershell
& tools/Invoke-AICFVehicleScenarios.ps1 `
  -EvidenceRoot .codex-runtime/vehicle-scenarios-<уникальное-имя>
```

Runner готовит isolated stage из текущего HEAD + production diff/untracked,
добавляет только этот fixture, компилирует Workbench и последовательно запускает
canonical `Start-AICFRuntime.ps1 -Role Server`. Каждый case получает свежий profile.
CLI/manifests, native exits, полные logs, auditor verdict и hashes stage сохраняются
в EvidenceRoot. `-Cases uaz5,ural5` выбирает поднабор; `-ServerPort` задаёт также
реальный native `-addr`. `-StageRoot` подключает заранее подготовленный stage,
`-SkipCompile` допускается лишь после его успешной компиляции. Два independent
runner можно запустить с разными портами и EvidenceRoot; общий stage неизменяем.

Scope — Stock Arland, recipient USSR. Изолированный транспортный прогон включает
`-AICommanderMode USSR`, HOLD_FIRE, запрет урона только тестовому составу/машине
и взаимную дружбу всех faction в тестовом мире. Это только fixture, отсутствующий
в production; проверки подтверждают поездку без боевого противодействия.
`-Combat -AICommanderMode BOTH` оставляет обычные отношения, урон и огонь для
разбора боевого прерывания поездки. Возле гарнизона native
VehicleCombatActivity может сам потребовать высадку; это отдельный исход,
не PASS доезда. Для поездок fixture выбирает реальные road vertices в350..900m,
исключает близость других баз и загружает native navmesh tile самой цели.
Нет координат/имён баз в Core или подготовленного teleport маршрута.

| Case | Состав и проверяемое действие |
|---|---|
| `uaz5` | Пятиместный закрытый UAZ,5 бойцов |
| `ural5` | Transport Ural,5 назначенных бойцов, остальные места свободны |
| `armed3` | UAZ PKM, обязательные Pilot/Turret + passenger,3 бойца |
| `solo1` | Невооружённый UAZ, только один водитель |
| `retarget5` | Та же поездка, после80m новый удалённый target, без подмены машины |
| `repeat5` | Две полные поездки теми же actor IDs/group/trip/lease/ТС; одна оплата и повторная посадка |

Каждый case физически садится на exact seats, ждёт30s у исходной близкой цели,
получает дальний приказ и едет до его dismount zone. В обычном case target/revision
не меняются до прибытия; `retarget5` разрешает ровно одно явное дальнее изменение.
В current policy после штатного TACTICAL_DEPLOYMENT бойцы должны пешком дойти к
той же точке: все живы, исходные actor IDs, farthest<=35m. Vehicle arrival требует
distance<=dismount radius+10m, displacement>=100m, >=5 polls с реальной скоростью,
без mobility relocation. Native timeout20min завершает case FAIL, а не зависает.
Нехватка достижимого test target после30s — отдельный TARGET_PRECONDITION FAIL.

`Test-VehicleScenarioLog.ps1 -LogPath ... -LauncherPath ... -NativeExitPath ...
-Scenario uaz5` проверяет полный остановленный log и соседние logs. Он требует
реальные occupant/arrival/foot/completion receipts, порядок, неизменность цели,
group/member diagnostics и native0. Старый «проехал25m» или пеший выход через
fallback не пройдут. Resource errors не превращаются в script PASS общего gate:
они остаются отдельным native baseline/blocker. Его contracts содержат45 controls.

Штатный OpenGate может временно высадить пассажира: fixture требует живой exact
цепочки PerformAction/OpenGate + GetOut + GetIn на исходное место, тот же group
activity, vehicle/member/seat identity. Возврат ограничен60s и должен завершиться
до начала высадки у цели. Самовольный выход остаётся FAIL; зависание у ворот,
смена actor/seat или незакрытая пара STARTED/RETURNED не проходят анализатор.
Production actions/таймеры ради теста не изменяются. Завершённый trip сохраняется
fixture до пешего прибытия: registry вправе убрать его раньше.
Прежняя legacy fixture допускала удаление машины в cleanup во время пешего подхода
и для `repeat5` запрашивала новый admission. Этот oracle сохраняется только для
historical logs. Current policy удерживает ту же машину, а новый дальний intent
вызывает обычный same-asset reboard. Повтор прежней цели идемпотентен; исходные
actor IDs сохраняются на обоих этапах.

Для ручного preset `AICF_TransportNativeGate` наблюдает доказанную OpenGate цепочку
водителя/стрелка до обычной mandatory crew recovery. Наблюдение хранится в movement
state, сверяет operation/group/member/vehicle EntityID/RplId, route и assignment
revisions, exact AI owner и seat. Оно не создаёт actions, не ставит reservations,
не меняет waypoint, motion/progress timestamps или абсолютный срок trip. До60s
возвращается WAIT/NATIVE_OPEN_GATE; exact settled return возобновляет обычный transit.
Deadline/identity loss возвращает typed fallback; состояние очищается при Reset
и AbortOwnedActions. Events: TRANSPORT_NATIVE_GATE_STARTED/RETURNED/REJECTED.
Случайный выход без native paired chain этого ожидания не получает.

После анализа архивируй и убери собственные stages/profiles. Не меняй stage,
пока жив хоть один использующий его сервер/клиент. Пользовательскую библиотеку
и profiles Frontend не удаляй. Перенос workflow на RHS/WCS и проверка всех
машин не подтверждаются одним Stock case; нужны отдельные future fixtures.

Для регрессии раздельных dismount budgets сначала выполняется обычный
`-Cases ural5`, затем отдельный новый stage:

```powershell
& tools/Invoke-AICFVehicleScenarios.ps1 -Cases ural5 -DismountBudgetProbe `
  -EvidenceRoot .codex-runtime/dismount-budget-<уникальное-имя> -ServerPort 2540
```

Opt-in добавляет только в stage `AICF_DismountOperationBudgetProbe.c` и CLI
`-aicfVehicleScenarioDismountBudget 1` и явный
`-aicfVehicleScenarioDismountBudgetAdapter 1`; production не содержит тестовых веток.
Это controlled fault injection, не обычная игровая последовательность. Fixture
`CONTROLLED_DISMOUNT_BRANCH_ONLY` сначала получает настоящий production
`START_TACTICAL_DEPLOYMENT/DESTINATION_REACHED` после реального маршрута и,
проверив exact bound probe/identities/TRANSIT/route_generation/actual distance,
возвращает только typed `StartDismount` обычному controller. Adapter не меняет
phase, health, budgets, deadlines или положение сущностей. Это позволяет
проверить прежний dismount owner на новой Core; normal manual arrival теперь
сохраняет машину в tactical state и сам в эту ветку не входит. Затем fixture
ускоряет расписание трёх существующих `super.TryForceExactManagedExit` и делит
exact native actors по batches1/1/3. Каждый положительный FORCE_BATCH обязан
иметь реальный charge, вызов original member recovery и принятый native exit;
прямая запись счётчиков не используется. После действительного полного logical
egress fixture переносит одного исходного AI внутрь hull, сохраняя native flags
снаружи. Production затем должен выполнить свою normal physical relocation.
Auditor требует все три native receipts, logical0/transitions0/hull blocker,
физический выход ghost через production relocation, прежние deadlines и всю
обычную цепочку COMPLETE→FOOT_ARRIVAL. Отсутствие реального charge/egress/recovery
остаётся coverage FAIL; одного успешного обычного `ural5` недостаточно.
В clearance telemetry допускаются только exit<=3, physical<=3 и aggregate=sum<=6
на фазу с явно объявленными максимумами3/3/6. Прежний общий предел3 не заявляется
сохранённым. Обычный `ural5` запускается отдельно без этой инъекции.

## Снимки групп

`AICF_GroupTrace` — read-only authority observer у VehicleCoordinator. По
умолчанию пишет группы с ручным vehicle preset раз в5s, включая пеший остаток
после trip. Он не создаёт actions, reservations, waypoint, callbacks или RPC.
Cursor хранит только stable key и счётчики; Stop его очищает.

- `-aicfGroupTrace 1` включает также обычные READY группы.
- `-aicfGroupTraceFaction USSR` и `-aicfGroupTraceSlot 0` ограничивают выбор.
- `-aicfGroupTraceIntervalMs 5000` задаёт cadence, clamp1000..60000ms.

`GROUP_TRACE`: run/time, faction/numeric slot/stable key, callsign, group/entity
и generation, assignment/intent/vehicle revisions, role/posture/decision authority,
target, живой leader/position, alive/inside, native current и owned waypoint,
queue count/position, native group action, trip phase/reason/age/deadline,
route generation/endpoint, recovery counts, vehicle position/speed/distances.
Controller origin группы не используется как позиция отряда.

`MEMBER_TRACE`: numeric position/actor ID, world coordinates/life, actual linked
vehicle/seat и transition flags; выбранное и реально исполняемое native action;
expected seat/occupant/reservation/lock, tracked boarding state/age. Данные tokens
читаются только после их создания. Перед boarding rejection пишется внеочередной
полный snapshot с trigger=reason. Sequence связывает членов с group snapshot;
group generation и entity различают старый состав и замену.

`PHYSICAL_MEMBER_TRACE` отдельно перечисляет всех фактических `group.GetAgents`,
включая платно нанятых сверх исходного deployment size. `physical_index` — лишь
порядок native массива; `roster_position` — найденная текущая позиция0..MAX_GROUP_SIZE-1
или-1, не постоянный ID. Пишутся parent/control/replication identity, расстояние
к target/waypoint и его completion radius, selected/executed action, group activity
с native waypoint/current proof и фактический DefendBehavior waypoint. Этот observer
не меняет policy и помогает отличить отставших бойцов от native cover formation.

`NATIVE_VEHICLE_PATH_TRACE/POINT` дополнительно сохраняют raw activation/count/
LOD/permanent LOD/max LOD фактических `driver_agent` и associated `movement_agent`,
native record/pilot handlers, `persistent_handbrake` и выбранное/исполняемое
group activity. `car_component_exact` означает только прочитанное соответствие
car component → associated agent → vehicle; глобальная уникальность не проверяется
(`world_vehicle_uniqueness_proven=0`). Missing agent/brake дают явный UNKNOWN.
Все поля читаются на прежней cadence и под прежними current trip/lease/route/
driver fences, с прежней повторной проверкой после native path getter. Если
нет current native route, activation по этой строке также не доказана.
Observer не включает AI, не меняет LOD/тормоз, actions, clocks или progress.
Расширение добавлено после ordinary USv11, где active/LOD после повторной
посадки не были записаны; старому run эти новые наблюдения не приписываются.

Для разбора сначала найди faction+numeric slot+group generation, затем operation.
Сопоставь transition reason и target revision с native group action, selected и
executed member actions. Например token может остаться EVALUATED после удаления
из utility, пока другой SCR_AIGetInVehicle уже FAILED; одна проверка token state
этого не показывает. VehicleCombatActivity + GetOutVehicle далеко от target
объясняют боевой выход; это отличается от arrival и от deadline fallback.

`NATIVE_VEHICLE_ACTION_SELECTED` пишет actual action/type/state, related activity,
priority/level, group/member/vehicle identity и координаты при native выборе
GetIn/GetOut и WrapBehaviorOutsideOfVehicle. `NATIVE_GET_OUT_MESSAGE` сохраняет
источник сообщения, activity/waypoint и priority. Все оригинальные callbacks
вызываются ровно один раз; observer не завершает и не создаёт actions.
Priority54 с PerformAction/OpenGate отличается от VehicleCombatActivity125.
`NATIVE_GROUP_MOVE_FAILED` сохраняет штатный EMoveError, связанную машину,
waypoint flag, координаты отказа и живого лидера. Original OnMoveFailed вызывается
ровно один раз. Native ProcessFailedMovementResult при некоторых ошибках выдаёт
SCR_AIGetOutActivity с PLAYER priority: это объясняет выход далеко от цели даже
при HOLD_FIRE/truce. ROAD_REACHABLE означает найденный road endpoint, а не гарантию
успеха последующего engine pathfinding. Такие случаи остаются FAIL поездки.

Preset boarding теперь повторяет застрявший/осиротевший action до2 раз на том же
member/seat, после12s без прогресса (20s для ещё owned nonterminal action), без
продления общего deadline. Повтор запрещён для linked/transitioning бойцов,
чужой reservation/occupant, stale identity или locked seat. Старый token снимается
owner-safe, replacement получает свежий fence. `VEHICLE_PRESET_SEAT_RETRY`
пишет причину, попытку, прошлое ownership/state и неизменный deadline; teleport0.
Общий budget рассчитывается один раз:25s × max(3, assigned seats), ограничен
absolute trip deadline; retries не продлевают срок. Все места сначала
резервируются, затем actions ставятся последовательно:
соседние бойцы не конкурируют за узкий вход. После двух settled polls завершается
только собственный GetIn на проверенном exact occupant/seat; Fail для сидящих
не вызывается. Намеренно выбранный состав из одного водителя после высадки
продолжает приказ к точке; правило отступления единственного выжившего относится
только к потерявшему остальных отряду с desired size>1.

Свежие verdict и ограничения — [TESTING.md](TESTING.md), полные evidence вне Git.
`Invoke-AICFTransportDamageFallback.ps1 -BoardingNavDomainProbe` включает тот же
read-only staging/boarding observer в isolated damage stage. CLI/fixture opt-in
и source manifest обязательны; это позволяет заснять путь второго подхода,
которого нет в vehicle-only GroupTrace. В damage-v4 первичная остановка лидера
предшествовала project cancel, но нужный foot path не был снят: native solver
или конфликт formation этим логом не доказаны. Сроки/retries не расширялись.

При планировании acquisition срок подхода теперь учитывает самого далёкого
живого участника до staging: запас60s + расстояние/2m/s, минимум configured
timeout, динамическая часть не более10min и всегда в пределах absolute trip
deadline. Срок фиксируется один раз, reservation не создаётся до проверки
живого roster. `VEHICLE_SPAWN_APPROACH_BUDGET` пишет основание расчёта.

Обычная высадка допускает до3 повторных animated exact-exit с интервалом5s
до исходного normal/terminal/trip deadline. Повтор затрагивает только точного
authoritative AI occupant без входа/выхода/смены места; identity перепроверяется
после прерывания его очереди. Общая очередь группы повторно не сбрасывается.
Ни сроки, ни hidden recovery/combat/player fences этими повторами не ослабляются.
