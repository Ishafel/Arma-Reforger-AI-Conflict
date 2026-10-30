# Раздельные квоты stock и AICF

## Статус

Пользователь в issue #78 разрешил исправить раздельный учёт `stock + AICF`.
Контракт ниже реализуется; на checkpoint30.10.2026,09:10 MSK подключение policy
и native floor написано как WIP, но integrated compile/runtime gates ещё не
пройдены. Первые compile FAIL сохранены и исправляются. Pure contracts и
отдельный counter control не означают успешную runtime partition. Старый
ordinary/MOVING2→4→6 FAIL сохранён.

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
Actual30cases/3unsafe подтверждают helper, native bootstrap ещё NOT RUN.
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

При передаче draft остаётся непокрытый producer: native
BaseCompartmentSlot.SpawnCharacterInCompartment создаёт персонажа до group
assignment и затем активирует/сажает его. Heterogeneous car+character basket
для этого пути ещё NOT IMPLEMENTED/NOT RUN. Generic MoveIn имеет отдельные
16actual/5unsafe/4pinned проверки, но не заменяет эту отсутствующую границу.
