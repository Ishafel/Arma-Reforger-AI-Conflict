# Раздельные квоты stock и AICF

## Статус

Последний human bridge: frozen255/1005SHA0/6closed0 и terminal Stock/RHS/WCS3/3
PASS на383payload26652760…90823. Native Catalogv11 подтвердил снятие прежней AI
записи exact player avatar и восстановление census, без actor/native-counter
writes. Body.MatchesIdentity остаётся master-strict; новый отдельный human proof
допускает только unowned STOCK/nonvehicle/no creator/no quarantine, exact
entity/world/Rplcomponent+ID, native player/PM/controller/main-ID agreement,
absent либо inactive+uncounted exact agent и совместимый полный census. Меняется
только binding/record; physical заменяет следующий ApplyCensus. World-scoped End
разрешён, successor и замена ledger/controller/epoch/record запрещены; monotonic
GetLastEpoch закрывает Begin→End ABA. Actual44/7+independent31 PASS.
Native purchase ещё FAIL на enumeration/selection, не на прежнем human-body
blocker. Новые observed53.1/41.3FPS в чистых pressure controls не исправляют их
functionalFAIL: без causal trace происхождение позднего extra-агента неизвестно.
Отдельный последующий current383 trace доказал actual super Refresh128/0→127/1,
BEGIN52→factory BOUND actor3CC1→128/0. В нём был учтённый разрешённый birth,
а не доказанный bypass/overshoot; какое прежнее stock body освободило unit,
не установлено. Этот вывод не переносится на предыдущие controls. Исходный
observerFAIL EXTRA_STOCK_IDENTITY_OR_BIRTH, resources28 и minFPS20.7 сохранены;
полные stopped logs и chronology — AI_QUOTA_STOCK_PRESSURE.md. Sets не вошли.

Пользователь в issue #78 разрешил исправить раздельный учёт `stock + AICF`.
Контракт ниже реализуется. После возобновления30.10.2026 frozen382production
`88B2F10A…DEB075` прошёл terminal Workbench Stock/RHS/ArlandWCSRHS3/3 PASS,
fullfatal0/source SHA drift0. Native RHS/Эверон/север на предыдущем380 прошли
300s наблюдения/ROSTER_READY/quota READY, но resources FAIL во всех3 и FPS Эверона
ниже30. Это не native pressure нового382 и не приёмка partition. Первые compile
FAIL сохранены. Старый ordinary/MOVING2→4→6 FAIL остаётся обязательным gate.

Полный frozen `resumed-fixed-v16`:247/247 PASS/987filesSHA0/collector0/6sessions
closed0. Поздние native fixtures/auditor supplements считаются отдельно.
Catalog native bootstrap впервые упал nativeAV до actual avatar/покупки;
причина UNKNOWN, dump/полные логи сохранены, проверка не объявлена PASS.

Следующие реальные native отказы локализованы отдельно: human requester является
valid server Rpl proxy/master0 (requester-only fix131/11, producer master сохранён);
compartment admission терял weak census row после собственного ReserveUnits
(strong local snapshot anchor28/8, прежние IsCurrent gates сохранены). Для этих
двух fixes завершён frozen249/Workbench3graphs. После randomizer fix новый
frozen250/994SHA0/6sessionsclosed0 и current383 Stock/RHS/ArlandWCSRHS3/3 PASS.
Stock pressure затем обнаружил legitimate native randomizer root→Rifleman
variant, отвергнутый strict prefab equality. Этот FAIL и reservation hold
сохранены. Новый immutable one-level набор читает pinned native metadata до
dispatch без RNG, сохраняет exact produced prefab для последующих проверок и
rollback; plain external factories сохраняют exact requested prefab. Actual28/8
и native root→variant BOUND подтверждены. Stock-pressure-v2 функционально прошёл:
STOCK128physical/reserved0, extraStock0 и четыре нормальных оплаченных join в
прежний managed US отряд до exact5. Это только stock-character/managed-recruit
proof, не universal partition или MOVING6. Resources28/minFPS22.2 остаются FAIL;
высокая измеренная стоимость RefreshCensus исследуется отдельно.

Команды воспроизведения stock-pressure и точные границы доказательства —
[AI_QUOTA_STOCK_PRESSURE.md](AI_QUOTA_STOCK_PRESSURE.md). Public runner сохраняет
весь raw payload, две fixture, compile receipt, frozen auditor и stopped logs;
сам public Run после публикации helper пока NOT RUN, прежний native использовал
те же385 source bytes и тот же functional auditor.

Human Catalog/FIFO воспроизводится public separate-role
`Invoke-AICFQuotaCatalogProbe.ps1`; команды и точный текущий scope —
[AI_QUOTA_CATALOG.md](AI_QUOTA_CATALOG.md). v17 на383production имеет
native0/0/fatal0/source0, но consumer funding readback0 и functionalFAIL;
generator SUFFICIENT/queue11 при consumer0/base0 не доказывает покупку.
Native full-basket negative→positive, GROUP/CHARACTER покупка и empty human
VEHICLE при полной character квоте пока NOT RUN/не доказаны. Resource и прочие
E/F также остаются strict FAIL; текущие source-only/CLI контракты их не снимают.

## Владелец и бюджеты

`AICF_AIWorldQuotaService` — authority/world-scoped владелец учёта и admission.
Он не назначает приказы, не создаёт стратегические slots, не платит за технику
и не меняет transport phase. Native hard limit должен покрывать сумму шести
ограниченных бюджетов; producer adapters обязаны защищать каждый бюджет до
создания/активации. Одно повышение общего лимита не реализует этот контракт.

| Bucket | Чьи units | Откуда ceiling |
|---|---|---|
| `STOCK` | Штатные персонажи, включая stock FIA | Исходный native limit, с однократным grandfather уже существующей stock occupancy |
| `COMBAT` | Managed roster, donor recruitment, logistics driver и его service car | Существующий `GetMaxManagedAgents`, обычно220, максимум256 |
| `BUILDERS` | Project worker на штатную base | Один immutable validated base snapshot, без прибавления каждый Update |
| `FLEET` | Native vehicle agent боевой capped lease | Существующий cap10/faction,20 для обеих сторон |
| `FIA_EXTRA` | Только project plan vehicles/crew/desant | Immutable bounded initial garrison/patrol plans |
| `STOCK_VEHICLES` | AI activation не принадлежащей live project lease техники | Отдельный исходный native limit, свой однократный grandfather |

Число128 — пример обычного исходного limit, не константа policy. Для исходных128,
COMBAT220, восьми bases, FLEET20 и FIA_EXTRA0 плановая сумма504. Исходные256
дают STOCK256 и STOCK_VEHICLES256; значение после увеличения не захватывается
заново. Общий arithmetic bound4096 — invalid config guard, а не разрешение
динамически расширять бюджет по фактическому заселению мира.

Original snapshot принадлежит exact `SCR_AIWorld`, а session epoch — текущему
controller. Capture выполняется до первого project `SetLimitOfActiveAIs`.
`AICF_AIQuotaStartup` читает original snapshot до `m_bStarted` и ждёт native
оплаченную catalog queue под старым лимитом до120s. Он не удаляет и не усыновляет
старые requests. Одноразовый retry1s хранит strong immutable config/content,
проверяет прежние controller/campaign/world/manager и снимается в `Cancel`/Stop.
Новая session уже активной world policy не ждёт её защищённую bound queue заново.
Actual30cases/3unsafe подтверждают helper; ordinary RHS bootstrap наблюдал READY,
но оплаченная очередь до включения policy отдельно native пока не доказана.
Restart controller в том же world не захватывает поднятый limit как stock и не
списывает существующие тела со счетов. После включения policy `Stop` или
неизвестный census не возвращают producers к свободному vanilla admission при
сохранившемся увеличенном native limit. Неинициализированный `Prepare` сам по
себе ещё не включает новую policy.

Ownership определяется exact group/slot/worker/plan/lease identity и generation,
а не faction, prefab или role-local именем. Stock US и managed US различаются.
Старые физические incarnations сохраняют учёт после session change; новое
разрешение требует актуального domain owner. Donor transfer не создаёт второго
unit. Logistics SERVICE lease не имеет military cap: driver+car резервируются
как два units COMBAT до создания/оплаты; reuse учитывает только новый delta.
Released fleet vehicle должен перейти в STOCK_VEHICLES по доказанному actual
ownership, без двойного учёта и без определения владельца по сидящему водителю.

## Census и разрешение на создание

Bounded census читает actual registered agents и native active/intended counters.
Полнота, уникальные identities, controlled entity/world, repeated readback и
согласованность native counters обязательны. Materialized, reserved/in-flight и
native intended публикуются отдельно. Временно inactive собственное тело не
считается автоматически свободной capacity. Доказанные DEAD/inactive/absence
освобождают учёт лишь после exact readback; UNKNOWN закрывает новое admission.

Каждый actual registered ALIVE/INCAP nonplayer character удерживает unit даже
при CountTowards=false. После полного ApplyCensus создаётся idempotent body
binding с нулевым добавлением occupancy. Временная потеря agent/activation не
освобождает living body: нужны exact DEAD+inactive либо actual world absence.
Initial stock grandfather включает только реальные тела, не dormant requests.
Actual37cases/5unsafe и peer PASS; на трёх native RHS наблюдениях Counted0 living
ветвь не встретилась, поэтому native proof этой ветви не заявляется. Prepolicy
journal metadata не равна native intended: partial actual1/intended1/pending9
проверены actual10cases/5unsafe; несуществующие9 не добавляются к grandfather.

Trip vehicle factory повторяет immutable assignment/intent/current slot и
generation, preset, exact reservation membership/expiration, selection pose,
base entity/faction и прежний ConflictAdapter.GetSpawnRejectionReason перед
созданием. FIA разрешение привязано также к конкретному prefab. Final pure
CommitNativeAttempt получает fresh nowMs от service после native проверок,
заново сверяет script phase/transition/slot group+generation/revisions и срок;
только затем расходует прежний attempt. Не меняет дедлайны и лимиты. Actual
58cases/21unsafe и независимые before FAIL сохранены; native service в этом
replay остаётся boundary adapter, полноценный runtime проверяется отдельно.

Full remaining authored basket резервируется до native enqueue. Permit имеет
exact world/session/owner/request/index/prefab и потребляется до native factory.
In-flight продолжает занимать место. Local factory result связывается до чужих
`OnAgentAdded`/native callbacks; nested callback не наследует ambient permission.
После native вызова повторяются identity guards. Различаются `VETO`, настоящий
`NATIVE_NULL`, `BOUND`, `ROLLED_BACK` и nonnull `QUARANTINED`: неизвестное тело
нельзя выдать за null и повторно создать. Чужое/adopted/player тело не удаляется.
Rollback отменённого собственного newborn требует повторной exact identity,
unassigned/nonplayer/authority проверки и подтверждённого исчезновения.

`SCR_AIGroup.SpawnGroupMember` — общий script seam queued и прямого group spawn,
но он возвращает bool, а не native entity. Active-policy adapter использует
version-locked pinned1.8.0.13 body с явной local factory boundary. Inverse
projection обязана совпасть со штатным телом вне именованных quota blocks.
Edit mode, client, другие worlds и неактивная policy сохраняют original `super`
chain. Native queue/dispatch не копируется и `IsExpandComplete` не подделывается.

## Catalog и активация техники

Catalog adapter резервирует CHARACTER1 либо полный GROUP N. Ограниченный
parent lock держит окно штатного counter до возврата внешнего PerformSpawn,
включая отмену/удаление producer во вложенном callback. Metadata exception
делегирует только проверенный Vehicle prefab; actual crew остаётся UNKNOWN,
native positive покупки при заполненном character bucket ещё NOT RUN.

Owned vehicle factory теперь сохраняет raw IEntity до Vehicle.Cast. Exact
producer guard удерживает исходный bucket до фактического собственного
BindReservedLeaseVehicle receipt; ссылка на произвольный новый lease не
разрешает transfer. Вложенный Reject, stale world/request/phase/deadline,
weak lease loss и NONNULL неверного типа не превращаются в native NULL.
Attempt commit переносится непосредственно к native вызову после admission.
Logistics заранее резервирует car+driver2COMBAT и передаёт оставшийся1 exact
registered group queue. Эти новые seams — текущая незавершённая интеграция;
targeted extracted PASS не означает native partition PASS.

Stock catalog имеет собственный direct character factory, обходящий group seam.

В native19:19:41 подтверждено: настоящий управляемый удалённым игроком avatar
на server имеет valid Rpl при IsMaster=false. Это источник намерения, а не
создаваемый AI. Поэтому requester identity допускает captured proxy role только
при обязательном present/valid/current Rpl и exact PlayerManager→controller→
controlled entity/player id/group/leader/slave/faction/world proof. Producer,
slots там, где master обязателен, и newborn AI сохраняют master gates. Старый
общий human-master veto был реальным дефектом, не исключением проверки.
Actual131cases/11unsafe/3pinned, independent peer0 и Stock/RHS/WCS3graphs PASS;
новый native catalog positive ещё требуется. Исходный serverAV до avatar и
два последующих opaque timeout controls сохранены отдельно, их причина не
переименована в найденный master-veto автоматически.
Full basket проверяется до первого тела и supply debit. FIFO permit связывается
с конкретным native request object и actual queue head; одинаковые prefab/user/
slot не заменяют request identity. Geometry callbacks и nested same-prefab calls
не могут взять permit родительской покупки. Quota-full результат требует
0новых тел/0дополнительного списания; частичный оплаченный отказ не считается PASS.
Штатные faction/rank/rally/group/supply проверки и native callbacks сохраняются.

`SCR_DefenderSpawnerComponent` не наследует catalog producer: его `SpawnUnit`
создаёт refill actor напрямую, затем снимает supplies и активирует AI. Для него
тоже требуется full remaining refill basket и exact local factory result до
оплаты. Native `HandleGroup` уменьшал бесплатный despawn credit после void
вызова независимо от результата; quota adapter допускает уменьшение только
после actual bound birth, удаления ровно первого queue item и повторной проверки
того же owner/counter. Quota VETO и настоящий native null сохраняют credit.
Остальные native timing/proximity/cleanup statements проверяются inverse
projection pinned body. Identity guard читает world/epoch/producer/queue/quote,
а `IsConfigured` остаётся admission guard: посторонняя quota quarantine не
является сменой владельца и не разрешает удалять валидного newborn.

Покупка человеком пустого `VEHICLE` не занимает AI character quota и не получает
новый AI_LIMIT отказ. Stock `GetRequestState` проверяет AI_LIMIT для CHARACTER и
GROUP. Unit техники возникает при доказанном AI activation: один vehicle agent
учитывается один раз независимо от числа пассажиров. Водитель managed группы в
неарендованной stock машине не превращает машину в FLEET.

Script entry seams исследуются отдельно; native `GetInVehicle` override
проверен и недоступен (`Multiple declaration`), production его не использует.
Mere action evaluation может только Peek. Actual dispatch держит exact permit
до activation либо доказанного abort. Veto никогда не очищает чужую reservation
через штатный failure cleanup действия, которое ещё не запускалось. Непокрытый
producer, autonomous/multiple-brain cost или неоднозначный census остаются
строгим blocker; универсальная совместимость неизвестных mods не заявляется.

### Создание персонажа внутри compartment

`BaseCompartmentSlot.SpawnCharacterInCompartment` создаёт actor до назначения
group. Его отдельный adapter не ослабляет обычный `AICF_AIQuotaVehicleEntry`:
до factory требуется typed `AICF_AIQuotaCompartmentCreation` с exact car/seat/
manager/Rpl, character prefab и текущим optional stock group. Фракция читается
из проверенного character prefab, а принадлежность машины — из lease registry;
фракция машины не заменяет ни одну из этих границ. Unknown metadata и попытка
выдать managed group за stock закрывают admission.

Car1 в его фактическом bucket и character1 в STOCK удерживаются до native
factory как две явные reservations. Уже учтённая машина не получает второй unit
для следующего пассажира. Prebirth custody защищает ещё пустую inactive машину
во время промежуточного census. Разрешение newborn действует только для exact
current creation/body до штатных `ActivateAI` и `GetInVehicle`. После успешного
входа отдельная проверка пустого group prefab/нового controller разрешает штатный
group bind; существующий header factory для GROUP N не меняется.

`m_Creation` в context — weak, а captured character body хранится отдельно:
strong cycle через creation guard отсутствует, и исчезновение weak creation
не освобождает pending entry. Source Stop возвращает только неиспользованные
reservations; уже выданный native вход удерживает car unit до exact settled seat
или positive absence. Rollback удаляет только подтверждённого собственного
нового unassigned nonplayer actor. При неоднозначности сохраняются тело/учёт и
quarantine, а не ложный `NATIVE_NULL` с повторным factory.

`Test-AIQuotaCompartmentProducerContracts.ps1` выполняет actual Context,
новые backend methods, producer и Ledger:28cases/8unsafe PASS в WinPS5.1.
Новый actual counterexample воспроизвёл потерю weak census row после
`ReserveUnits`→`RefreshCensus`. Backend теперь удерживает исходный snapshot
до повторной проверки `capturedCensus.IsCurrent()` и `use.IsCurrent()`;
равенство старого и нового `m_Census` не требуется и native gates не сняты.
External service/factory callbacks здесь boundary simulation; этот результат
не заменяет Workbench и native physical proof. Generic MoveIn16actual/5unsafe/
4pins — отдельный путь.

Native `native-async-v7` на exact382 source выполнил две настоящие factory,
два `GetInVehicle(true)`, последующую посадку двух разных здоровых actors в
точные места той же машины и неизменные EntityID/Rpl. BOUND до активации и
BODY после неё совпали после materialization scalar log arguments. Полный
gate **FAIL**: на первом tail poll после начала окна native census стал
UNKNOWN (counted-active32 против native-active31); такой же transient был до
первой factory. Native0/fatal0/resources28FAIL, полные логи сохранены.
`native-known-tail-v8` завершился с initial census UNKNOWN до reservation/
factory, поэтому его новый tail branch не проверен: factories/births/entries0.

Fixture-only observer ждёт первый достоверный census до reservation, выдаёт
каждую factory только один раз, ждёт exact-seat readback перед следующей
factory и требует15 непрерывных секунд known census/двух исходных healthy
occupants. UNKNOWN сбрасывает только это окно наблюдения; исходные300s,
native identity, health и quota gates не меняются. Здоровье/identity/seat
проверяются даже на UNKNOWN poll; физическая потеря не становится ожиданием.
`native-initial-wait-v9` завершён: Workbench0,40source/4actual observer
callbacks/36log cases/14настоящих WinPS5.1 CLI controls PASS, independent peer0.
Его native functional **12checks PASS**: две настоящие factory/две принятые
посадки, два exact STOCK character bindings и один STOCK_VEHICLES body, те же
EntityID/Rpl/health и15.068s/15polls непрерывного known census. Восемь ранних
observer resets не продлевали исходные300s; total fixture62.579s. Native0,
full sibling fatal0, raw382 source drift0; strict **FAIL:28resource errors**.
Это scoped Stock proof exact382 source с backend `AA82CE67`, а не новый
whole-source383/RHS/ordinary gate. Свой сервер штатно остановлен, CIM0.
Evidence: `.codex-runtime/issue78-finalization-20261030/compartment-producer/`.

## Проверка и доказательства

Обязательные результаты учитываются раздельно:

1. Actual/extracted math, owner, census, ledger, queue/factory tests: same-faction
   stock/managed, transfer, ABA/reentry, stale epoch/request/Rpl, partial census,
   full basket, no doublecount и bounded retirement.
2. Терминальный Workbench Stock/RHS/WCS и inverse projection pinned adapters.
   Совпадение pinned body само по себе не доказывает runtime mod compatibility.
3. Native pressure: stock ceiling и частично materialized queued group не
   занимают managed reserve; managed spawn при полном stock проходит, свой
   ceiling даёт WAIT до body/debit, освобождение допускает следующий запрос.
4. Реальный catalog positive/negative и late vehicle activation, включая empty
   human vehicle при полном stock character bucket, multiple occupants и
   отсутствие лишнего списания. Raw native/faction veto сохраняются.
5. Исходный MOVING2→4→6 с production background/default rosters/cap/deadlines,
   затем ordinary1800 обеих сторон и затронутые client/JIP gates на новом source.

Counter control30.10.2026 проверил14 новых пустых controllers с recordedAlive10,
Request10/Purge/ActivateAI(false): actual bodies0, group activation/countTowards0
и world active/intended0/0, включая следующий frame. Native0/fullfatal0,
resources28 FAIL. Это не доказательство частично materialized basket и не
основание вводить deactivation workaround. Необходимые полные native pressure
tests ещё NOT RUN. История/evidence — в [TESTING.md](TESTING.md) и ignored
`.codex-runtime/issue78-finalization-20261029/REPORT.md`.

Native-only persistence/resource blockers #78 сохраняются независимо от
успехов квот. `NOT RUN`/UNKNOWN/coverage FAIL не превращаются в PASS.

## Разбор Census UNKNOWN без изменения допуска

Test-only `AICF_AIQuotaCensusDecisionProbe.c` различает ранний busy veto,
native Capture/IsCurrent, BuildCounts, character custody/owner classification
и vehicle holds. Один настоящий вызов каждого метода сохраняет аргументы,
bool и out-значения; observer не вызывает повторные admission/Refresh и не
удерживает census/body/row через дополнительные сильные поля. Он пишет только
отрицательные решения: cached input identity и отдельно помеченные
AFTER_DECISION native counters. Они не заменяют исходный snapshot.

Опция public Soak runner `-QuotaCensusDecisionObserver` добавляет fixture в
отдельный stage и CLI `-aicfQuotaCensusDecisionProbe 1`. Бюджет384distinct records,
overflow/scope loss и ограниченный cached context первых16машин явно отмечены;
непросмотренный suffix не является доказательством полноты. Statics очищаются
в OnGameEnd. Default запуск не включает observer. Цель — разобрать реальный
долгий UNKNOWN при посадке в ordinary383; отказ сам по себе не разрешает
ослаблять census, классифицировать чужую группу или создавать unit без permit.

Текущий targeted контракт:32source/120actual wrappers/8frame/6edge,
48новых instance-wrapper cases/16hierarchy cases/4unsafe PASS. Четыре новые
обёртки вызывают ровно один исходный `IsCurrent` для Owner/Use/Occupant/Entity.
Отдельный свежий `AFTER_DECISION_NOT_ORIGINAL_INPUT` обход ограничен512nodes
и12samples, cadence1s. Cached quartet/rows и свежие счётчики помечены раздельно;
ни то ни другое не подменяет исходные локальные значения admission.
Native boundaries simulated; Workbench/native этого observer учитываются
отдельно. Явный `ref` у managed local frame — только избыточная запись:
локальные ссылки уже сильные по Enforce ARC, старый lifetime defect не заявлен.

Stock combined390 source-v7 прошёл terminal WB0/validation/full3log fatal0/
sourceSHA0. Native US600 на нём завершился native0, functionalFAIL boarding/
route. Свежий hierarchy snapshot показал316nodes, из них204в character subtrees,
без parent/world/duplicate/truncation ошибок. Это отдельное доказательство
переполнения прежнего полного256node обхода на таком дереве, а не атомарная
запись исходных locals. Исправление и новая игровая проверка учитываются
отдельно; V25 All261 не включает это позднее расширение observer. Новый V27
включает его и topology fix:262/262PASS, collector0/1021SHA0/6closed0;
production WB3graphs и combined Soak390/Multi388 отдельно PASS. Native нового
topology fix ещё NOT RUN; границы — [VEHICLE_CENSUS_TOPOLOGY.md](VEHICLE_CENSUS_TOPOLOGY.md).

В историческом draft checkpoint `5c87bae` compartment producer оставался
`NOT IMPLEMENTED/NOT RUN`. Его новая typed реализация описана выше; новый
native gate остаётся открытым до проверки текущего source.

### Точные аргументы решения Census

Полный stopped USSR1800 на topology V27 дал повторные UNKNOWN окна80.517с,
154.958с и незавершённое321.776с до остановки. Все280 Capture failures имели
`NATIVE_CENSUS_MISMATCH`, vehicle Classify failures0. Часто AFTER nativeActive
равнялся q11+q10, но были исключения: это не основание менять формулу допуска.
q10 уже оплачивается в BuildCounts как counted character; проблема не является
доказанным пропуском расхода квоты. Native budget implementation отсутствует
в pinned Script Diff, а прежние логи не содержали исходных локальных значений.

Пустой protected instance hook `AICF_ObserveCaptureDecision` получает результат
единственного actual IsComplete и его8 исходных аргументов. Сам Core ничего
не логирует и не меняет решение. Механическое удаление hook и замена локального
bool/returned обратно на прежний if восстанавливают исходный код точно после
нормализации EOL; счётчики движка, пределы, BuildCounts и dispatch не меняются.

Изолированная fixture `AICF_AIQuotaExactCaptureProbe.c` включается только
`-aicfQuotaCensusExactCaptureProbe 1`. Она сохраняет примитивы внутри hook, а
публикует `[AICF][QUOTA_CENSUS_EXACT]` после возврата единственного super.Capture.
Actual row.IsCurrent делегируется один раз; его свидетели имеют отдельный
`DYNAMIC_CAPTURE_CALLS` scope. Они могут включать вложенный посторонний вызов.
Поэтому dedicated q10 count/IDs собираются отдельно только из actual
snapshot.m_aRows после возврата: `CAPTURE_SNAPSHOT_CACHED_AFTER_RETURN`.
Это script fields; свежие life/LOD/Rpl и дополнительные native getters
отсутствуют, `life_lod_rpl=NOT_OBSERVED` не выдаётся за измерение.

Record budget384, sample budget8 и bound MAX_TOTAL_UNITS сопровождаются
явными overflow/неполнотой. Unique frame object и instance token не дают
устаревшему callback менять successor, в том числе после Stop/Start и ABA.
OnGameEnd отключает observer до явного нового lifecycle Start; последующий
Capture при cleanup только делегируется. Повторный Start активного lifecycle
не стирает boot evidence. Default запуск не подключает fixture.

Проверки exact observer и его compilation/native scope фиксируются отдельно
в TESTING.md; они не доказывают исправление q11/nativeActive инварианта.
Final actual-source contract:479assertions/21executed unsafe PASS в WinPS5.1
и pwsh; кандидат legacy Census4120/6 PASS, inverse EXACT. После Root apply
все3 профильных canonical contracts PASS. Full `r30`:265/265PASS/collector0/
1029SHA0/6actual sessions closed0. Production WB3graphs и mixed Soak393/
Multi391 отдельно compiled0/fullfatal0/source0. Native exact observer NOT RUN;
WCS bootstrap подключает прежние2readonly fixtures и не доказывает его работу.
Исходные full logs, повторные окна, inverse и отдельные development FAIL:
`.codex-runtime/issue78-finalization-20261030/census-exact-capture-diagnostic-v1/`
и `ordinary-topology-USSR-1800-v1/`.
