# Проверенные API Arma Reforger для этапов 0 и 1

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

Имя передаётся без ведущего `-`; возвращаемый `bool` отличает отсутствующий параметр от присутствующего значения. Stage 1 читает:

```text
-aicfInitialTickets
-aicfReplacementTicketCost
-aicfReinforcementDelayMs
-aicfCommanderIntervalMs
-aicfMaxManagedAgents
-aicfExpectedPlayerFaction
```

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

Ниже перечислены только API и ресурсы, проверенные по локальному official Script Diff `v1.7.0.54`, установленной Resource Database и успешному Workbench validation. Runtime-поведение посадки, вождения и navmesh всё равно требует матрицы из `STAGE_3_TESTING.md`.

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

До геометрического query Stage 3 повторно применяет `AICF_ConflictAdapter.GetSpawnRejectionReason`: base существует и initialized, принадлежит нужной стороне, не contested/capturing, enemies отсутствуют, spawn point enabled/active и имеет правильный faction key. Дополнительно safe base должна находиться не дальше `aicfVehicleMaximumSpawnDistanceMeters` от assigned group. Сам spawn выполняется только при `Replication.IsServer()`, `campaign.IsMaster()` и заранее занятом slot/cap runtime.

### AI vehicle usage и compartment

Источники: [`SCR_AIVehicleUsageComponent.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Components/SCR_AIVehicleUsageComponent.c), [`SCR_AIGroupUtilityComponent.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Components/SCR_AIGroupUtilityComponent.c), [`SCR_BoardingWaypoint.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Group/SCR_BoardingWaypoint.c), [`SCR_BoardingEntityWaypoint.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Group/SCR_BoardingEntityWaypoint.c).

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
```

Перед посадкой транспорт добавляется в utility конкретной managed-группы. На dismount/fallback/cleanup он удаляется оттуда. Водитель и стрелок определяются по фактическим occupant pilot/turret compartment, а не по предположению о порядке агентов.

### Проверенные waypoint resources

Resource Database и штатный `SCR_ResupplyTaskSolver` подтверждают цепочку посадки/движения/высадки:

```text
{712F4795CF8B91C7}Prefabs/AI/Waypoints/AIWaypoint_GetIn.et
{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et
{C40316EE26846CAB}Prefabs/AI/Waypoints/AIWaypoint_GetOut.et
```

Stage 3 удаляет только собственный transient waypoint: сначала `group.RemoveWaypoint()`, затем `RplComponent.DeleteRplEntity(..., false)`. Пехотный target сохраняется в stable slot; после высадки или отказа `AICF_OrderPlanner` восстанавливает обычный Search-and-Destroy/Defend/CaptureRelay order. Ресурс и семантика `CaptureRelay` не изменены.

### Vehicle watchdog

Источники: [`SCR_AIVehicleUsability.c`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/v1.7.0.54/scripts/Game/AI/Utils/SCR_AIVehicleUsability.c), `SCR_AIVehicleUsageComponent`, `BaseCompartmentManagerComponent`, `CompartmentAccessComponent`.

Watchdog отдельно от пехотного stuck timer проверяет:

- `EDamageState.DESTROYED`;
- `SCR_AIVehicleUsability.VehicleCanMove()` и `VehicleIsOnFire()`;
- orientation up-vector для переворота;
- живого occupant pilot/turret compartment;
- фактическое нахождение всех живых членов managed-группы в/вне конкретной машины;
- сокращение остатка vehicle route на настроенную величину;
- дистанцию пеших членов группы до движущейся машины.

Vehicle runtime не регистрирует `CallLater` или ScriptInvoker callback. Каждый poll сверяет stable slot, identity группы, `group_generation`, runtime reference и отдельный `vehicle_generation`. Поэтому replacement-группа не может получить stale vehicle state предыдущего поколения.

### Ограничения Stage 3-кандидата

1. Автоматический `RESULT status=PASS` доказывает только завершение configured поездок обеими сторонами и отсутствие Stage 3 errors. Он не доказывает recovery/fault/limit матрицу.
2. Наземная проходимость определяется штатным AI Move-waypoint и navmesh runtime. Radio graph не является vehicle road graph; watchdog обязан завершить непроходимый маршрут пешим fallback.
3. Armed-light машина используется как транспорт одного ATTACK-slot и не создаёт отдельную бессрочную боевую группу. После высадки исходная пехота продолжает objective.
4. Overflow-policy — `ALL_OR_FALLBACK`: если всем живым членам группы не хватает доступных compartment, частично уехавшая группа не допускается.
5. Abandoned/destroyed entity учитывается в faction cap до cleanup. Entity с живым occupant не удаляется; после освобождения cleanup может завершиться.
6. Workbench validation выполнен без запуска server/client. Фактические boarding, driving, dismount, driver/gunner replacement и cleanup ещё не приняты runtime-тестом.
