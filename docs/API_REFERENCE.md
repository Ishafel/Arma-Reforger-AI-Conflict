# Проверенные API Arma Reforger для этапов 0–3

## Зафиксированная версия

- Arma Reforger / Reforger Tools / ServerDiag: 1.7.0.54.
- Официальный репозиторий: [BohemiaInteractive/Arma-Reforger-Script-Diff](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/tree/v1.7.0.54).
- Проверенный commit: [`2735631ce1400eaf9f1761c66cdee10c46921d37`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/tree/2735631ce1400eaf9f1761c66cdee10c46921d37).
- SHA-256 tag-архива: `005f312180cdf48cfc98723bcfa3f89eee838da1571d9050d609c3657fea7164`.

Script Diff — снимок скриптов конкретной версии игры, а не обещание вечной совместимости. После обновления Reforger этот документ и каждую используемую сигнатуру нужно сверить повторно.

Исторический Stage 0 был подтверждён ручным runtime-тестом с итогом `[AICF][STAGE0][RESULT][PASS]`. Для текущей Stage 1-ветки дополнительно выполнены Workbench `Validate Scripts` и direct `ArmaReforgerServerDiag.exe` smoke-запуск штатного Arland: созданы `4 × US` и `4 × USSR`, подтверждены роли `2 ATTACK / 1 DEFEND / 1 RESERVE` на сторону и начальные приказы. Это ещё не полная A/B-приёмка Stage 1.

## Локальный кэш

```bash
./tools/fetch_reforger_api_reference.sh
```

Скрипт загружает официальный tag-архив в:

```text
.cache/reforger-api/Arma-Reforger-Script-Diff-1.7.0.54/
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
| Stage 3 lifecycle | `aicfVehiclesEnabled`, `aicfTransportVehiclesPerFaction`, `aicfArmedLightVehiclesPerFaction`, `aicfMaxVehiclesPerFaction`, `aicfVehicleBoardingTimeoutMs`, `aicfVehicleMaxRecoveries`, `aicfVehicleDismountDistanceMeters`, `aicfVehicleRetryIntervalMs`, `aicfVehicleCleanupDelayMs` |
| Stage 3 movement | `aicfVehicleStuckTimeoutMs`, `aicfVehicleProgressMeters`, `aicfVehicleMotionMeters`, `aicfVehicleObjectiveProgressTimeoutMs`, `aicfVehicleMinimumRouteMeters`, `aicfVehicleMaximumReuseDistanceMeters`, `aicfVehicleMaximumSpawnDistanceMeters`, `aicfVehicleCohesionDistanceMeters` |

Числа после `ToInt()` ограничиваются безопасными min/max, а ожидаемая сторона принимает только `US` или `USSR`. Нижняя граница `max_managed_agents` равна 32: она покрывает обязательный стартовый roster и консервативный budget одной создаваемой replacement-группы, поэтому заниженный CLI-параметр не может навсегда заблокировать lifecycle. Direct ServerDiag smoke подтвердил применение ускоренных значений `initial_tickets=1`, `commander_interval_ms=5000`, `replacement_delay_ms=30000`, `max_managed_agents=64` и `expected_player_faction=US` в событии `CONFIG`.

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

Собственные 16-hex GUID приняты Resource Database. Текущие `AIConflictCore` и `AIConflictArland` прошли Workbench validation на 1.7.0.54; direct ServerDiag загрузил оба проекта и выполнил Stage 1 smoke до `ROSTER_READY` с восемью готовыми группами.

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

Базовые API и ресурсы ниже привязаны к локальному official Script Diff `v1.7.0.54` и установленной Resource Database. Текущий post-T8 snapshot прошёл `tools/Test-Stage3Static.ps1` и Workbench 1.7 `Validate Scripts` по пяти конфигурациям: `.cache/stage3-t8-fixes-final5-20260809-191200/console.log`, Game CRC32 `013129aa`, `Script validation successful`, `SCRIPT (E/F)=0`. Это только compile/static evidence: dedicated runtime T9/A2 ещё не выполнялся, а Transport T7/T8 и Armed A1 остаются историческими `FAIL`.

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

До геометрического query Stage 3 повторно применяет `AICF_ConflictAdapter.GetSpawnRejectionReason`: base существует и initialized, принадлежит нужной стороне, не contested/capturing, enemies отсутствуют, spawn point enabled/active и имеет правильный faction key. Все safe base сортируются детерминированно по расстоянию и `BaseKey`; затем каждая по очереди проходит максимальную spawn-дистанцию, `FindEmptyTerrainPosition` и измерение расстояния всех живых managed members до найденного точного site. `SpawnEntityPrefabEx` вызывается единственный раз и только после выбора полностью допустимого кандидата; если живых бойцов уже нет, возвращается retryable `GROUP_NOT_READY` без entity. Сам spawn выполняется только при `Replication.IsServer()`, `campaign.IsMaster()` и заранее занятом slot/cap runtime.

Spawner возвращает координатору точную причину и признак `retryable`. `CONTESTED`, временно неактивная spawn point, отсутствие пустого участка и превышение допустимой дистанции проходят максимум четыре попытки с экспоненциальным backoff от `aicfVehicleRetryIntervalMs` до `aicfVehicleRetryBackoffMaxMs`. После четвёртой попытки runtime переходит в `WAITING_FOR_SITE`: пехотный приказ остаётся активным, запрос не учитывается в active/reserved AI cap, а одинаковый отказ не создаёт 10-секундный state/log churn. Через `aicfVehicleWaitProbeIntervalMs` выполняется новый полный site preflight; найденный допустимый site даёт `VEHICLE_REQUEST_RESUMED wake=ELIGIBLE_SITE_PREFLIGHT`. Изменение target или ревизии Conflict-баз немедленно сбрасывает request generation/attempts через `VEHICLE_REQUEST_CONTEXT_CHANGED`. Invalid prefab, отсутствие faction/AI-usage component, неудачное назначение faction и невозможность создать entity считаются terminal для текущего поколения группы. Они не преобразуются в `NO_SAFE_SPAWN_AVAILABLE` и не запускают бесконечный retry; новый шанс появляется только после нового `group_generation`.

Recoverable trip-причины `APPROACH_LOST`, `APPROACH_STALL`, `TIMEOUT_APPROACH`, `VEHICLE_TOO_FAR` и `GROUP_COHESION` не защёлкивают assignment suppression. Группа получает пехотный приказ, а vehicle request переходит в cap-free `WAITING_FOR_SITE` и может проснуться по site probe или изменившемуся strategic context.

У штатной main-base spawn point faction key может оставаться пустым во время ранней инициализации Conflict. Это состояние классифицируется как `SPAWN_FACTION_INITIALIZING`, а не как faction mismatch: координатор пишет информационный `VEHICLE_SPAWN_DEFERRED` и выполняет короткий повтор через 1 секунду. После появления faction key действуют обычные проверки владельца и фракции; одинаковое временное состояние не создаёт warning/error churn.

### AI vehicle usage и compartment

Источники: [`SCR_AIVehicleUsageComponent.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Components/SCR_AIVehicleUsageComponent.c), [`SCR_AIGroupUtilityComponent.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Components/SCR_AIGroupUtilityComponent.c), [`SCR_BoardingWaypoint.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Group/SCR_BoardingWaypoint.c), [`SCR_BoardingEntityWaypoint.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Group/SCR_BoardingEntityWaypoint.c), [`SCR_AIUtils.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Utils/SCR_AIUtils.c).

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

На обязательных crew-фазах транспорт ещё не добавлен в utility managed-группы: иначе generic vehicle behavior может занять cargo/gunner раньше driver. Он подключается к group utility только перед пассажирской фазой и удаляется на dismount/fallback/cleanup. Водитель и стрелок определяются по фактическим occupant pilot/turret compartment, а не по предположению о порядке агентов.

До подключения машины к group utility координатор измеряет authoritative server distance каждого живого managed member. Если самый удалённый боец дальше `aicfVehicleMaximumReuseDistanceMeters`, `BOARDING_REJECTED reason=VEHICLE_TOO_FAR` переводит группу в пеший fallback до выдачи любого GetIn. Exact-site preflight выполняет ту же проверку до создания entity.

Если группа допустима для поездки, но `farthest_m > 75`, каждый удалённый живой участник получает отдельный MOVE-only `SCR_AIMoveIndividuallyBehavior`, нацеленный на фактическую vehicle entity/origin с action radius не больше 70 м. В этой фазе нет group Move-waypoint, vehicle utility и GetIn actions. Координатор отслеживает action-token и authoritative distance каждого бойца: сокращение минимум на 2 м считается progress, terminal action или 15 секунд без progress переиздаётся ровно один раз (`BOARDING_APPROACH_REISSUED`), после чего `BOARDING_APPROACH_MEMBER_STALLED` ограниченно завершает попытку. Exact-role посадка начинается только после двух последовательных poll, в которых все живые находятся не дальше 75 м.

Водитель и стрелок назначаются точным `SCR_AIGetInVehicle` на заранее зарезервированный Pilot/Turret slot. Неверно занятые места синхронно нормализуются до создания DRIVER action и повторно после `APPROACH_COMPLETE`. Затем машина подключается к group utility и получает групповой `CARGO_ONLY` waypoint. План фиксируется один раз: transport включает максимум `APPROACH + DRIVER + PASSENGERS` (3 фазы), armed-light — дополнительно `GUNNER` (4 фазы); уже settled crew и ненужный approach из плана исключаются.

Каждая фактически начатая фаза получает полный `aicfVehicleBoardingTimeoutMs`, а общий предел равен `plannedPhaseCount × timeout`. На всю попытку допускается только один sticky grace `+10 с`, и лишь при target-scoped `IsGettingIn()` либо свежем физическом progress. Повторный poll не продлевает phase/total clock. `BOARDING_COMPLETE` возможен после двух последовательных poll, где все живые linked, находятся в compartment, не переходят get-in/get-out и подтверждают `IsInVehicle()`.

Обычная посадка AICF не вызывает teleport-in API. Crew использует exact-role action, cargo — stock `SCR_BoardingWaypoint`, а дальний staging — только отдельные per-member move actions; удалённый GetIn не выдаётся. Teleport применяется только наружу как bounded physical-clearance recovery/forced exit.

При `SAFE_REUSE` очищаются stale reservations/actions и заново проверяются current-group occupants. Сценарий `mounted > 0 && driver == null` запускает единственный synchronous role reset до DRIVER action; повторное нарушение либо reset timeout создаёт `BOARDING_ROLE_VIOLATION`, acceptance failure и пеший fallback.

Потеря экипажа после начала движения обрабатывается адресно: координатор выбирает одного живого агента, исключает occupant другой обязательной роли, резервирует конкретный `Pilot`/`Turret` slot и синхронно создаёт штатный `SCR_AIGetInVehicle`. Точная ссылка на этот action-token сохраняется в runtime: при abort/fallback завершается только сохранённый token, без поиска и риска отменить чужой get-in. После settled-занятия нужного места tracking снимается без `Fail()`: штатный `OnActionFailed()` мог бы высадить только что восстановленного водителя/стрелка. Если одновременно потеряны обе роли, driver восстанавливается первым, затем gunner; success допустим только после повторной проверки полного crew и пишет `ALL_REQUIRED_CREW_RESTORED`. Асинхронные `SendGetInMessage`/`SendCancelMessage` с `relatedActivity=null` в recovery-контракте не используются.

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

Пехотный target сохраняется в stable slot; после высадки или отказа `AICF_OrderPlanner` восстанавливает обычный Search-and-Destroy/Defend/CaptureRelay order. Перед этим `AICF_GroupCohesionPolicy.NormalizeAfterVehicle()` вызывает `ClearGroupMoveHandlers()` и повторно применяет stock `Column`/default movement handler, чтобы временные vehicle/individual handlers не блокировали дальнейшее пехотное движение. Нормализация выполняется только после подтверждённой высадки, fallback или guarded physical-clearance path и не телепортирует бойцов. При commander replan после `BASE_OWNER_CHANGED` moving-runtime сравнивается с новым target стабильного слота: существующая машина получает новый road waypoint (`STRATEGIC_TARGET_CHANGED`) либо начинает высадку, если новый target уже в радиусе. Для одного retarget не создаётся вторая машина. Ресурс и семантика `CaptureRelay` не изменены.

Восстановленный пехотный приказ сначала пишет `ORDER_RECOVERY_ISSUED`. Событие `ORDER_RECOVERED` разрешено только после трёх последовательных reliability-наблюдений, где exact waypoint одновременно является current и остаётся в queue, и не раньше `max(10 с, 2 × reliability interval)` от первого стабильного наблюдения. `ORDER_RECOVERY_STABILITY` фиксирует длительность и число poll; ранняя потеря кандидата расходует bounded stuck budget и в пределе приводит к recycle вместо бесконечного churn.

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

Истечение любого deadline создаёт `VEHICLE_STUCK_DETECTED` и проходит один bounded recovery budget. Критерий успеха зависит от причины: recovery после физической неподвижности может подтвердить новое физическое движение (или route progress), а recovery после отсутствия objective progress требует именно сокращения дистанции до route endpoint; движение в сторону без такого сокращения его не завершает. Если damage-сигнал уже запрещает движение, причинная последовательность заканчивается `VEHICLE_RECOVERY_FAILED reason=VEHICLE_RECOVERY_MOBILITY_UNAVAILABLE → INFANTRY_FALLBACK`. Проверка budget выполняется до `VEHICLE_RECOVERY_STARTED`. Destroyed, fire и overturned остаются отдельными terminal-причинами.

Vehicle runtime не регистрирует `CallLater` или ScriptInvoker callback. Каждый poll сверяет stable slot, identity группы, `group_generation`, runtime reference и отдельный `vehicle_generation`. Поэтому replacement-группа не может получить stale vehicle state предыдущего поколения. Переход в `ABANDONED`/`DESTROYED` является edge-событием: повторный poll не меняет terminal reason, state timestamp или cleanup deadline. `BeginDetachedCleanup()` и terminal processing идемпотентны; при очистке array-entry зануляется только если он всё ещё содержит тот же runtime, поэтому старое поколение не может удалить новое.

Исправная abandoned-машина больше не удаляется ради replacement capacity. Координатор всегда немедленно снимает её с managed group/runtime и active AI cap, пишет `VEHICLE_WORLD_POOL_RELEASED ... ai_cap_reserved=0 player_available=1` и оставляет в отдельном faction world pool. `aicfVehicleAbandonedWorldPoolPerFaction` (default 4) — safety-first soft target, не hard cap: при превышении пишется `VEHICLE_WORLD_POOL_SOFT_OVERFLOW pool_size=<n> pool_limit=4 retirement_candidates=<n> policy=PLAYER_SAFE_DEFERRED_RETIREMENT`, координатор помечает/ищет oldest безопасно удаляемый entry, но pool может временно оставаться больше target, пока player/interaction/proximity gate блокирует retirement. В destructive cleanup попадают только destroyed/on-fire/overturned/неподвижные машины либо явно выбранный безопасный retirement-entry; usable world-pool vehicle остаётся доступным игрокам.

Перед любым authority delete watchdog сканирует все compartments, target-scoped get-in/get-out transitions и живых player-controlled characters, связанных с машиной или находящихся не дальше 15 м. Любой occupant, transition или nearby player сбрасывает stable-clear clock и при длительном ожидании пишет `VEHICLE_CLEANUP_DEFERRED` с samples. Удаление разрешено только после непрерывных 5 секунд полного clear и повторного scan непосредственно перед destructive call. Это консервативный server-side gate, а не гарантированная engine interaction reservation; T9/A2 обязаны повторить race посадки игрока.

`Stop(cleanupEntities=true)` переносит активные и world-pool runtime в отдельную deferred stop-cleanup очередь вместо синхронного удаления. `CallLater(..., repeat=false)` выполняет one-shot poll раз в секунду и явно планирует следующий только пока остаются записи. В течение 60 секунд каждый runtime обязан получить тот же 15-метровый player/transition gate и 5 секунд stable-clear, после чего delete использует прежний `EntityID`/`RplId` identity guard и authority confirmation. `VEHICLE_STOP_CLEANUP_STARTED` открывает окно; `VEHICLE_STOP_CLEANUP_CONFIRMED` завершает подтверждённое удаление. Identity mismatch, отсутствие stable-clear к 60 секундам или неподтверждённый delete дают `VEHICLE_STOP_CLEANUP_RETAINED ... action=FAIL_CLOSED`: entity сохраняется, replacement с совпавшим ID не удаляется.

После невосстановимого fallback stable slot запоминает пару `group_generation + target`. Пока жива та же группа и назначен тот же objective, новая vehicle trip подавлена и восстановленный пехотный приказ остаётся управляющим. Recoverable approach/cohesion/site причины перечислены выше и обходят этот latch: они переводят запрос в `WAITING_FOR_SITE` без suppression. Новая группа или действительно новый target также могут запросить транспорт снова.

Suppression записывается только если `IsRuntimeCurrent(runtime, slot)` подтверждает текущую identity группы и `group_generation`. В latch передаётся актуальный `slot.GetTargetBase()` (runtime target используется только как fallback при временно пустом slot target). Поэтому terminal poll stale generation не может подавить transport для replacement-группы или старой целью перезаписать уже изменившуюся assignment.

Fallback сначала выдаёт штатный animated get-out waypoint. После bounded boarding deadline координатор может прервать vehicle action queue и принудительно высадить только управляемых членов текущей группы в состоянии `ECharacterLifeState.ALIVE`: через `FindSuitableTeleportLocation()` + `GetOutVehicle_NoDoor()`, с `GetOutVehicle(EGetOutType.TELEPORT, ...)` как запасным путём. `INCAPACITATED` не force-eject-ится. Успешная принудительная высадка отмечается единичным `FALLBACK_FORCE_DISEMBARK`.

Hard fallback конечен. Если к абсолютному deadline `2 × aicfVehicleBoardingTimeoutMs` в машине остаётся protected member текущей группы, `FALLBACK_DISEMBARK_FAILED` пишется один раз, исходная terminal cause сохраняется и дополняется `+FALLBACK_DISEMBARK_FAILED`, vehicle runtime переводится в `ABANDONED`, а slot получает terminal failure и suppression для текущей assignment. Одновременно выставляется `infantry fallback restore pending`: пехотный приказ в этой ветке не восстанавливается немедленно. Последующие terminal poll восстанавливают его только после `AreAllProtectedMembersOutOfVehicle()` и только если runtime всё ещё принадлежит текущему group generation. Таким образом hard failure не зацикливает fallback и не выдаёт пеший waypoint бойцу, который ещё находится в compartment.

Protected occupant — любой `ChimeraCharacter` с life state, отличным от `DEAD`: как `ALIVE`, так и `INCAPACITATED`. Проверка удаления сканирует все compartments машины, поэтому защищены также посторонние occupants другой группы или фракции. Forced exit применяется только к `ALIVE` членам managed-группы; `INCAPACITATED` и foreign occupants не выталкиваются. Пока остаётся любой protected occupant, `RplComponent.DeleteRplEntity()` не вызывается и cleanup остаётся pending; мёртвый occupant сам по себе этот gate не удерживает.

Завершение хранится отдельно для каждого configured transport/armed slot обеих фракций, но больше не создаёт mid-run PASS. Когда завершены все настроенные первые поездки и ещё нет Stage 3 error/acceptance failure, координатор пишет только `RESULT_CANDIDATE status=READY ... final=0 requires_log_review=1`. `BOARDING_TIMEOUT`, `DISEMBARK_TIMEOUT` и `BOARDING_ROLE_VIOLATION` защёлкивают cumulative `ACCEPTANCE_FAILURE_LATCHED`; первый поздний дефект после READY дополнительно пишет `RESULT_CANDIDATE status=INVALIDATED`. Кандидат не останавливает gameplay и не скрывает повторные поездки.

При остановке координатора `[AICF][STAGE3][RESULT]` остаётся финальным отрицательным событием для незавершённой автоматической приёмки: причины различают `ACCEPTANCE_FAILURE_*`, `STAGE3_ERRORS`, `READY_NOT_FINALIZED` и `STOPPED_BEFORE_ACCEPTANCE`. Автоматического финального PASS нет; его заменяет review полного остановленного server/client log и ручной матрицы.

### Ограничения Stage 3-кандидата

1. `RESULT_CANDIDATE status=READY final=0` доказывает только завершение настроенного набора поездок до этой точки. Это не PASS; поздняя acceptance failure инвалидирует кандидата.
2. Transport T7, Transport T8 и Armed A1 остаются историческими runtime-срезами с итогом FAIL. Post‑T8 изменения прошли static audit и Workbench validation по пяти конфигурациям, но dedicated runtime T9/A2 ещё не выполнялся.
3. Штатные мины Arland не удаляются и не меняются. `VEHICLE_ON_FIRE`/уничтожение на мине остаётся валидной проверкой fallback, но может заблокировать конкретный happy-path.
4. Наземная проходимость определяется stock AI Move/navmesh. Radio graph не является дорожным графом; bounded watchdog обязан перейти к пешему fallback при недостижимом маршруте.
5. Overflow-policy — `ALL_OR_FALLBACK`: если всем живым членам группы не хватает compartment, частично уехавшая группа запрещена.
6. Исправная abandoned entity освобождает active AI cap, но учитывается в отдельном faction world pool с soft target 4. Pool разрешено временно превысить target: protected occupant, player transition или живой игрок в радиусе 15 м важнее лимита и блокирует destructive cleanup; после освобождения требуется ещё 5 секунд непрерывного clear.
7. В T8 встречались внешние/versioned сообщения до или вне AICF init: server resource load `WORLD (E): Unknown keyword/data 'm_bEnabled'`, `DEFAULT (E): Unknown keyword/data 'm_bCanAIMarkTargets'` и client stock UI `ScriptInvoker::Invoke: Incompatible parameter ... ShowFactionPlayerList`. Символы отсутствуют в репозитории, а stack/resource context относится к stock `GameMode_Campaign`/RNGD и `SCR_RoleSelectionMenu`; они не классифицируются как AICF `SCRIPT (E/F)`, но сохраняются в отчёте и не могут использоваться как доказательство чистого окружения.
8. Автоматического финального PASS нет. Приёмка требует полного остановленного server/client log, ручной матрицы, Transport T9, Armed A2, отдельных retry/approach/order/cap/cleanup fault-срезов и 30-минутного прогона.
