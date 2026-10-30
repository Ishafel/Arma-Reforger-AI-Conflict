# Поведение транспорта и боевой машины

Дополнение пользователя к #78 от 28.10.2026 разрешает короткую принудительную
посадку и высадку AI, в том числе при видимом/близком игроке. Прежнее требование
полностью скрытой коррекции для этих операций заменено этим разрешением.
Воскрешение, лечение и дальние переносы запрещены. Human possession, чужие
occupants, authority и identity остаются обязательными проверками.

Пресет schema2 хранит `behavior` (`TRANSPORT`/`COMBAT`) и `returnDistance`.
Пользователь выбрал N=300м; редактор допускает целое значение50..1000м.
Schema1 читается с TRANSPORT/300. Изменение поведения или N меняет assignment;
Save/Load сами не приобретают машину. Настройки проверяются серверным Decode.

В retained deployment раз в10s `AICF_VehicleCombatTrace` записывает
`VEHICLE_COMBAT_READBACK/CLUSTER/MEMBER`: фактический результат `HasLocalCombat`
из текущего Tick, current native action/behavior, свежесть и расстояние
cluster/target, target entity/life state, threat measure без injury factor и
consciousness живых участников. Trace не повторяет принятие решения, не меняет
actions/health/clocks и ограничен `MAX_SEATS`. Эти readbacks нужны, чтобы
отличать продолжающийся бой от устаревшей угрозы; наличие `LOCAL_THREAT` само по
себе ещё не доказывает корректную причину долгой остановки маршрута.

## Владельцы и цикл

`AICF_VehicleBehaviorFlow` возвращает typed outcomes; только
`AICF_TransportTripController` переводит TRANSIT↔TACTICAL_DEPLOYMENT, снимает
route waypoint, временно восстанавливает infantry order через Handoff/OrderPlanner
и вновь подключает vehicle utility. Lease, vehicle identity и debit сохраняются.
Автоматические trips без ручного пресета сохраняют прежний одноразовый цикл.

Временное пустое место экипажа во время уже захваченной native цепочки
`OpenGate → exact-seat return` само по себе не означает потерю экипажа.
`AICF_TransportNativeGate.OwnsTemporaryCrewExit` подтверждает текущий token,
исходный assigned actor/seat, entity/Rpl/control/group, сознание, физическую
связь только с той же машиной/местом и текущее собственное native действие.
Лишь такой proof позволяет BehaviorFlow продолжить TRANSIT до штатного gate
observer. `LOCAL_THREAT` сохраняет приоритет; dead/unconscious/foreign/stale,
expired или отсутствующий token не дают исключения. Исходное окно60s, общий
deadline и бюджеты не продлеваются, actions/health/poses не меняются.
Короткий native handoff после успешного `Perform` подтверждается отдельным
completion receipt: exact native invoker фиксирует только первое завершение.
Если utility ещё хранит именно этот `Perform:COMPLETED/remove1`, допускается
только уже стоящий в той же utility queue exact живой несuspended `Return`
того же activity. `EVALUATED` у текущего завершённого Perform не даёт исключения.
Оба completion listeners снимаются в `Stop`; повторные callbacks не обновляют
время receipt. Диагностический invoker observer лишь читает actual production
`OwnsTemporaryCrewExit`, не управляет outcome. Событие
`TRANSPORT_NATIVE_GATE_PERFORM_COMPLETED` добавляет `perform_completed_receipt`,
`perform_completed_at_ms`, `perform_listener_attached` к обычному gate payload.
`Test-NativeGateCrewExitContracts.ps1` исполняет actual extracted readonly proof
и unsafe mutants, включая receipt/queue/lifecycle; отдельный
`Test-NativeGateCompletedHandoffProbeContracts.ps1` проверяет fixture и строгий
лог-аудитор boundary. Native CrewReturn и paired G2/G3 field runtime остаются
отдельными gates. Пара G2→G3 на raw342, отличающаяся только Gate class,
проверила настоящее завершение native Perform: прежний `Owns` вернул0,
новый вернул1 после production completion receipt. Затем AFTER подтвердил
возврат исходного Pilot в то же место и native14,3248м/3polls без изменения
здоровья, entity/Rpl, deadline и прежнего recovery budget. Оба stopped native
exit0, source drift0, функциональные field/boundary и full fatal gates PASS;
resource gate остаётся FAIL28. В v4 первого regular-cadence отказавшего
predicate нет в журнале: доказан и исправлен класс реального native
`COMPLETED/remove1` handoff, а тождество причины v4 остаётся inference.
Полные v4 FAIL, промежуточные диагностические PASS и первый CLI-аудитор FAIL
сохранены раздельно в evidence `native-gate-crew-exit/REPORT.md`.
Последний вызван dot-source перезаписью входных путей; после save/restore
путей добавлены реальные отдельные PS5.1 CLI проверки, а остановленные логи
перепроверены новой frozen версией без повторного native/изменения raw logs.
Исходный
`crew-final-current-v4` сохранён как FAIL: actual native Pilot OpenGate был
ошибочно принят за `CREW_UNAVAILABLE`, затем `TACTICAL_BEGIN_REJECTED`.

TRANSPORT при местной угрозе высаживает весь живой отряд. После окончания боя
возвращает выживших в ту же машину; новый приказ с перемещением дальше N также
может потребовать возврат. После высадки по прибытии пеший приказ сохраняется
до нового дальнего intent. Порог N решает необходимость повторной посадки
пешего отряда; он не расширяет прежнюю зону mounted destination wait и не
останавливает уже посаженную машину с незавершённым маршрутом. Погибший/недееспособный водитель
заменяется свободным живым бойцом в текущей trip-копии рассадки.

Перед высадкой водителя TRANSPORT `AICF_VehicleParking` включает штатный
`CarControllerComponent.SetPersistentHandBrake(true)` и ждёт actual speed≤1m/s.
Слабый exact lease/car/Rpl/generation token переживает пеший эпизод и повторно
проверяет original occupants, human control/possession/target transition и чужую
группу непосредственно перед записью. Рядом стоящий игрок не блокирует парковку.
Ранее включённый native handbrake не присваивается проектом. Только собственная
запись снимается на той же машине после actual settled native Pilot при resume
либо identity-safe Stop cleanup. Другой пользователь/машина дают отказ без записи.
Парковка не меняет pose/velocity/health, не переносит машину и не продлевает сроки.
При human takeover запись запрещена, поэтому token не снимает тормоз за игрока.
Автоматический native сброс при входе human **NOT RUN**; его нельзя обещать по
Script Diff. В stock есть `SCR_ToggleHandbrakeAction`, переключающий persistent
flag и тормоза simulation с wakeup physics; наличие/исполнение этого действия
при human takeover на каждой модели отдельно не проверялось. Это сохранённая
граница доказательства, а не разрешение обходить human fence.
Для native AI отдельно проверен preexisting true перед route start без parking
token: штатный Pilot снял flag и проехал30.8516m/5velocity polls; project parking
writes отсутствовали. Это controlled native proof, не доказательство входа human.

COMBAT высаживает только десант. Исправный driver/turret остаётся в машине,
vehicle и пехота сохраняют общую группу и текущий приказ. Native combat policy
не допускает преждевременной замены экипажа во время местного боя; после его
окончания flow выбирает выживших, отдавая Pilot приоритет. Отсутствие стрелка
само по себе не означает потерю подвижности. Когда живого способного водителя
нет или машина физически не может ехать, controller возвращает пеший порядок.

`AICF_VehicleBehaviorPolicy` хранит weak identity-bound bindings и ограничивает
только соответствующие native GetIn/GetOut/crew-replacement решения. Местный
бой определяется по текущей native threat/target информации в отдельном радиусе
300м; этот радиус независим от N. Неизвестная локализация актуальной угрозы
обрабатывается консервативно. Stop/phase exit снимает bindings и owned actions.

Во время пешего эпизода delivery clock явно приостановлен. Это отдельное
состояние владения техникой, а не наблюдавшийся movement progress. При выходе
из TRANSIT прежний MovementState сбрасывается его существующим owner; возврат
создаёт новый bounded route epoch той же машины. Высадка и повторная посадка
имеют отдельные конечные сроки; повторный Tick не продлевает их.

Identity проверяется для всех захваченных numeric roster positions, независимо
от текущих effective seats. Новый живой боец на прежней позиции водителя после
переназначения требует новой incarnation. Проснувшийся original survivor получает
первое свободное Cargo в детерминированном порядке; crew/turret для этого не
используются. Если места нет, весь отряд возвращается к пешему порядку. Проверка
повторяется во время посадки без нового deadline. Перед Resume, после отмены
owned actions, перечитывается actual current group: каждый живой conscious
боец должен быть тем же captured actor, иметь ровно одно effective место и
фактически занимать его без перехода. Смена здоровья этим механизмом не выполняется.

Для текущего counted infantry repair сохранён его владелец: tactical flow не
снимает waypoint до terminal receipt проверки приказа. Повторный Handoff не
перезапускает уже актуальную verification и её stable clock. Это сохраняет
строгий attempted/confirmed/pending учёт вместо потери pending при reboard.

## Короткая коррекция

`AICF_VehicleShortRecovery` проверяет exact actor/group/seat/lease/generations,
живого conscious AI, отсутствие human/foreign occupancy и реальную дистанцию
до места не более25м, перепад высоты не более5м, скорость машины не более1м/с.
Caller сначала прекращает собственный конфликтующий action. Native force
посадка не считается доказанной до actual compartment readback.

При повторной посадке исходный живой AI может фактически занять место другого
исходного бойца той же машины. `AICF_ReboardSeatConflictScope` фиксирует обоих
actors, места, entity/componentRpl, lease/preset/group/trip, assignment/intent,
transition count, BehaviorState и исходный deadline. Только такой конфликт
получает короткую высадку; расходуется прежний общий short budget именно
занявшего чужое место бойца, максимум3. Native enter/exit/switch и живой
собственный GetIn в освобождаемое место ждут readback в том же окне. Дальний
боец и его собственный native GetIn не прерываются.

Scope хранит первоначальный `IsReservedBy` конфликтующего бойца. После
native Interrupt, пока captured actor ещё занимает wrong seat, освобождается
только доказанно его reservation. Перед записью и после callback повторяются
owner/health/identity/mapping/phase, отсутствие animations и live wrong-seat
GetIn; затем повторяется предел25м перед native GetOut. Этот порядок
соответствует stock GetIn completion: occupant продолжает защищать место.
После выхода pending receipt подтверждает actual readback; существующий
empty-only fallback не разрешает очистить чужое бронирование. Unknown/foreign
reservation не очищается, новое правильное бронирование Pilot сохраняется.
Замена pending
receipt во время native callback отменяет старое наблюдение и сохраняет
successor. `CancelEntries` снимает ссылку, но accepted exit уже не оставляет
старое own бронирование. Исправление не создаёт окно/retry. Новая реальная
угроза может штатно отменить episode; следующий episode создаёт сам flow.
Native TryExit/TrySeat/incapacitated exit журналируют только строки identity,
захваченные до native вызова: удаление actor/seat в callback не вызывает
повторных native getters из сообщения журнала.
Нативные GetIn messages подавляются только у текущего TRANSPORT reboard с
актуальными assignment/intent revisions; собственные exact-seat actions flow
выдаются напрямую. Foreign/human/stale/expired guards остаются fail-closed.
`Test-ReboardOwnSeatConflictContracts.ps1` исполняет реальные predicates,
recovery/exit, pending-owner ветку и unsafe mutants. Отдельный
`Test-ReboardOwnSeatProbeContracts.ps1` проверяет controlled fixture/лог-аудитор;
native результаты и сохранённые FAIL находятся в evidence
`own-seat-recovery/REPORT.md` и не заменяются этими adapters.
Native BEFOREv4 воспроизвёл actual own-Cargo в Pilot и настоящий отказ старого
Flow `TACTICAL_SEAT_OCCUPIED`. Первый AFTERv4 выполнил выход, но сохранил
непринадлежащую Pilot reservation пустого места: последовали исходный125s
timeout и6 SCRIPT E от meaningful-task deadline; полный FAIL сохранён.
Новый AFTERv6 подтвердил captured owner Cargo, пустое место до очистки,
actual unreserved после неё, возврат тех же5 в назначенные места и native
13,1481м/5moving polls. Health/entity/Rpl/mapping/deadline сохранены, общий
short budget Cargo2 из3 (выход+возврат), mobility0, одна машина/покупка50.
Native0/source0/functional и full fatal gates PASS; resourcesFAIL28 остаётся.
Это отдельный current-after proof: native BEFOREv4 и AFTERv6 имеют разные
общие Scope/State hashes. Подготовленная статическая v6 пара отличается только
Flow, но native BEFOREv6 не запускался. Original ordinary v11 reservation
owner не был залогирован; тождество этой дополнительной причины не заявляется.
Последующий actual-extracted lifecycle тест доказал отдельный gap: прежний
`CancelEntries` между accepted exit и empty reservation readback терял receipt,
и следующая посадка Pilot блокировалась. Atomic pre-exit исправление и
133 actual cases/9 executed unsafe mutants закрывают этот replay; прежние
219 full checks и AFTERv6 относятся к состоянию до atomic delta. Свежие
непрерывный и real LOCAL_THREAT interruption native gates закрыты на одной
замороженной343 production +fixture сборке: native0/source0/fullfatal0,
functionalPASS, resourcesFAIL28 в каждом. Standard вернул exact5 и подтвердил
11,147м/4moving polls при short2/3 и unchanged125s deadline277965. Interrupted
сначала подтвердил actual `COMBAT_REBOARD_CANCELLED` с pending1→0, combat1,
distantOrder0 и unreserved Pilot; первый core deadline208731 не менялся.
Следующий штатный episode создал core deadline211731, сохранённый до exact5
и13,5366м/8moving polls. Это новые normal core episodes, а не продление helper.
Оба сохранили исходные actor/entity/Rpl/health, одну машину и покупку50;
интеграция с quota/anti-cycle и полные ordinary gates учитываются отдельно.
Ignored wrapper после второго native stop неверно передал `-Interrupt`
аудитору через array splat; ошибка сохранена. Hashtable binding исправлен,
тот же frozen аудитор проверил те же stopped logs в WindowsPS5.1 без повторного
runtime. Текущий контракт содержит133 actual/9 unsafe, fixture17/log48/CLI6.

`AICF_OverturnRecovery` допускает короткое выправление здоровой перевёрнутой
машины в текущем manual TRANSIT: authoritative exact lease/componentRpl,
действующий native Move/waypoint/положительный Pilot handler и весь исходный
живой conscious AI roster в устойчивых местах этой машины. Подтверждённые
native Gate/crew/actor owners, human/foreign/stale scope, повреждение, огонь,
потеря бойца и завершившийся deadline оставляют прежний fallback.

Captured assignment/lease/route token даёт максимум3s для затухания физики;
окно не возобновляется на retry и не продлевает trip clocks. Перенос разрешён
только при скорости≤1м/с и angular speed≤0.5, с прежними двумя mobility attempts,
поиском4..15м, дополнительной границей фактического3D переноса≤15м для overturn,
terrain/live-character/pressure/player/occupancy fences. Scope и
скорость повторно проверяются перед transform. Отсутствие safe candidate не
создаёт route progress; исчерпанный budget/window возвращает fallback. Нет
лечения, ремонта, воскрешения или дальней перевозки. Token удаляется при Stop,
успехе, обычном upright Tick; несовпавшая incarnation не получает новое окно.

Контракт исполняет весь извлечённый helper:56 positive/negative cases. Native
`overturn-native-v3`: WB0/native0/full fatal0,11 checks PASS; локальная controlled
pose fault95°/1.4м → actual upright correction3D8.09129м/oldbudget1 из2 → native
drive29.4957м, original5/samecar/health/deadline1189069 неизменны. Это controlled
проверка, не завершённый ordinary/Multi soak. Raw resource errors сохранены FAIL.

При повторной посадке progress actor означает приближение к exact seat минимум
на0.25м в3D относительно последней credited позиции. Обе actor positions
сравниваются с одной текущей seat position: одна подвижка машины не сбрасывает
stall clock, круги и отход/возврат вокруг неё также не дают credit. Прежнее
правило любого XZ-сдвига могло бесконечно откладывать short correction рядом
с машиной. Остались прежние2s наблюдения RUNNING,3short attempts и absolute
reboard deadline; дальнего actor helper к машине не переносит.

После нового ordinary US `proof4-final-v4` исправлен отдельный retry gap:
живой пассажир200A оставался примерно70м от healthy припаркованной машины,
собственный `SCR_AIGetInVehicle` был RUNNING без progress около97s, но retries0.
Short entry правильно отклонял дистанцию, а прежняя native retry ветка принимала
только исчезнувший/FAILED action. Теперь настоящий RUNNING stall также может
расходовать прежние два native retries. Только exact свободный actor без
compartment/vehicle link, get-in/out/switch transition и чужого RUNNING action
допускает повтор; guards повторяются после cancellation. Новый owned entry
получает свою локальную2s observation baseline, absolute deadline и бюджеты
не продлеваются. `VEHICLE_REBOARD_NATIVE_RETRY` сохраняет distance, progress age,
номер native retry и remaining deadline; physical relocation=0.
Regression исполняет actual EnsureEntry12 cases и CanRestartEntry17 cases:
старый код давал FAIL на distant RUNNING stall, новый профильный contract PASS.
Новый matched Cargo native A/B завершён отдельно в
`.codex-runtime/issue78-finalization-20261029/reboard-stalled-running/`:
before WB0/native0 сохранил119actual RUNNING stall polls, retries0 и timeout;
after WB0/native0/frozen audit0,29checks/0failures, одна native retry после
actual stall108.795м, тот же Cargo вернулся в exact seat, original5/samecar,
два маршрута и высадки, одна покупка, прежний deadline. Full stopped logs:
script/native fatal0, source drift0, raw resources28FAIL каждого. A/B использует
сохранённый333-file baseline + только два production изменения Action/Flow,
без новых QueueApproach/Overturn lanes; это узкий controlled proof, не новый
ordinary soak всей текущей сборки. Ранние V1/V2 fixture FAIL сохранены в отчёте;
прежний retained PASS ниже не подменяет новый A/B.

`VEHICLE_REBOARD_ENTRY` читается каждые5s и при deadline: exact member/seat,
distance/height/speed, accessibility/lock/reservation/occupant, native access,
transitions, owned/selected/executed actions, retry/short budgets и progress age.
Диагностика не вызывает stateful `EntryBlocked` и не меняет clocks/actions.
`Test-ReboardApproachProgressContracts.ps1` исполняет извлечённый production
predicate на круговом jitter, отходе/возврате, движении машины и реальном3D
приближении. Это offline проверка причины, не native proof старых timeout.

Свежая проверка29.10.2026: профильный набор6/6PASS; controlled retained repeat
US и USSR каждый Workbench0/native0/frozen functional audit0,26checks, два
физических маршрута и высадки, original5actors/та же машина/lease/Rpl/одна
покупка. Строгие resources28FAIL каждого сохранены. Полные stopped logs и hashes:
`.codex-runtime/issue78-finalization-20261029/reboard/REPORT.md`. Обычная война
и все исторические причины timeout этим не объявляются проверенными.

Высадка/освобождение места недееспособного original crew также ограничены25м.
Операция не восстанавливает здоровье или life state. ТС этим helper не двигается.
Прежняя короткая mobility relocation имеет собственные safety/identity guards;
дальнего перемещения машин это изменение не добавляет. Cleanup отдельно
обрабатывает недееспособного original actor, который уже полностью отсоединён
от compartments, но физически остался внутри расширенного hull. Право берётся
из captured текущей группы/agent/entity/Rpl и exact lease generations, а не
из истории трассировки. Требуются свежая whole-job проверка отсутствия human/
foreign occupants и transitions, медленная машина и безопасная сухая точка
не дальше25м. Перенос сохраняет life state и health, использует прежний общий
лимит3 managed exact attempts и прежний clearance deadline. Недоказанный
контекст оставляет cleanup fail-closed; логический выход и удаление техники
остаются у прежнего owner.

Та же captured permission разрешает короткую коррекцию полностью отсоединённого
`ALIVE` original actor внутри expanded hull. Дополнительно отсутствует живой
owned native `GetIn`/`GetOut` action; selected/executed action также проверяется.
Стоячая сухая destination дважды проходит `AICF_InfantrySpawnPlacement`, её
высота отличается не более чем на5м, actual3D displacement не превышает25м.
Общий лимит3/deadline сохраняется. Событие
`VEHICLE_CLEANUP_ALIVE_HULL_RELOCATED` содержит `life_state`; прежнее INCAP
событие остаётся отдельным. История actor и старые trace не дают permission.

## Проверки

`tools/Invoke-AICFVehicleBehavior.ps1` создаёт byte-exact изолированный snapshot
и запускает только через terminal Workbench и `Start-AICFRuntime.ps1`.
`AICF_VehicleBehaviorSettingsProbe` проверяет реальные Encode/Decode и библиотеку;
`AICF_VehicleBehaviorProbe` проверяет физический lifecycle при управляемой native
угрозе и реальной потере driver. Это controlled проверка, не обычная война.
Matching `Test-VehicleBehaviorLog.ps1` требует occupants/outside, same asset,
replacement/defer cases, exact CLI, native exit и full stopped logs. Строгий
resource gate учитывается отдельно и не превращается в PASS от functional checks.

Runner принимает `-Cases transport,combat`, `-Faction US|USSR`, новый
`-EvidenceRoot` и отдельный `-ServerPort`. Он сохраняет source/fixture SHA256,
Workbench logs, launcher manifest, полный server profile, native exit,
matching audit и раздельные targeted/strict-resource/overall verdicts. Fixture
создаёт свой roster, управляемое перемирие и native threat, повреждает одного
бойца и действительно теряет водителя; это не proof платного найма или30минут
обычной войны. Проверяются обе стороны, сохранённые actor identities, прежняя
машина/lease, отсутствие resurrection/heal и фактическое движение после N.
TRANSPORT дополнительно проверяет дальний новый intent при продолжающейся
угрозе; разрешение native combat override действует только для этого intent.

`-Cases combat -CleanupIncapProbe` добавляет отдельную controlled ветвь.
Fixture после штатного egress коротко помещает своего бойца в hull и вызывает
native `ForceUnconsciousness(0)` только для подготовки дефекта. Затем настоящий
cleanup должен физически освободить hull, сохранить health/resilience/life,
не превысить3 attempts, не изменить deadline и завершить release. Обязательны
более поздний native readback и `CLEANUP_INCAP_*` receipts; подготовка сама
не считается успешным recovery. В production fixture отсутствует.

Отдельный `-aicfCleanupAliveProbe 1` использует того же original survivor после
настоящего egress. Только fixture отключает movement controls и помещает его
в expanded hull вне solid body; unconsciousness/health/life не меняет.
`CLEANUP_ALIVE_*` требует более поздний physical clearance, неизменные
life/health/resilience, общий budget≤3, прежний deadline и full release.
Контракты отдельно исполняют384 состояния ALIVE/INCAP/DEAD и физические flags;
source mutants не заменяют native compile/runtime.

Settings probe выполняет23 проверки schema1→TRANSPORT/300, schema2/custom N,
обязательных полей/границ и настоящих Save/update/delete/reload библиотеки в
отдельном `$profile:AICF_BehaviorSettings`. Client/JIP probe дополнительно
принимает `-aicfVehicleBehaviorReadback 1`; matching client auditor —
`-BehaviorSettings`. Это hidden widget/RPC/readback проверка, не визуальный UX.

Focused offline contracts охватывают settings, lifecycle/native policy,
captured topology, короткую коррекцию, fit roster и incapacitated hull cleanup.
`Test-VehicleFitRosterContracts.ps1` исполняет извлечённые production predicates
и cargo-fill body с адаптерами native типов; это не native пробуждение AI.
Свежий focused fit-roster набор5/5 и32 controls сохранён в
`.codex-runtime/vehicle-behavior-20261028/fit-roster-after-v3/REPORT.md`.
Итоговые source manifests и раздельные offline/compile/runtime verdict сведены
в [TESTING.md](TESTING.md): полный170PASS, production graphs3/3PASS, controlled
native receipts и оставшиеся ordinary-war/MOVING/native-error FAIL. Исправления
test-only анализаторов имеют отдельные версии и controls. Визуальные критерии остаются
[NOT RUN](VEHICLE_VISUAL_CHECK.md).

Текущие результаты и ограничения находятся в [TESTING.md](TESTING.md).
Исторические #78 результаты относятся к исходникам до этих новых правил.

## Native garbage и удерживаемая машина

`AICF_VehicleGarbagePermission` принадлежит exact lease; Fleet регистрирует её после accepted binding. Registry и native entity references — weak; current permission сверяет faction/slot/group/trip/lease/vehicle generations, EntityID/component Rpl, master authority и Fleet membership. `ACTIVE`, `RELEASE_PENDING` и `FAILED_CLOSED` защищены до safety-approved physical transfer, включая повреждённую свою машину. Чужие машины сохраняют stock GC; corpse retention не меняется.

Owned `OnInsertRequested`/`OnBeforeDelete` возвращают veto до stock resource subscriptions; остальные entities проходят `super` ровно один раз. Blacklist, глобальный Flush и общие native lifetimes не меняются. Если собственный veto прекратил tracking, successful physical transfer снимает registry до очистки lease reference и возвращает exact entity в native `Insert` с обычной конфигурацией. Существующее tracking не перезапускается; другая current permission запрещает stale restoration. Owner teardown использует captured entity/Rpl/master/world fence без уже уничтоженных weak Fleet/lease; current protection по-прежнему требует всех owner fences.

`GroupTrace` пишет `gc_inserted`, `gc_lifetime_s`, `gc_remaining_s`, `gc_permission_current`, `gc_veto_ended_tracking`. Native `Insert` и callback разнесены по времени: немедленное IsInserted1/lifetime−1 является pending request. Controlled probe ждёт stock callback и committed TTL5 до Fleet binding, проверяет настоящий delete veto, release/teardown restoration и обычное удаление released/unowned машин с TTL1. Local authority control сохраняет actual native role/Rpl: local spawn не объявляется proxy без readback.

В stopped `soak-USSR-v3` исправный UAZ исчез примерно через20мин после высадки. Историческая причина остаётся **UNPROVEN**: совпадение времени не доказывает GC. Старые Soak snapshots не включают эту GC permission и последующий recruitment short hop. Свежие controlled и ordinary gates подводятся отдельно в TESTING.

Native v5 установил ограничение server-local oracle: `SpawnEntityPrefabLocal` UAZ на server остаётся actual MASTER1/PROXY0, а bare Vehicle без prefab получает EntityID0. Это не proxy negative. Server fixture сохраняет положительный `LOCAL_NATIVE_AUTHORITY_READBACK`; реальный отказ authority проверяется отдельно на client/JIP с подготовленной private Fleet/lease и native realm0. Source guard не подменяется тестовым Boolean. Native v5 также подтвердил restoration после release и sole-Fleet teardown: default lifetime1200s/remaining1199s. Полные дальнейшие gates остаются отдельными.

Client gameplay-world local spawn фактически вернул `CanSpawnEntityPrefab=0`;
этот setup FAIL сохранён. Новый test-only owned-controller RPC при обоих
probe flags создаёт отдельный empty UAZ в безопасной точке10..20м от actual
avatar. Owner получает native RplId; существующий Tick ждёт настоящий streamed
proxy, затем private client Fleet/ACTIVE lease с полными IDs/generations.
Обязательный `GC_NO_AUTHORITY_REJECTED` на первом client и JIP требует actual
server0/master0/proxy1 и настоящих отказов Register/CanProtect. Server удаляет
только свой exact empty asset после двух свежих occupancy checks; NPC и player
vehicles не меняет. Это controlled authority gate, не визуальная проверка;
его native verdict подводится отдельно.

RPC preparation использует server realm, оба test CLI flags и exact
PlayerManager/controller ownership. В свежем native diagnostic запрос дошёл
до server с owner_exact1, но PlayerController RplComponent имел
master0/proxy0/role0: vehicle-specific master predicate для этого controller
неприменим и удалён только из fixture RPC guard. Spawned vehicle по-прежнему
требует master/identity; client требует настоящий proxy. Один последующий
same-tick setup отказал `NOT_EMPTY_OR_UNSAFE` без branch receipt; его причина
остаётся UNKNOWN. Safety diagnostic сохраняет native seats/occupants,
nearby characters/resources и player links; эти safety guards не ослаблены.
Следующий JIP diagnostic отдельно подтвердил `nearby_character=1` при exact
identity, authority, пяти пустых местах и resources_clear1. Поэтому каждый
spawn candidate до создания машины теперь проходит тот же native sphere5
character clearance; занятую точку fixture пропускает. Повторные post-spawn
и cleanup guards сохранены, окружающие actors не перемещаются.

Health audits различают raw classifier в `Tick` и public classifier для
`PrepareRoute`: raw failure проверяется до reconcile/create/newcrew; только
`VEHICLE_OVERTURNED` может перейти в typed helper с исходным3s/health/identity
scope. Combined roll+movementDamage1 сохраняет старый raw priority OVERturned,
но actual helper отвергает recovery повреждённой машины. Любой такой отказ
сохраняет snapshot/AbortOwnedActions/Fallback. Четыре устаревших matchers после
этого изменения усилены: NativeRouteLoss39 и MovementDamage91 controlsPASS,
Stage3/Stage35PASS; independent review0. Damage contract исполняет фактическую
Tick health-ветку/public classifier и отдельно полный actual helper для12
health/scope случаев; физическое восстановление доказывает отдельный native11,
а не этот adapter. Full198 исходныйFAIL4 сохранён; новый полный gate учитывается
отдельно после test delta.

Canonical health audits требуют Windows PowerShell5.1: JSON bridge использует PSObject.Properties, без ConvertFrom-Json -AsHashtable. После исправления совместимости все4 audits заново PASS в exactWindowsPS5.1; старый full-v7 shard5FAIL сохранён отдельно.
