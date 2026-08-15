# Проверенные API Arma Reforger для этапов 0–3

## Зафиксированная версия

- Текущий production runtime / Reforger Tools / ServerDiag: 1.8.0.10 (engine 191843). Исторический Stage 0/1 PASS был получен на 1.7.0.54 (engine 190965) и не переносится автоматически на 1.8.
- Официальный текущий snapshot: [BohemiaInteractive/Arma-Reforger-Script-Diff v1.8.0.10](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/tree/v1.8.0.10). Ссылки на commit `2735631...` ниже сохранены как историческое 1.7 evidence, если раздел явно не говорит об 1.8.
- Проверенный текущий commit: [`b46bdd8f4932f3a256c765f93a44417996a6da73`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/tree/b46bdd8f4932f3a256c765f93a44417996a6da73). Исторический 1.7 commit: [`2735631ce1400eaf9f1761c66cdee10c46921d37`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/tree/2735631ce1400eaf9f1761c66cdee10c46921d37).

Script Diff — снимок скриптов конкретной версии игры, а не обещание вечной совместимости. После обновления Reforger этот документ и каждую используемую сигнатуру нужно сверить повторно.

Исторический Stage 0 был подтверждён ручным runtime-тестом с итогом `[AICF][STAGE0][RESULT][PASS]`. Для текущей Stage 1-ветки дополнительно выполнены Workbench `Validate Scripts` и direct `ArmaReforgerServerDiag.exe` smoke-запуск штатного Arland: созданы `4 × US` и `4 × USSR`, подтверждены роли `2 ATTACK / 1 DEFEND / 1 RESERVE` на сторону и начальные приказы. Это ещё не полная A/B-приёмка Stage 1.

Статус vehicle-domain: **FAILED — REWRITE IMPLEMENTATION/CUTOVER COMPLETE, RUNTIME ACCEPTANCE NOT PASSED**. Legacy runtime/coordinator fragments удалены, новый vehicle-domain включён без второго active side-effect path. Проверенные ниже сигнатуры, ресурсы и исторические engine/API решения сохраняются как справочная основа; successful compile/static development evidence не повышает ни один runtime-результат. Исторические `PASS`, `FAIL`, `BLOCKED` и `NOT RUN` остаются без переклассификации.

Финальный dirty-working-tree rewrite snapshot прошёл `tools/Test-Stage3Static.ps1` и `tools/Test-Stage35Static.ps1` вместе с negative-fixture rules `COORDINATOR_SIDE_EFFECT`, `FLOW_CROSS_CALL`, `WAYPOINT_SIDE_EFFECT_OWNER`, `TRANSITION_OUTSIDE_CONTROLLER`, `TRANSITION_EFFECT_ORDER`, `WAITING_WITH_LEASE`, `HANDOFF_CLEARANCE_GATE`, `CLEANUP_CLEARANCE_OWNER`, `CLEANUP_IDENTITY_SAFETY` и `VEHICLE_LIVENESS_OWNERSHIP`. Workbench log `.cache/vehicle-rewrite-final-validate-20260812-r2/console.log` подтверждает Game `5692` files / `11109` classes, CRC32 `7f2cbec0`, `Game successfully created`, `SCRIPT(E/F)=0`, `ENGINE(F)=0`. Два harness `PLATFORM(E)` (`SteamAPI_Init failed`/platform services) и 25 shutdown `RESOURCES(E)` строк сохранены как platform/shutdown-resource caveat; они не являются AICF script compile error или runtime gameplay evidence.

Final-tree headless development smoke `.cache/Stage35-Rewrite-FinalSmoke-20260812-002210` (`2026-08-12T00:22:10+03` — cutoff `2026-08-12T00:22:51.7568935+03`) создал Game без `SCRIPT(E/F)`, `ENGINE(F)` и VM exception, но получил 12 `BACKEND(E)` (`SSL peer certificate`/`BAD_REQUEST`) до AICF bootstrap. Поэтому `AICF=0`, roster/vehicle evidence отсутствует, а smoke имеет статус **BLOCKED external backend** и не является Repeat T, Repeat-T2 или M30. Commit/SHA не записан из-за dirty working tree.

Первый post-cutover run `stage1-server-30017` на 1.8 завершился до подключения клиента: все восемь group entities были bound, но `AIChar:0` сохранялся 30 секунд. Причина — устаревший direct `SpawnUnits()`; в 1.8 его синхронные transient navmesh failures больше не попадают в retry queue. Managed path переведён на bind/observers → `RequestSpawn(5)`, а vehicles-off dedicated smoke `stage1-server-8212` подтвердил восемь точных roster `5/5` и `ROSTER_READY` за 5.986 с. Это устраняет Stage 1 blocker, но не является runtime evidence vehicle-domain; подробности — в [отчёте о spawn cutover](STAGE_1_SPAWN_CUTOVER_2026-08-13.md).

## Локальный кэш

```bash
./tools/fetch_reforger_api_reference.sh
```

Скрипт загружает официальный tag-архив в:

```text
.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.10/
```

Кэш исключён из Git, не включается в addon и не является сторонним модом. Скрипт не скачивает данные повторно, если целевой snapshot уже присутствует.

## Подтверждённые контракты Stage 0 — историческая основа

Следующие контракты были зафиксированы для Stage 0 и продолжают использоваться Stage 1. Упоминания одноразового контроллера ниже описывают именно подтверждённый Stage 0, а не текущий долгоживущий match loop.

### Запуск Conflict и server authority

Источник: [`SCR_GameModeCampaign.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/GameMode/SCR_GameModeCampaign.c).

```c
override void OnGameStart()
bool HasStarted()
ScriptInvoker GetOnStarted() // callback: void, без параметров
SCR_CampaignMilitaryBaseManager GetBaseManager()
SCR_CampaignFaction GetFactionByEnum(SCR_ECampaignFaction faction)
FactionKey GetFactionKeyByEnum(SCR_ECampaignFaction faction)
```

Штатный `OnGameStart()` вызывает `Start()` только при `GetGame().InPlayMode() && IsMaster()`. В `Start()` база-менеджер получает `UpdateBases(true)`, затем выполняется `OnAllBasesInitialized()`, и только после этого вызывается `OnStarted()`. Поэтому `modded OnGameStart()` обязан сначала вызвать `super.OnGameStart()`.

В нашем bootstrap дополнительно проверяются реальные методы `Replication.IsServer()` и `IsMaster()`. Контроллер и модель графа являются обычными server-only объектами; клиентский код их не создаёт.

### Conflict-базы

Источник: [`SCR_CampaignMilitaryBaseManager.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/Campaign/SCR_CampaignMilitaryBaseManager.c).

```c
int GetBases(notnull out array<SCR_CampaignMilitaryBaseComponent> bases, Faction faction = null)
bool IsBasesInitDone()
int GetActiveBasesCount()
int GetTargetActiveBasesCount()
OnAllBasesInitializedInvoker GetOnAllBasesInitialized() // callback: void
```

Ограничение: manager `GetBases()` намеренно возвращает только `SCR_ECampaignBaseType.BASE` и `SOURCE_BASE`; `RELAY` исключён. Его `OnAllBasesInitialized` invoker срабатывает до последнего `RecalculateRadioCoverage()` для BLUFOR/OPFOR, поэтому fallback-callback bootstrap откладывает работу через `CallLater(..., 0, false)`.

Для полного графа используется [`SCR_MilitaryBaseSystem.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/Systems/SCR_MilitaryBaseSystem.c):

```c
static SCR_MilitaryBaseSystem GetInstance()
int GetBases(notnull out array<SCR_MilitaryBaseComponent> bases)
```

Каждый результат явно приводится через `SCR_CampaignMilitaryBaseComponent.Cast()` и проверяется `IsInitialized()`.

### Связи и доступность цели

Источник: [`SCR_CampaignMilitaryBaseComponent.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/Components/Locations/SCR_CampaignMilitaryBaseComponent.c).

```c
bool CanReachByRadio(notnull IEntity entity)
bool IsValidTarget(notnull SCR_CampaignFaction faction)
SCR_SpawnPoint GetSpawnPoint()
SCR_CampaignFaction GetCampaignFaction()
SCR_ECampaignBaseType GetType()
string GetBaseName()
```

`CanReachByRadio()` ищет `SCR_CoverageRadioComponent` у target, но в 1.7.0.54 обращается к source `m_RadioComponent` без собственного null-check. Перед вызовом код отдельно проверяет `SCR_CoverageRadioComponent` у source owner. Рёбра считаются ориентированными; обратное ребро не добавляется предположением.

`IsValidTarget()` отклоняет свою базу и базу без HQ radio traffic. Это штатная стратегическая доступность, но не navmesh-проверка.

### Фракции США и СССР

Источники: [`SCR_GameModeCampaign.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/GameMode/SCR_GameModeCampaign.c), [`SCR_CampaignFaction.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/Faction/SCR_CampaignFaction.c).

```c
SCR_CampaignFaction GetFactionByEnum(SCR_ECampaignFaction.BLUFOR)
SCR_CampaignFaction GetFactionByEnum(SCR_ECampaignFaction.OPFOR)
FactionKey GetFactionKey()
SCR_CampaignMilitaryBaseComponent GetMainBase()
ResourceName GetDefendersGroupPrefab()
```

Код не ищет фракции по придуманным таблицам: он получает штатные стороны и затем требует, чтобы ключи были `US` и `USSR`. Конкретные defender prefab находятся в runtime-конфигурации фракций; их GUID не подменяется предположением.

### Позиция появления и создание сущностей

Источники: [`SCR_SpawnPoint.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/GameMode/Respawn/SCR_SpawnPoint.c), [`game.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/game.c), официальный [`SCR_AmbientPatrolSpawnPointComponent.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/Components/Locations/SCR_AmbientPatrolSpawnPointComponent.c).

```c
void SCR_SpawnPoint.GetPositionAndRotation(out vector pos, out vector rot)
Resource Resource.Load(ResourceName name)
bool Resource.IsValid()
IEntity SpawnEntityPrefabEx(ResourceName prefab, bool randomizeEditableVariant,
    BaseWorld world = null, EntitySpawnParams params = null)
```

Применяется `EntitySpawnParams` с `ETransformMode.WORLD`, `Math3D.AnglesToMatrix()` и `Transform[3]`. Локальный `SpawnEntityPrefabLocal()` не используется.

### AI-группа

Источник: [`SCR_AIGroup.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/Entities/SCR_AIGroup.c).

```c
bool GetSpawnImmediately()
void SpawnUnits()
int GetAgentsCount()
void AddWaypointAt(AIWaypoint waypoint, int index) // inherited AIGroup API
int GetWaypoints(out array<AIWaypoint> outWaypoints) // inherited AIGroup API
```

Если defender prefab отключает immediate spawn, Stage 0 явно вызывал `SpawnUnits()`, после чего до 30 секунд проверял наличие бойцов и назначенного waypoint. Полноценный lifecycle/respawn не входил в Stage 0; в Stage 1 он реализуется отдельно через стабильные слоты и `GetOnEmpty()`.

### Waypoint

Штатный Move-resource подтверждён официальным [`SCR_ScenarioFrameworkWaypointMove.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/ScenarioFramework/Waypoints/SCR_ScenarioFrameworkWaypointMove.c):

```text
{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et
```

Используемые методы:

```c
AIWaypoint.Cast(IEntity entity)
void AIWaypoint.SetCompletionRadius(float completionRadius)
void SCR_AIGroup.AddWaypointAt(AIWaypoint waypoint, int index)
```

Waypoint создаётся на сервере через `SpawnEntityPrefabEx`, а не в локальной клиентской копии, и ставится в индекс `0`, чтобы уже существующие приказы defender-prefab не перехватили тестовую цель.

### Вспомогательные вызовы

Дополнительно по тому же snapshot 1.7.0.54 проверены сигнатуры всех вспомогательных вызовов, которые влияют на порядок инициализации, валидацию и cleanup:

```c
bool SCR_CampaignMilitaryBaseComponent.IsInitialized()
int SCR_MilitaryBaseComponent.GetCallsign()
Faction SCR_MilitaryBaseComponent.GetFaction(bool checkDefaultFaction = false)
Faction SCR_AIGroup.GetFaction()
sealed bool SCR_BaseGameMode.IsMaster()
ScriptCallQueue ArmaReforgerScripted.GetCallqueue()
proto void ScriptCallQueue.CallLater(func fn, int delay = 0, bool repeat = false,
    void param1 = NULL, void param2 = NULL, void param3 = NULL,
    void param4 = NULL, void param5 = NULL, void param6 = NULL,
    void param7 = NULL, void param8 = NULL, void param9 = NULL)
static proto void RplComponent.DeleteRplEntity(
    IEntity entity, bool releaseFromReplication)
```

`CallLater(..., 0, false)` имеет штатные примеры в Script Diff. В Stage 0 `DeleteRplEntity(..., false)` применялся только к сущностям, созданным текущим проходом, если последующий шаг завершался ошибкой. В Stage 1 тот же API используется для управляемых групп и созданных AICF waypoint после их отсоединения от группы.

## Проверенные контракты Stage 1

### Смена владельца базы: порядок аргументов invoker

Источник: [`SCR_MilitaryBaseSystem.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/Systems/SCR_MilitaryBaseSystem.c).

```c
void OnBaseFactionChangedDelegate(SCR_MilitaryBaseComponent base, Faction faction)
typedef ScriptInvokerBase<OnBaseFactionChangedDelegate> OnBaseFactionChangedInvoker
OnBaseFactionChangedInvoker GetOnBaseFactionChanged()
```

У внутреннего dispatch-метода системы порядок другой:

```c
void SCR_MilitaryBaseSystem.OnBaseFactionChanged(
    Faction faction,
    SCR_MilitaryBaseComponent base)
```

Он вызывает invoker как `m_OnBaseFactionChanged.Invoke(base, faction)`. Поэтому подписчик обязан иметь сигнатуру именно `(SCR_MilitaryBaseComponent base, Faction faction)`, несмотря на обратный порядок аргументов внутреннего метода. Stage 1 подписывает и удаляет один и тот же callback через `Insert()`/`Remove()`.

После события Stage 1 заново собирает текущие базы и строит live graph. Если rebuild не удался в отложенном callback, флаг остаётся установленным и следующая попытка выполняется на commander tick; это больше не одноразовый Stage 0 snapshot.

### Уничтожение группы через `GetOnEmpty()`

Источник: [`SCR_AIGroup.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/Entities/SCR_AIGroup.c).

```c
void ScriptInvoker_AIGroupOnEmpty_Callback(AIGroup group)
typedef ScriptInvokerBase<ScriptInvoker_AIGroupOnEmpty_Callback>
    ScriptInvoker_AIGroupOnEmpty
ScriptInvoker_AIGroupOnEmpty SCR_AIGroup.GetOnEmpty()
```

Invoker вызывается только на сервере. Callback принимает `AIGroup`, затем явно приводит его к `SCR_AIGroup`. Комментарий рядом с `GetOnEmpty()` в snapshot говорит «No invoker params», но фактическое объявление delegate и успешная Workbench-компиляция подтверждают параметр `AIGroup group`; обработчик без аргумента использовать нельзя.

На каждую управляемую группу handler добавляется после создания и удаляется при `OnEmpty`, остановке контроллера или cleanup. Сам slot сохраняется и переходит в ожидание replacement.

### Проверка безопасной базы появления

Источники: [`SCR_CampaignMilitaryBaseComponent.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/Components/Locations/SCR_CampaignMilitaryBaseComponent.c), [`SCR_MilitaryBaseComponent.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/Components/Locations/SCR_MilitaryBaseComponent.c), [`SCR_SpawnPoint.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/GameMode/Respawn/SCR_SpawnPoint.c).

```c
bool SCR_CampaignMilitaryBaseComponent.IsInitialized()
Faction SCR_MilitaryBaseComponent.GetFaction(bool checkDefaultFaction = false)
SCR_EBaseCaptureState SCR_CampaignMilitaryBaseComponent.GetCaptureState()
bool SCR_CampaignMilitaryBaseComponent.IsBeingCaptured()
bool SCR_CampaignMilitaryBaseComponent.AreEnemiesPresent()
SCR_SpawnPoint SCR_CampaignMilitaryBaseComponent.GetSpawnPoint()
bool SCR_SpawnPoint.IsSpawnPointEnabled()
bool SCR_SpawnPoint.IsSpawnPointActive()
FactionKey SCR_SpawnPoint.GetFactionKey()
```

Replacement может появиться только если база инициализирована, принадлежит нужной фракции, имеет `GetCaptureState() == SCR_EBaseCaptureState.NONE`, не захватывается, не содержит противника и предоставляет активный spawn point той же фракции. Проверка выполняется непосредственно перед `SpawnEntityPrefabEx`, а не только при постановке 30-секундного таймера. Отклонённые причины остаются видимыми в `SPAWN_SITE_REJECTED`.

`IsSpawnPointActive()` базового `SCR_SpawnPoint` возвращает `true`, но API виртуален по фактическому типу spawn point, поэтому отдельная проверка сохранена.

### Замена и удаление waypoint

Источники: generated [`AIGroup.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/generated/AI/AIGroup.c), [`SCR_AIGroup.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/Entities/SCR_AIGroup.c), официальное использование `DeleteRplEntity` в [`SCR_EntityHelper.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/Helpers/SCR_EntityHelper.c).

```c
void AIGroup.RemoveWaypoint(AIWaypoint waypoint)
static proto void RplComponent.DeleteRplEntity(
    IEntity entity,
    bool releaseFromReplication)
```

`RemoveWaypoint()` только отсоединяет приказ от группы; он не является удалением созданной replicated entity. При retarget/cleanup Stage 1 сначала вызывает `group.RemoveWaypoint(oldWaypoint)`, затем `RplComponent.DeleteRplEntity(oldWaypoint, false)`. Обратный порядок оставлял бы у группы ссылку на уже удалённый объект.

Stage 1 удаляет только собственный waypoint, сохранённый в slot, и не перебирает с удалением все prefab-waypoint группы.

### Репликация билетов и JIP

Источники: [`EnNetwork.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Core/proto/EnNetwork.c), [`Replication.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Core/generated/Replication/Replication.c), [`RplDocs.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/GameLib/replication/RplDocs.c).

```c
[RplProp(onRplName: "OnValueReplicated")]
protected int m_iValue;

static proto void Replication.BumpMe()
```

Stage 1 хранит два primitive ticket value как `RplProp` на уже реплицируемом `SCR_GameModeCampaign`. Authority меняет оба значения, вызывает `Replication.BumpMe()`, а proxy получает snapshot и `onRplName` callback. На authority callback вызывается явно только для локальной диагностики, потому что `onRpl` предназначен для получателя.

Официальное описание state replication прямо указывает, что snapshot-доставка учитывает join-in-progress. Поэтому выбранный `RplProp`-контракт пригоден для JIP-снимка текущих билетов и не заменён одноразовым RPC. Это подтверждение API-механизма, но фактическая синхронизация поздно подключившегося клиента всё ещё входит в полную MVP-матрицу и пока не принята runtime-тестом.

### Параметры ускоренной приёмки из CLI

Источник: generated [`System.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Core/generated/System/System.c).

```c
static proto bool System.GetCLIParam(string param, out string val)
```

Имя передаётся без ведущего `-`; возвращаемый `bool` отличает отсутствующий параметр от присутствующего значения. Параметры, используемые в ускоренной приёмке:

| Срез | Параметры |
|---|---|
| Stage 1/2 | `aicfInitialTickets`, `aicfReplacementTicketCost`, `aicfReinforcementDelayMs`, `aicfCommanderIntervalMs`, `aicfMaxManagedAgents`, `aicfExpectedPlayerFaction` |
| Stage 3.5 roles | `aicfActiveForcesRolesEnabled` (`1`: `3 ATTACK / 1 DEFEND-QRF`; `0`: baseline `2/1/1`, всегда по пять бойцов) |
| Stage 3 lifecycle | `aicfVehiclesEnabled`, `aicfTransportVehiclesPerFaction`, `aicfArmedLightVehiclesPerFaction`, `aicfMaxVehiclesPerFaction`, `aicfVehicleBoardingTimeoutMs`, `aicfVehicleMaxRecoveries`, `aicfVehicleDismountDistanceMeters`, `aicfVehicleRetryIntervalMs`, `aicfVehicleNoRangeProgressTimeoutMs`, `aicfVehicleCleanupDelayMs` |
| Stage 3 movement | `aicfVehicleStuckTimeoutMs`, `aicfVehicleProgressMeters`, `aicfVehicleMotionMeters`, `aicfVehicleObjectiveProgressTimeoutMs`, `aicfVehicleMinimumRouteMeters`, `aicfVehicleMaximumReuseDistanceMeters`, `aicfVehicleMaximumSpawnDistanceMeters`, `aicfVehicleCohesionDistanceMeters` |

Числа после `ToInt()` ограничиваются безопасными min/max, а ожидаемая сторона принимает только `US` или `USSR`. В Stage 3.5 нижняя граница `max_managed_agents` равна 48: она покрывает обязательные 40 бойцов и консервативную pending-проекцию 8, поэтому заниженный CLI-параметр не блокирует replacement lifecycle. Стандартное значение остаётся 64. Исторический Direct ServerDiag smoke подтвердил применение ускоренных значений `initial_tickets=1`, `commander_interval_ms=5000`, `replacement_delay_ms=30000`, `max_managed_agents=64` и `expected_player_faction=US` в событии `CONFIG`.

### Завершение матча

Источники: [`SCR_BaseGameMode.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/GameMode/SCR_BaseGameMode.c), [`SCR_GameModeEndData.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/GameMode/SCR_GameModeEndData.c), [`SCR_GameModeCampaign.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/GameMode/SCR_GameModeCampaign.c).

```c
static SCR_GameModeEndData CreateSimple(
    EGameOverTypes reason = EGameOverTypes.ENDREASON_UNDEFINED,
    int winnerId = -1,
    int winnerFactionId = -1)

void SCR_BaseGameMode.EndGameMode(SCR_GameModeEndData endData)
protected void SCR_GameModeCampaign.CheckForWinner()
```

`EndGameMode()` действует только на master. Сторона считается исчерпавшей подкрепления, когда остатка билетов уже недостаточно для стоимости replacement и у неё нет живых управляемых групп; это также корректно завершает нестандартные профили, где стоимость не делит начальный баланс без остатка. Stage 1 получает фактический индекс победившей фракции из `FactionManager`, создаёт `SCR_GameModeEndData` с `EGameOverTypes.ENDREASON_SCORELIMIT` и именованным `winnerFactionId`, затем вызывает `EndGameMode()` ровно один раз. `MATCH_END` выводится только после наблюдаемого `campaign.IsRunning() == false`, а не сразу после запроса завершения.

Штатный `CheckForWinner()` реализует территориальные условия Conflict и сам может вызвать `EndGameMode()`. Тонкая Arland-интеграция компилируемо переопределяет этот `protected` метод пустым override, чтобы stock victory не опередила Stage 1 ticket condition. Override действует только пока загружен `AIConflictArland`; для другого режима этот addon использовать нельзя.

### Захват базы без обязательного игрока

Источники: [`SCR_SeizingComponent.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/GameMode/Components/SCR_SeizingComponent.c), [`SCR_CampaignSeizingComponent.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/2735631ce1400eaf9f1761c66cdee10c46921d37/scripts/Game/GameMode/Components/SCR_CampaignSeizingComponent.c).

```c
[Attribute("0", desc: "If enabled, at least one player has to be present in the capture area to progress.")]
protected bool SCR_SeizingComponent.m_bCapturingRequiresPlayer;
```

В штатной логике при `true` атакующая фракция без игрока в зоне исключается из прогресса захвата. `SCR_CampaignSeizingComponent` наследует поле как `protected`, поэтому scripts-only Arland-интеграция переопределяет `OnPostInit()` и устанавливает `m_bCapturingRequiresPlayer = false`. Workbench validation подтверждает доступность поля и корректность override; реальный AI-only owner change остаётся обязательной частью A/B runtime-приёмки.

### Arland и `.gproj`

Штатный scenario ID из [Server Config](https://community.bistudio.com/wiki/Arma_Reforger%3AServer_Config):

```text
{C41618FD18E9D714}Missions/23_Campaign_Arland.conf
```

Формат scripts-only проекта и GUID штатной зависимости `58D0FB3206B6F859` подтверждены официальным [`SampleMod_ModdedScript.gproj`](https://github.com/BohemiaInteractive/Arma-Reforger-Samples/blob/main/SampleMod_ModdedScript/SampleMod_ModdedScript.gproj). Путь `Scripts/Game` подтверждён [официальным sample](https://github.com/BohemiaInteractive/Arma-Reforger-Samples/tree/main/SampleMod_ModdedScript/Scripts/Game).

Собственные 16-hex GUID приняты Resource Database. Историческая версия 1.7 и текущий candidate 1.8 прошли Workbench validation; direct ServerDiag smoke на 1.8 загрузил оба проекта и выполнил Stage 1 initial roster до `ROSTER_READY` с восемью готовыми группами.

## Зафиксированные ограничения решений

1. `AIConflictArland` использует `modded SCR_GameModeCampaign`, `SCR_CampaignSeizingComponent` и override штатной победы. При загрузке addon эти изменения действуют на запущенный Conflict, поэтому эталонный сценарий строго ограничен Arland и direct Diag-командой из `STAGE_1_TESTING.md`.
2. `SCR_CampaignMilitaryBaseManager.GetBases()` не даёт relay; полный набор графа по-прежнему берётся из `SCR_MilitaryBaseSystem`.
3. Stage 0 использовал одноразовый graph snapshot. Stage 1 подписан на `GetOnBaseFactionChanged()`, перестраивает live graph после смены владельца и сохраняет флаг повторной попытки до следующего commander tick, если rebuild временно не удался.
4. Live rebuild отражает новые faction/radio-связи только после обрабатываемого события. Он не превращает radio reachability и `IsValidTarget()` в navmesh-проверку и не гарантирует физическую проходимость маршрута.
5. Транзит разрешён только через узлы, принадлежащие выбирающей фракции, включая её `RELAY`. Вражеская или нейтральная objective-base может быть конечной целью, но не транзитной территорией.
6. Safe-spawn API проверяет владельца, contested-state и доступность spawn point непосредственно перед созданием. Это не является геометрическим collision/clearance query; смещения четырёх слотов остаются частью текущего Arland vertical slice.
7. Defender group prefab зависит от штатной runtime-конфигурации US/USSR. Direct ServerDiag smoke подтвердил по четыре трёхбойцовые группы на сторону для текущего Arland, но не гарантирует тот же размер после обновления игры или замены faction prefab.
8. `RplProp` + `BumpMe()` подтверждены API и Workbench для ticket snapshot, однако реальный JIP-клиент, синхронность двух клиентов и UI билетов ещё не прошли полную MVP-матрицу.
9. Workbench `Validate Scripts` и direct ServerDiag smoke `4 × 4` подтверждены. Smoke доказал bootstrap, CLI config, роли, spawn, начальные приказы и `ROSTER_READY`, но не доказал реальный AI capture, retarget, `OnEmpty → 30 s → replacement`, unsafe-site rejection, ticket debit или единственное завершение матча.
10. Полные зеркальные прогоны A/B из `STAGE_1_TESTING.md` ещё не выполнены: нет принятого результата с игроком US и отдельного результата с игроком USSR. Поэтому Stage 1, полный MVP и двухчасовой soak пока не объявлены принятыми.
11. Instance-флаг не является persistent-маркером сохранённых групп. Каждый dedicated acceptance run запускается с новым `-profile` и `-backendFreshSession`.

## Stage 3: проверенные API наземной техники (1.7.0.54)

Базовые API и ресурсы ниже привязаны к локальному official Script Diff `v1.7.0.54` и установленной Resource Database. Прежний post-T9 snapshot прошёл `tools/Test-Stage3Static.ps1` и Workbench 1.7 `Validate Scripts` по пяти конфигурациям: `.cache/stage3-post-t9-vehicle-unstuck-final2-20260809/console.log`, Game CRC32 `946e5a78`, `Script validation successful`, `SCRIPT (E/F)=0`. Это только историческое compile/static evidence: Transport T9 остаётся `FAIL`, dedicated Transport T10/Armed A2 остаются `NOT RUN`, а runtime acceptance новой архитектуры должна подтвердить те же API-контракты заново.

### Faction vehicle catalog

Источники: [`SCR_Faction.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/Faction/SCR_Faction.c), [`SCR_EntityCatalog.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/EntityCatalog/SCR_EntityCatalog.c), [`SCR_EntityCatalogEntry.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/EntityCatalog/EntityCatalogEntry/SCR_EntityCatalogEntry.c).

```c
SCR_EntityCatalog SCR_Faction.GetFactionEntityCatalogOfType(
    EEntityCatalogType catalogType,
    bool printNotFound = true)

int SCR_EntityCatalog.GetEntityList(
    notnull out array<SCR_EntityCatalogEntry> entityList)

ResourceName SCR_EntityCatalogEntry.GetPrefab()
```

Stage 3 получает `EEntityCatalogType.VEHICLE` у фактической `SCR_CampaignFaction`, а затем выбирает проверенный path suffix. GUID не угадывается и не копируется из внешнего списка: сохраняется полный `ResourceName`, который вернул загруженный faction catalog. После выбора выполняется `Resource.Load()`/`IsValid()`.

Проверенные в Resource Database пути:

```text
US transport:   Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et
USSR transport: Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et
US armed light: Prefabs/Vehicles/Wheeled/Conflict_Variants/M1025_armed_M2HB_Conflict.et
USSR armed:     Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_PKM.et
```

В коде присутствуют проверенные fallback-path того же faction catalog, но cross-faction fallback запрещён.

### Active faction после runtime-spawn

Источник: [`SCR_FactionAffiliationComponent.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/Components/SCR_FactionAffiliationComponent.c). Штатные faction-catalog prefab могут хранить фракцию как default affiliation, не выставляя active affiliation у только что созданной entity. Поэтому принадлежность prefab нужному каталогу используется как разрешение на создание, а authoritative spawner затем явно выполняет:

```c
SCR_FactionAffiliationComponent affiliation = ...;
affiliation.SetAffiliatedFaction(faction);
Faction assignedFaction = affiliation.GetAffiliatedFaction();
```

Только после этого Stage 3 проверяет точный `FactionKey` и передаёт машину AI/group systems. Это соответствует штатному способу смены affiliation в `SCR_FactionAffiliationComponent.SetFaction()`, `SCR_AIGroup` и Conflict-компонентах. Отсутствие компонента или несовпадение после setter является terminal configuration failure для текущего `group_generation`, а не причиной повторять spawn каждые 10 секунд.

### Безопасная позиция создания

Источник: [`SCR_WorldTools.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/Global/SCR_WorldTools.c). Штатный Conflict использует тот же query при создании HQ vehicle в `SCR_CampaignMilitaryBaseComponent`.

```c
static bool SCR_WorldTools.FindEmptyTerrainPosition(
    out vector outPosition,
    vector areaCenter,
    float areaRadius,
    float cylinderRadius = 0.5,
    float cylinderHeight = 2,
    TraceFlags flags = TraceFlags.ENTS | TraceFlags.OCEAN,
    BaseWorld world = null)
```

До геометрического query `AICF_VehicleAcquisitionFlow` повторно применяет `AICF_ConflictAdapter.GetSpawnRejectionReason`: base существует и initialized, принадлежит нужной стороне, не contested/capturing, enemies отсутствуют, spawn point enabled/active и имеет правильный faction key. Все safe base сортируются детерминированно по расстоянию и `BaseKey`; затем каждая по очереди проходит максимальную spawn-дистанцию, `FindEmptyTerrainPosition` и измерение расстояния всех живых managed members до найденного точного site. `VEHICLE_SPAWN_CANDIDATES_EVALUATED` один раз на bounded attempt/редкий wait-probe агрегирует каждую невражескую базу и её точный результат (`CONTESTED`, inactive spawn, `TOO_FAR`, `NO_EMPTY_TERRAIN`, `NO_BOARDING_SITE_WITHIN_RANGE`, `GROUP_NOT_READY` или `SELECTED`); ожидаемые hostile entries представлены только счётчиком `hostile_skipped`. `SpawnEntityPrefabEx` вызывается единственный раз и только после выбора полностью допустимого кандидата; если живых бойцов уже нет, возвращается retryable `GROUP_NOT_READY` без entity. Сам spawn выполняется только при `Replication.IsServer()`, `campaign.IsMaster()` и точной Fleet-reserved Lease identity; accepted vehicle generation коммитит только `FactionFleet` при bind.

Spawner возвращает `AICF_VehicleAcquisitionFlow` точную причину и признак `retryable`, а flow возвращает только typed `AICF_TripOutcome`. `CONTESTED`, временно неактивная spawn point, отсутствие пустого участка и превышение допустимой дистанции проходят максимум четыре попытки с экспоненциальным backoff от `aicfVehicleRetryIntervalMs` до `aicfVehicleRetryBackoffMaxMs`. После четвёртой попытки TripController переводит request phase в `WAITING_FOR_SITE`; `AICF_VehicleRequestState` сохраняет absolute deadline/attempt evidence, пехотный приказ остаётся активным, Lease отсутствует и active/reserved AI cap не удерживается. Через `aicfVehicleWaitProbeIntervalMs` выполняется новый полный site preflight; найденный допустимый site даёт `VEHICLE_REQUEST_RESUMED wake=ELIGIBLE_SITE_PREFLIGHT`. Для `NO_BOARDING_SITE_WITHIN_RANGE` при нуле spawn-attempt отдельно измеряются nearest safe-base reference, group spread/motion и range trend; если улучшения больше 5 м нет `aicfVehicleNoRangeProgressTimeoutMs` (default 150000 мс), `BOARDING_RANGE_WAIT_EXHAUSTED` завершает только vehicle request и сохраняет действующий пехотный приказ — без Lease, entity, teleport или скрытой мутации. Изменение assignment target или base revision fenced обновляет request context/attempts, не сбрасывая absolute Trip deadline. Invalid prefab, отсутствие faction/AI-usage component и identity/cap bind violation дают typed terminal fail-closed outcome; бесконечный retry запрещён.

Recoverable trip-причины `APPROACH_LOST`, `APPROACH_STALL`, `TIMEOUT_APPROACH`, `VEHICLE_TOO_FAR` и `GROUP_COHESION` не защёлкивают assignment suppression. Группа получает пехотный приказ, а vehicle request переходит в cap-free `WAITING_FOR_SITE` и может проснуться по site probe или изменившемуся strategic context.

У штатной main-base spawn point faction key может оставаться пустым во время ранней инициализации Conflict. Это состояние классифицируется как `SPAWN_FACTION_INITIALIZING`, а не как faction mismatch: `AICF_VehicleAcquisitionFlow` возвращает typed retry/defer outcome и пишет rate-limited `VEHICLE_SPAWN_DEFERRED`; absolute Trip deadline не сбрасывается. После появления faction key действуют обычные проверки владельца и фракции; одинаковое временное состояние не создаёт warning/error churn.

### AI vehicle usage и compartment

Источники: [`SCR_AIVehicleUsageComponent.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Components/SCR_AIVehicleUsageComponent.c), [`SCR_AIGroupUtilityComponent.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Components/SCR_AIGroupUtilityComponent.c), [`SCR_AIUtils.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Utils/SCR_AIUtils.c). Сигнатуры [`SCR_BoardingWaypoint.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Group/SCR_BoardingWaypoint.c) и [`SCR_BoardingEntityWaypoint.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Group/SCR_BoardingEntityWaypoint.c) ниже сохранены только как историческое API evidence отклонённого group-waypoint пути; production BoardingFlow их не создаёт.

```c
PilotCompartmentSlot SCR_AIVehicleUsageComponent.GetPilotCompartmentSlot()
TurretCompartmentSlot SCR_AIVehicleUsageComponent.GetTurretCompartmentSlot()
bool SCR_AIVehicleUsageComponent.CanBePiloted()
bool SCR_AIVehicleUsageComponent.IsVehicleTypeValid()

void SCR_AIGroupUtilityComponent.AddUsableVehicle(
    notnull SCR_AIVehicleUsageComponent vehicleUsageComp)
void SCR_AIGroupUtilityComponent.RemoveUsableVehicle(
    notnull SCR_AIVehicleUsageComponent vehicleUsageComp)
bool SCR_AIGroupUtilityComponent.IsUsableVehicle(
    notnull SCR_AIVehicleUsageComponent vehicleUsageComp)

void SCR_BoardingEntityWaypoint.SetEntity(IEntity entity)
void SCR_BoardingWaypoint.SetAllowance(
    bool driverAllowed,
    bool gunnerAllowed,
    bool cargoAllowed)

void BaseCompartmentSlot.SetReserved(IEntity entity)
void SCR_AIUtilityComponent.AddAction(notnull AIActionBase action)

SCR_AIGetInVehicle(
    SCR_AIUtilityComponent utility,
    SCR_AIActivityBase groupActivity,
    IEntity vehicleEntity,
    BaseCompartmentSlot compartmentSlot,
    EAICompartmentType roleInVehicle,
    float priority,
    float priorityLevel)
```

На обязательных crew-фазах транспорт ещё не добавлен в utility managed-группы: иначе generic vehicle behavior может занять cargo/gunner раньше driver. Он подключается к group utility только после settled mandatory crew и удаляется task handoff при dismount/fallback; utility attachment не является passenger waypoint. Водитель и стрелок определяются по фактическим occupant pilot/turret compartment, а не по предположению о порядке агентов.

До подключения машины к group utility `AICF_VehicleBoardingFlow` измеряет authoritative server distance каждого живого managed member. Если самый удалённый боец дальше `aicfVehicleMaximumReuseDistanceMeters`, flow возвращает typed bounded fallback/wait outcome до выдачи любого GetIn. Exact-site preflight выполняет ту же проверку до создания entity.

Если группа допустима для поездки, но `farthest_m > 75`, каждый удалённый живой участник получает отдельный MOVE-only `SCR_AIMoveIndividuallyBehavior`, нацеленный на фактическую vehicle entity/origin с action radius не больше 70 м. В этой фазе нет group Move-waypoint, vehicle utility и GetIn actions. `AICF_VehicleBoardingFlow` отслеживает owner-token и authoritative distance каждого бойца: сокращение минимум на 2 м считается progress, terminal action или 15 секунд без progress переиздаётся ровно один раз (`BOARDING_APPROACH_REISSUED`), после чего flow возвращает typed bounded outcome. Exact-role посадка начинается только после двух последовательных poll, в которых все живые находятся не дальше 75 м.

Водитель и стрелок назначаются точным `SCR_AIGetInVehicle` на заранее зарезервированный Pilot/Turret slot. Неверно занятые места синхронно нормализуются до создания DRIVER action и повторно после `APPROACH_COMPLETE`. После settled mandatory crew машина может подключиться к group utility, но passenger group waypoint не создаётся. До первой passenger action BoardingFlow атомарно сопоставляет каждому живому пассажиру отдельный exact `CargoCompartmentSlot`, reservation и owner-token, затем выдаёт per-member `SCR_AIGetInVehicle`. План фиксируется один раз: transport включает максимум `APPROACH + DRIVER + PASSENGERS` (3 фазы), armed-light — дополнительно `GUNNER` (4 фазы); уже settled crew и ненужный approach из плана исключаются.

Каждая фактически начатая фаза получает полный `aicfVehicleBoardingTimeoutMs`, а общий предел равен `plannedPhaseCount × timeout`. На всю попытку допускается только один sticky grace `+10 с`, и лишь при target-scoped `IsGettingIn()` либо свежем физическом progress. Повторный poll не продлевает phase/total clock. Для анализа preemption `BOARDING_ACTION_OWNERSHIP` раз в 10 секунд записывает `Type()`/`GetActionState()` текущего utility action каждого живого бойца, состояние group/vehicle waypoint и tracked crew action (`phase`, agent, state, `is_current`). `BOARDING_CREW_ROLE_LOST` делает тот же authoritative snapshot непосредственно перед fallback и защёлкивает acceptance failure. `BOARDING_COMPLETE` возможен после двух последовательных poll, где все живые linked, находятся в compartment, не переходят get-in/get-out и подтверждают `IsInVehicle()`.

Обычная посадка AICF не вызывает teleport-in API. Crew использует exact-role action, каждый пассажир — token-owned `SCR_AIGetInVehicle` на атомарно зарезервированный exact Cargo slot, а дальний staging — только отдельные per-member move actions; group vehicle boarding waypoint и remote GetIn отсутствуют. В normal dismount relocation/teleport запрещён: физический blocker получает bounded per-member movement guidance. Exact eject/relocation наружу допустим только как bounded terminal/fallback fail-closed recovery.

При `SAFE_REUSE` очищаются stale reservations/actions и заново проверяются current-group occupants. Сценарий `mounted > 0 && driver == null` запускает единственный synchronous role reset до DRIVER action; повторное нарушение либо reset timeout создаёт `BOARDING_ROLE_VIOLATION`, acceptance failure и пеший fallback.

Потеря экипажа после начала движения обрабатывается адресно в `AICF_VehicleTransitFlow`: flow выбирает одного живого агента, исключает occupant другой обязательной роли, резервирует конкретный `Pilot`/`Turret` slot и синхронно создаёт `SCR_AIGetInVehicle`. Exact CrewToken хранится в `AICF_VehicleMovementState`; abort/fallback завершает только owned token, а settled-role снимает tracking без `Fail()`. Если одновременно потеряны обе роли, driver восстанавливается первым, затем gunner. Settled полный crew лишь armed pending recovery evidence: ни общий, ни crew success не эмитится до последующего требуемого physical motion либо route progress. Crew attempt не расходует mobility budget и не сбрасывает route/motion timestamps. Асинхронные `SendGetInMessage`/`SendCancelMessage` с `relatedActivity=null` не используются.

### Проверенные waypoint resources

Resource Database и штатный `SCR_ResupplyTaskSolver` подтверждают цепочку посадки/движения/высадки:

```text
{712F4795CF8B91C7}Prefabs/AI/Waypoints/AIWaypoint_GetIn.et
{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et
{C40316EE26846CAB}Prefabs/AI/Waypoints/AIWaypoint_GetOut.et
```

Для привязки vehicle waypoint к дорожной сети дополнительно проверены [`ChimeraAIWorld.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/generated/AI/ChimeraAIWorld.c), [`RoadNetworkManager.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/generated/RoadNetwork/RoadNetworkManager.c) и штатное применение в [`SCR_ResupplyTaskSolver.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/TaskSolver/SCR_ResupplyTaskSolver.c):

```c
RoadNetworkManager SCR_AIWorld.GetRoadNetworkManager()
bool RoadNetworkManager.GetReachableWaypointInRoad(
    vector agentPos,
    vector goalPos,
    float range,
    out vector outPos)
```

`AICF_OrderPlanner` сначала вычисляет тот же тактический target, который получит пехота; для ATTACK это capture point, а не origin базы. Stage 3 проецирует этот target в достижимую дорожную точку через `GetReachableWaypointInRoad()` и пишет `route_mode=ROAD_REACHABLE`; если дорожная точка не найдена, используется диагностируемый direct fallback. `route_distance_m` — прямая оставшаяся дистанция до полученного route endpoint, а не длина рассчитанного дорожного пути. На объезде она может временно расти, поэтому `VEHICLE_PROGRESS` означает только чистое сокращение этой дистанции не менее чем на `aicfVehicleProgressMeters`.

Физическое перемещение машины отслеживается отдельно и пишет `VEHICLE_MOTION`, даже если чистого сокращения до route endpoint пока нет. Высадка, напротив, проверяется не по route endpoint: расстояние сравнивается с исходным тактическим capture point и порогом `aicfVehicleDismountDistanceMeters`. Stage 3 удаляет только собственный transient waypoint: сначала `group.RemoveWaypoint()`, затем `RplComponent.DeleteRplEntity(..., false)`. Если protected managed occupants остаются внутри на половине `aicfVehicleBoardingTimeoutMs`, actions/waypoint очищаются и stock GetOut выдаётся повторно ровно один раз (`DISEMBARK_REISSUED`). На полном deadline отдельный `DISEMBARK_TIMEOUT` защёлкивает acceptance failure и запускает bounded fallback.

Пехотный target сохраняется в stable slot. При любом прекращении vehicle control `AICF_VehicleTaskHandoff` немедленно удаляет owned vehicle utility/waypoint, очищает временные vehicle/individual movement handlers и восстанавливает обычный Search-and-Destroy/Defend/CaptureRelay order для живой current-группы. Успех `order_restored` требует `bound_to_group=1`, `is_current=1`, waypoint в queue и `postcondition_meaningful_task=1`; он не зависит от `clearance_safe`. Logical occupant, get-in/get-out transition, oriented bounds и player protection продолжают проверяться cleanup-контуром и блокируют только lease release/delete. При commander replan после `BASE_OWNER_CHANGED` исправная машина используется через `SAFE_REUSE` либо начинает высадку, если новый target уже близко; для одного retarget не создаётся вторая машина. Ресурс и семантика `CaptureRelay` не изменены.

Восстановленный пехотный приказ сначала пишет `ORDER_RECOVERY_ISSUED`. Событие `ORDER_RECOVERED` разрешено только после трёх последовательных reliability-наблюдений, где exact waypoint одновременно является current и остаётся в queue, и не раньше `max(10 с, 2 × reliability interval)` от первого стабильного наблюдения. `ORDER_RECOVERY_STABILITY` фиксирует длительность и число poll; ранняя потеря кандидата расходует bounded stuck budget. После его исчерпания `GROUP_STUCK_PERSISTENT action=FIELD_HOLD` очищает stale group move handlers и заменяет только failed waypoint на локальный Defend в authoritative позиции живого лидера. Group entity, `group_generation`, target, tickets и достигнутая позиция сохраняются (`GROUP_STUCK_FIELD_HOLD entity_preserved=1 ticket_policy=NONE`). Через один stuck-timeout выполняется bounded retry текущей операции; graph/target revision возобновляет или переназначает её немедленно. Неудачный resume возвращает ту же группу в hold и никогда не вызывает `MarkDestroyed`, `DeleteRplEntity(group)` или MOB replacement.

### Vehicle watchdog

Источники: [`SCR_AIUtils.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Utils/SCR_AIUtils.c), `SCR_AIVehicleUsageComponent`, `SCR_DamageManagerComponent`, `BaseCompartmentManagerComponent`, `CompartmentAccessComponent`.

Watchdog отдельно от пехотного stuck timer проверяет:

- `EDamageState.DESTROYED`;
- `SCR_AIVehicleUsability.VehicleCanMove()` как damage/usable-сигнал и `VehicleIsOnFire()` как отдельное terminal-состояние;
- orientation up-vector для переворота;
- живого occupant pilot/turret compartment;
- фактическое нахождение всех живых членов managed-группы в/вне конкретной машины;
- leader/nearest/farthest distance всех живых managed members до машины перед GetIn;
- физическое перемещение машины и чистое сокращение дистанции до route endpoint;
- дистанцию пеших членов группы до движущейся машины.

В API 1.7 `VehicleCanMove()` не измеряет физическое перемещение: он возвращает только `SCR_DamageManagerComponent.GetMovementDamage() < 1`. Поэтому этот сигнал не вызывает немедленный `VEHICLE_IMMOBILIZED`/fallback. Watchdog держит два независимых deadline:

- `aicfVehicleStuckTimeoutMs` (default `120000`) — нет физического перемещения минимум на `aicfVehicleMotionMeters` (default `3`);
- `aicfVehicleObjectiveProgressTimeoutMs` (default `300000`) — нет чистого сокращения route endpoint минимум на `aicfVehicleProgressMeters` (default `25`), даже если машина движется по объезду.

Crew-role и mobility recovery используют независимые budgets, оба ограниченные `aicfVehicleMaxRecoveries`. Settled driver/gunner лишь armed pending evidence в `AICF_VehicleMovementState`, не изменяет route/motion timestamps и не создаёт success. Последующее физическое движение даёт промежуточный `VEHICLE_MOBILITY_RESTORED`, но `VEHICLE_CREW_RECOVERY_SUCCEEDED` и общий recovery success разрешены только после одновременного physical-motion evidence и чистого route progress.

Истечение physical-motion deadline создаёт `VEHICLE_STUCK_DETECTED` и `VEHICLE_UNSTUCK_STARTED`. Authority может переместить целую машину с уже settled managed occupants не более чем на 15 м, но только если нет foreign occupants, compartment transition и игроков в защитной области; `FindEmptyTerrainPosition` проверяет obstacle/ocean clearance, а отдельный sphere-query отвергает активные pressure-trigger mines и живых персонажей. Если managed occupants временно не settled, пишется `VEHICLE_RECOVERY_DEFERRED ... attempt_consumed=0`: mobility budget не расходуется. Скорость и угловая скорость обнуляются, маршрут перестраивается, а `VEHICLE_UNSTUCK_ATTEMPT` остаётся `evidence=PENDING`: сам reposition не считается успехом. Самостоятельное physical movement создаёт только `VEHICLE_MOBILITY_RESTORED`; `VEHICLE_ROUTE_PROGRESS_CONFIRMED` и общий `VEHICLE_RECOVERY_SUCCEEDED` требуют также чистого сокращения route endpoint. Повторная неподвижность сначала создаёт `VEHICLE_UNSTUCK_FAILED`; лишь после исчерпания реально начатого mobility-budget разрешён `INFANTRY_FALLBACK`. Если безопасная позиция не найдена, выполняется bounded route-rebuild-only attempt без телепортации; damage, fire, overturned и destroyed остаются отдельными terminal-причинами.

Каждый async action, retry, poll, `CallLater` и delete confirmation fenced через `group_generation`, `trip_generation`, `lease_generation`, `vehicle_generation`, `EntityID`/`RplId` и action/reservation token. Stale/ABA callback может только self-cancel и не меняет Trip, Lease, waypoint или entity нового поколения. Terminal transition коммитит только TripController через единый transition path; cleanup job идемпотентно проверяет immutable Fleet/Lease snapshot перед каждым destructive step.

Исправная abandoned-машина больше не удаляется ради replacement capacity. `AICF_VehicleCleanupManager` через `AICF_FactionFleet` освобождает Lease/active AI cap, пишет `VEHICLE_WORLD_POOL_RELEASED ... ai_cap_reserved=0 player_available=1` и передаёт asset в отдельный faction world pool. `aicfVehicleAbandonedWorldPoolPerFaction` (default 4) — safety-first soft target, не hard cap: при превышении пишется `VEHICLE_WORLD_POOL_SOFT_OVERFLOW ... policy=PLAYER_SAFE_DEFERRED_RETIREMENT`, а CleanupManager выбирает oldest-safe retirement; pool временно остаётся больше target при occupant/player/transition/proximity blocker. Destroyed/unusable asset не попадает в functional pool и проходит отдельную destructive retirement quarantine.

Перед любым authority delete или release `AICF_VehicleCleanupManager` сканирует все compartments, managed logical link/get-in/get-out/oriented bounds, target-scoped player transitions и controlled/main entity каждого игрока. Любой blocker сбрасывает stable-clear clock. Clearance-related `FAILED_CLOSED` остаётся cap-held, но получает exact recheck раз в 5 секунд: `PLAYER_POSITION_UNKNOWN` и другие временные fences не превращаются в вечную утечку лимита. После исчезновения blocker нужны непрерывные 5 секунд полного clear, immediate scan и повторная full identity проверка; только затем `FactionFleet.ReleaseRetainedLeaseAt`/`RetireRetainedLeaseAt` атомарно снимает cap. Hard identity/RPL/delete-confirmation failures recheck не получают.

`AICF_VehicleCleanupManager.Stop(cleanupEntities=true)` создаёт deferred cleanup jobs вместо синхронного удаления. One-shot poll раз в секунду планируется только пока остаются jobs, максимум 60 секунд. Каждый callback заново проверяет Trip/Lease/vehicle generations, `EntityID`/`RplId` и action token; stale job self-cancel. После 15-метрового player/transition gate и 5 секунд stable-clear выполняются immediate rescan, identity-safe delete и bounded authority confirmation. Identity mismatch, отсутствие stable-clear или неподтверждённый delete дают `VEHICLE_STOP_CLEANUP_RETAINED ... action=FAIL_CLOSED`: entity сохраняется.

После невосстановимого fallback `AICF_GroupSlot` не хранит vehicle suppression. Authoritative terminal `AICF_TransportTrip` остаётся в `AICF_TransportTripRegistry`, а facade удерживает его для того же current assignment, пока независимый cleanup не завершён; в это время восстановленный infantry order остаётся управляющим, а новый Trip не допускается. Recoverable approach/cohesion/site outcome остаётся внутри того же non-terminal Trip и переводит request phase в cap-free `WAITING_FOR_SITE` без Lease.

Retire terminal Trip и повторный admission разрешены только при одновременно завершённом cleanup и изменившемся current context: проверяются как минимум group generation, assignment revision, target и base revision, а не только пара `group_generation + target`. Каждый facade poll/callback сверяет exact registry Trip identity с immutable assignment snapshot; stale Trip/callback self-cancel и не может подавить replacement-группу либо старой target/base revision перезаписать новую assignment.

Fallback сначала использует normal animated get-out и per-member movement guidance. Только bounded terminal/fail-closed path `AICF_VehicleDismountFlow` может owner-safely exact-eject/relocate `ALIVE` членов current managed-группы; normal path не телепортирует. `INCAPACITATED` и foreign occupants не force-eject-ятся. Flow возвращает typed outcome TripController и пишет единичную per-member terminal telemetry, не вызывает cleanup или handoff напрямую.

Исторически отклонённая реализация hard fallback выставляла `infantry fallback restore pending` и ждала `AreAllProtectedMembersOutOfVehicle()` перед восстановлением пехотного приказа. Именно эта связь оставляла current-группу без meaningful task при `ABANDONED` и больше не является допустимым контрактом.

Новый строгий контракт хранит две независимые обязанности: `order_restored` и `clearance_safe`. Если к абсолютному deadline protected member всё ещё внутри, terminal outcome фиксируется один раз и немедленно прекращает vehicle control; `AICF_VehicleTaskHandoff` сразу восстанавливает и доказывает meaningful infantry order для живой current-группы. Параллельно `AICF_VehicleCleanupManager` продолжает logical/transition/bounds/player clearance. Pending protected participant запрещает lease release/delete до полного safe-clear, но не задерживает infantry order; terminal/`ABANDONED`/`DESTROYED` никогда не владеет движением группы.

Protected occupant — любой `ChimeraCharacter` с life state, отличным от `DEAD`: как `ALIVE`, так и `INCAPACITATED`. Проверка удаления сканирует все compartments машины, поэтому защищены также посторонние occupants другой группы или фракции. Forced exit применяется только к `ALIVE` членам managed-группы; `INCAPACITATED` и foreign occupants не выталкиваются. Пока остаётся любой protected occupant, `RplComponent.DeleteRplEntity()` не вызывается и cleanup остаётся pending; мёртвый occupant сам по себе этот gate не удерживает.

`AICF_VehicleAcceptanceMonitor` хранит completion/failure evidence отдельно для каждого configured transport/armed slot обеих фракций и не создаёт mid-run PASS. Когда завершены все настроенные первые поездки и ещё нет Stage 3 error/acceptance failure, monitor пишет только `RESULT_CANDIDATE status=READY ... final=0 requires_log_review=1`. Typed Trip failures и late cleanup failure защёлкивают cumulative `ACCEPTANCE_FAILURE_LATCHED`; первый поздний дефект после READY дополнительно пишет `RESULT_CANDIDATE status=INVALIDATED`. Кандидат не останавливает gameplay и не скрывает повторные поездки.

При `VehicleCoordinator.Stop()` thin facade запрашивает остановку controller/cleanup/acceptance owners; `[AICF][STAGE3][RESULT]` от `AICF_VehicleAcceptanceMonitor` остаётся финальным отрицательным событием для незавершённой автоматической приёмки. Автоматического финального PASS нет; его заменяет review полного остановленного server/client log и ручной матрицы.

### Исторические ограничения отклонённой реализации Stage 3

1. `RESULT_CANDIDATE status=READY final=0` доказывает только завершение настроенного набора поездок до этой точки. Это не PASS; поздняя acceptance failure инвалидирует кандидата.
2. Transport T7, Transport T8, Transport T9 и Armed A1 остаются историческими runtime-срезами с итогом FAIL. Post‑T9 изменения прошли static audit и Workbench validation по пяти конфигурациям, но dedicated runtime T10/A2 ещё не выполнялся.
3. Штатные мины Arland не удаляются и не меняются. `VEHICLE_ON_FIRE`/уничтожение на мине остаётся валидной проверкой fallback и диагностируется в runtime, но само по себе не защёлкивает acceptance failure: без provenance нельзя отличить штатный бой или мину от дефекта транспортного кода.
4. Наземная проходимость определяется stock AI Move/navmesh. Radio graph не является дорожным графом; bounded watchdog обязан перейти к пешему fallback при недостижимом маршруте.
5. Overflow-policy — `ALL_OR_FALLBACK`: если всем живым членам группы не хватает compartment, частично уехавшая группа запрещена.
6. Исправная abandoned entity освобождает active AI cap, но учитывается в отдельном faction world pool с soft target 4. Pool разрешено временно превысить target: protected occupant, player transition или живой игрок в радиусе 15 м важнее лимита и блокирует destructive cleanup; после освобождения требуется ещё 5 секунд непрерывного clear.
7. В T8 встречались внешние/versioned сообщения до или вне AICF init: server resource load `WORLD (E): Unknown keyword/data 'm_bEnabled'`, `DEFAULT (E): Unknown keyword/data 'm_bCanAIMarkTargets'` и client stock UI `ScriptInvoker::Invoke: Incompatible parameter ... ShowFactionPlayerList`. Символы отсутствуют в репозитории, а stack/resource context относится к stock `GameMode_Campaign`/RNGD и `SCR_RoleSelectionMenu`; они не классифицируются как AICF `SCRIPT (E/F)`, но сохраняются в отчёте и не могут использоваться как доказательство чистого окружения.
8. Автоматического финального runtime PASS нет. Решением владельца от 15.08.2026 rewrite implementation/cutover Stage 3/3.5 приняты как продуктовая базовая линия и Stage 4 разблокирован. Техническая матрица новой архитектуры по-прежнему требует полного остановленного server/client log, Transport T10, Armed A2, отдельных retry/approach/order/cap/cleanup fault-срезов и 30-минутного прогона; до выполнения они остаются `NOT RUN`, не отменяя продуктовую приёмку.

## Stage 4: stock supply API и серверная экономика

Production-контракт проверен по локальному Script Diff Reforger `1.8.0.10` в `.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.10/scripts/Game/Components/Locations/SCR_CampaignMilitaryBaseComponent.c`:

```c
float GetSupplies();
float GetSuppliesMax();
void AddSupplies(int suppliesCount, bool replicate = true);
void SetSupplies(float suppliesCount);
SCR_ECampaignBaseType GetType();
```

Stage 4 не создаёт параллельную валюту: reservation, rollback, shipment dispatch, delivery и return используют только `SCR_CampaignMilitaryBaseComponent.AddSupplies()`. Это сохраняет общий stock pool с player deliveries, building/repair/respawn расходами и штатной репликацией Conflict.

`AICF_ObjectiveGraph` теперь предоставляет revision, общий hop distance и friendly BFS path. `AICF_SupplyNetwork` дополнительно проверяет ownership, initialized/capture/enemy state каждого узла пути. Source определяется как faction main base или `SCR_ECampaignBaseType.SOURCE_BASE`.

Агрегаты Stage 4 добавлены в уже реплицируемый `SCR_GameModeCampaign` через `RplProp` + `Replication.BumpMe()`: total/connected supplies, logistics tier, pending reinforcement requests и shipments in transit отдельно для US/USSR. Конкретный stock каждой базы остаётся собственностью vanilla replication.

Workbench 1.8 validation `.cache/stage4-implementation-validate-20260815-r5/console.log`: Game `5729 files / 11223 classes`, CRC32 `307e40e6`, `Game successfully created`, `SCRIPT(E/F)=0`, `ENGINE(F)=0`; shutdown resource-leak list относится к стандартному Workbench harness.

Direct ServerDiag calibration probe `C:\Users\retar\AppData\Local\AICF\Stage4-Probe-20260815-115400\logs\logs_2026-08-15_11-54-11\console.log` дошёл до AICF bootstrap и `ROSTER_READY`, записал девять отложенных `SUPPLY_PROBE` после инициализации обеих сторон и прошёл `tools/Test-Stage4Log.ps1`. Faction MOB имели `1000/1000`; стандартные non-relay базы — `supplies_max=1000`, более крупные pools — `2150/3000`. На этом evidence defaults откалиброваны как group cost `500`, delivery package `500`, source reserve `500`. Это `E1 PASS`, а не полный runtime PASS Stage 4.
