# Проверенные API Arma Reforger для этапа 0

## Зафиксированная версия

- Arma Reforger / Reforger Tools: 1.7.0.54.
- Официальный репозиторий: [BohemiaInteractive/Arma-Reforger-Script-Diff](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/tree/v1.7.0.54).
- Проверенный commit: [`2735631ce1400eaf9f1761c66cdee10c46921d37`](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/tree/2735631ce1400eaf9f1761c66cdee10c46921d37).
- SHA-256 tag-архива: `005f312180cdf48cfc98723bcfa3f89eee838da1571d9050d609c3657fea7164`.

Script Diff — снимок скриптов конкретной версии игры, а не обещание вечной совместимости. После обновления Reforger этот документ и каждую используемую сигнатуру нужно сверить повторно.

## Локальный кэш

```bash
./tools/fetch_reforger_api_reference.sh
```

Скрипт загружает официальный tag-архив в:

```text
.cache/reforger-api/Arma-Reforger-Script-Diff-1.7.0.54/
```

Кэш исключён из Git, не включается в addon и не является сторонним модом. Скрипт не скачивает данные повторно, если целевой snapshot уже присутствует.

## Подтверждённые контракты

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

Если defender prefab отключает immediate spawn, код явно вызывает `SpawnUnits()`. Затем до 30 секунд проверяет наличие бойцов и назначенного waypoint. Полноценный lifecycle/respawn группы не входит в Stage 0.

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

`CallLater(..., 0, false)` имеет штатные примеры в Script Diff. `DeleteRplEntity(..., false)` применяется только к сущностям, созданным текущим проходом Stage 0, если последующий шаг завершился ошибкой.

### Arland и `.gproj`

Штатный scenario ID из [Server Config](https://community.bistudio.com/wiki/Arma_Reforger%3AServer_Config):

```text
{C41618FD18E9D714}Missions/23_Campaign_Arland.conf
```

Формат scripts-only проекта и GUID штатной зависимости `58D0FB3206B6F859` подтверждены официальным [`SampleMod_ModdedScript.gproj`](https://github.com/BohemiaInteractive/Arma-Reforger-Samples/blob/main/SampleMod_ModdedScript/SampleMod_ModdedScript.gproj). Путь `Scripts/Game` подтверждён [официальным sample](https://github.com/BohemiaInteractive/Arma-Reforger-Samples/tree/main/SampleMod_ModdedScript/Scripts/Game).

Собственные 16-hex GUID выделены проектам, но их принятие Resource Database и фактическая компиляция не могли быть проверены без Reforger Tools.

## Зафиксированные ограничения решений

1. `AIConflictArland` использует минимально инвазивный `modded SCR_GameModeCampaign`; при загрузке этого addon он расширяет любой Conflict, поэтому тестовый запуск ограничен Arland инструкцией, а не непроверенным scenario-detection API.
2. `SCR_CampaignMilitaryBaseManager.GetBases()` не даёт relay; полный набор берётся из `SCR_MilitaryBaseSystem`.
3. Транзит разрешён только через узлы, уже принадлежащие выбирающей фракции, включая её `RELAY`. Вражеская или нейтральная objective-base может быть конечной целью, но не транзитной территорией.
4. Граф — одноразовый snapshot. Подписка на последующие изменения сигнала относится к более позднему этапу.
5. Radio reachability и `IsValidTarget()` не равны физической достижимости AI по navmesh.
6. Defender group prefab зависит от штатной конфигурации US/USSR в выбранном scenario и проверяется только runtime.
7. Map descriptor/UI API не используется: на dedicated server UI-компоненты Conflict могут отсутствовать.
8. Отдельный Arland scenario-resource текстом не создаётся, поскольку он не нужен для scripts-only запуска, а фиктивный Workbench-ресурс был бы непроверяемым.
9. При любой ошибке инициализация Stage 0 завершается `RESULT FAIL` без автоматического retry; сама игровая сессия продолжается. Это защищает от дубликатов в рамках текущего GameMode instance.
10. Instance-флаг защищает от повторной инициализации одного GameMode. Он не является persistent-маркером уже сохранённых групп; dedicated-тест запускается с `-backendFreshSession`.
