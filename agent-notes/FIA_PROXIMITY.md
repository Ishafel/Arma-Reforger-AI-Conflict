# Proximity lifecycle дополнительных сил FIA

Issue #59. Общий Core adapter `AICF_FIAProximityLifecycle` подключает экипажи
гарнизонов, высаженный десант и экипажи дорожных патрулей к штатному
`SCR_AIGroup` / `ProximityDriven`. Игрок приходит через native observer,
US/USSR — через существующий `AICF_ManagedObservers`; FIA себя не активирует.
Состав, strongpoint, fleet cap и economy не меняются. Easy по-прежнему без
дополнительных гарнизонов и дорожных патрулей.

До acquisition сервис читает `ObserversSystem` с `GetDespawnDistance()`
штатного ambient system и `AIWorld.CanActivateGroup(null)`. Дальнее ожидание
не занимает общий интервал успешного spawn и не блокирует другие slots.
После создания controller members материализует только native lifecycle и
очередь AIWorld. `BeginRosterSpawn` и принудительный `KeepCaptureEligible`
для этих FIA групп не используются. Native spawn/despawn bands, very-near
block, hysteresis, permanent LOD pins и budget остаются у движка.

Якорь controller обновляется по живому бойцу через `AICF_GroupRuntime`, а не
используется как положение отряда. После dormancy сохраняется последнее
фактическое положение. Vehicle entity не удаляется и не создаётся повторно:
identity, lease, повреждения и vehicle inventory остаются прежними.

## Логические места и incarnation

`AICF_FIALifecycleMember` хранит исходный seat index, prefab, текущую entity/ID,
agent и постоянный флаг потери. Native `OnAgentRemoved` вне плановой dormancy
закрывает место; `ExpandOneMember` получает только непотерянные места. При
полном уничтожении native dormant alive count становится нулевым. Initial
plan и generation не перебрасываются. Передача пассажира в desant controller
переносит ту же запись; она не создаёт нового бойца или места.

Плановый desant transfer начинается с `BeginTransfer` до native `RemoveAgent`.
Source удерживает logical seat, destination помечает входящую identity и не
привязывает её к другому свободному месту. Пока remove/add не завершены,
expansion и dormancy обеих сторон откладываются. Перед destination Add и
rollback Add повторяется `TransferCurrent`: обе group/generation/vehicle
identities, та же entity/ID/agent, живой authoritative AI.

`Transfer` завершает попытку при любом результате native Add: исходный parent
означает rollback без изменения logical seat, destination parent — перенос той
же записи. Неплановая потеря, смерть и неизвестный parent остаются потерями;
флаг `m_bLost` не сбрасывается. Pending references снимаются при завершении
попытки и Stop. Это закрывает P2 PR #60: повторный `OnAgentAdded` исходной группы
сам по себе не восстанавливает ошибочно потерянное место.

Перед dematerialization сохраняются server-generated inventory snapshot
штатного arsenal serializer и здоровье hit zones. После native materialization
adapter привязывает новую entity identity к тому же месту, ждёт integration
equipment readiness, восстанавливает snapshot и здоровье, затем разрешает
посадку. RHS/WCS readiness остаётся в `AICF_RHSContentProfile`, Core не знает
PMC API. Snapshot не является полной persistence-моделью состояния персонажа.

Member integrity observers снимаются до native удаления и заново привязываются
после восстановления. Постоянный recovery ban не сбрасывается. Initial boarding
сохраняет `RecoveryAllowed`; fenced native reactivation допускает посадку новой
incarnation выжившего после `Current`/`Ready`. Recovery существующих бойцов,
route teleport и запрет восстановления погибших сохраняют прежние gates.

`Current` сверяет server/master, group/entity identity, vehicle identity,
faction и generation. Initial смена владельца базы закрывает slot и очищает
очередь. Захват после первого READY сохраняет прежнее поведение обороны.
`Stop` переводит controller в `Manual`, purges native queue и оставляет закрытый
fence для устаревших callbacks. Собственного таймера и subscriptions у adapter нет.

## Очистка и occupancy

`AICF_VehicleCleanupManager.CanDormantFIA` проверяет свежие compartments и
переходы всех удаляемых members. Проверка повторяется непосредственно перед
native destructive loop. Игрок/main entity, чужой или неизвестный occupant,
неизвестная identity и незавершённый вход/выход запрещают dormancy.
Исходное тело в кресле распознаётся по сохранённым entity/ID и остаётся в мире;
оно не требует держать выживших в активной симуляции. Живой occupant должен
принадлежать исходной crew/desant группе. Vehicle delete здесь отсутствует.

Сохраняй явные ветви `if (occupant) ... else if (seat.IsReserved())`: сохранённый
runtime исходной компактной формы с вложенным `continue` дошёл до null occupant.

## Проверки

`tests/contracts/Test-FIAProximityContracts.ps1` входит в offline runner;
проверяет native boundaries, authority/identity, logical losses, snapshot,
equipment gates, occupancy и Stop с отрицательными мутациями. Existing
FIAGarrison/Integrity и ManagedObservers contracts остаются отдельными gates.

Три fixture помещаются только в Core `Scripts/Game/ZZTests/` isolated stage:

- `AICF_FIAProximityProbe.c` под своим именем;
- `AICF_FIAProximitySafetyProbe.c` как `ZZA_AICF_FIAProximitySafetyProbe.c`;
- `AICF_FIAProximityPlayerProbe.c` как `ZZB_AICF_FIAProximityPlayerProbe.c`.

Порядок нужен для modded методов. Baseline получает только первый файл.
`-aicfFIAProximityProbe us|ussr|player|scale|baseline` выбирает actor/case;
`scale` и `baseline` наблюдают дальнюю карту 80 секунд, остальные — 20 секунд.
Fixture фиксирует HQ из штатных candidates и PRNG input `590013`, удерживает
армии у HQ и перемещает только живой actor. `player` требует настоящего client,
подтверждает controlled entity и останавливает временные managed observers.
Тестовое перемещение выполняется native API владельца character на реальном
client; новые observers fixture не регистрирует. Для occupancy сервер передаёт
этому же игроку уже сидящего исходного бойца FIA через `SetInitialMainEntity`.
Это сохраняет known member identity и проверяет защиту от реального игрока
в исходном экипаже. Server и client читают controlled entity и фактическую
машину через `CompartmentAccessComponent.GetVehicleIn`, затем сервер вызывает
production dormancy. Должны сохраниться все бойцы и та же машина.

Player case запускается с `-aicfFIAPlayerOccupancyOnly 1` и заканчивается после
14 уникальных проверок approach/retention/desant/identity/player occupancy.
Полные loss/return/road cases проходят отдельно без игроков; их этот режим
не подменяет. Client launcher сначала проверяет свежие server CLI, process
и `ROSTER_READY` по [DEVELOPMENT.md](DEVELOPMENT.md).
Budget case временно снижает native limit и приостанавливает builder floor;
native manager/queue/lifecycle не подменяются. Десант получает контролируемый
deployment intent; фактические выход, transfer и dormancy читаются из мира.

Запускай Workbench и server/client только по [DEVELOPMENT.md](DEVELOPMENT.md).
После штатного закрытия анализируй полный console.log:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/log-audits/Test-FIAProximityLog.ps1 `
  -LogPath <console.log> -LauncherPath <terminal.txt> -NativeExitPath <exit.txt>
```

`-Baseline` проверяет воспроизведение исходного дефекта, `-Player` добавляет
controlled-player/occupancy cases и требует `-ClientLogPath`,
`-ClientLauncherPath`, `-ClientNativeExitPath`; `-Easy` проверяет пустой план.
Client проверяется по реальным controlled/occupied readbacks, manifest,
полноте лога и exit. Игровой участок до первого `Game destroyed.` проверяется
на script errors; native post-world-destruction diagnostics считаются отдельно,
VM/fatal/AICF leaks запрещены во всём логе. Финальный fixture реплицирует
завершение и закрывает client до server disconnect, затем server;
его CallLater явно снимаются при завершении. Полные native diagnostics сохраняются.
Фактические сочетания и verdict: [TESTING.md](TESTING.md). FPS, визуальный бой,
длительный soak и полная persistence физиологии персонажа этим не доказываются.

Отдельный `AICF_FIAProximityOwnershipProbe.c` помещается после общего probe
как `ZZZ_AICF_FIAProximityOwnershipProbe.c`, режим `ownership`. Он ставит
реальный native request, меняет faction базы до dispatch, затем проверяет
отказ expansion, retirement, очищенную очередь и нулевой roster. Для его лога
используется `Test-FIAProximityLog.ps1 -Ownership`.

Для регрессии P2 добавь `AICF_FIADesantTransferProbe.c` после common/safety
как `ZZZ_AICF_FIADesantTransferProbe.c` и флаг
`-aicfFIADesantTransferProbe baseline|after` вместе с common `us` mode.
Штатный `GetOnAgentAdded()` callback первого destination Add синхронно
удаляет принятого agent до возврата Add; production видит отказ и выполняет
свой rollback. Проверяются тот же parent/entity/ID, capacity=agents, Ready,
retry и общий roster без дубликатов; затем идут обычные loss/dormancy/road cases.
Subscription явно снимается при Stop. Baseline заканчивается после readback
исходного несоответствия. `Test-FIADesantTransferLog.ps1` проверяет полный
остановленный log/manifest/exit, исходный дефект через `-Baseline` либо все
семь transfer cases и полный existing proximity log gate после исправления.
