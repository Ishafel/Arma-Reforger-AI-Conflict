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

## Владельцы и цикл

`AICF_VehicleBehaviorFlow` возвращает typed outcomes; только
`AICF_TransportTripController` переводит TRANSIT↔TACTICAL_DEPLOYMENT, снимает
route waypoint, временно восстанавливает infantry order через Handoff/OrderPlanner
и вновь подключает vehicle utility. Lease, vehicle identity и debit сохраняются.
Автоматические trips без ручного пресета сохраняют прежний одноразовый цикл.

TRANSPORT при местной угрозе высаживает весь живой отряд. После окончания боя
возвращает выживших в ту же машину; новый приказ с перемещением дальше N также
может потребовать возврат. После высадки по прибытии пеший приказ сохраняется
до нового дальнего intent. Порог N решает необходимость повторной посадки
пешего отряда; он не расширяет прежнюю зону mounted destination wait и не
останавливает уже посаженную машину с незавершённым маршрутом. Погибший/недееспособный водитель
заменяется свободным живым бойцом в текущей trip-копии рассадки.

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
