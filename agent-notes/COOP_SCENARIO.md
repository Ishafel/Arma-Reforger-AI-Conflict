# Co-op: стороны, карты и контент

Общая policy поддерживает US и USSR во всех девяти сочетаниях карты и контента:

| `Variant` | Карта | Контент / реальные faction keys |
|---|---|---|
| `Stock` | Arland | vanilla: `US` / `USSR` |
| `Everon`, `EveronNorth` | Everon / север | vanilla: `US` / `USSR` |
| `RHS` | Arland | RHS: `RHS_USAF` / `RHS_AFRF` |
| `EveronRHS`, `EveronNorthRHS` | Everon / север | RHS: `RHS_USAF` / `RHS_AFRF` |
| `ArlandWCSRHS` | Arland | WCS + RHS: `RHS_USAF` / `RHS_AFRF` |
| `EveronWCSRHS`, `EveronNorthWCSRHS` | Everon / север | WCS + RHS: `RHS_USAF` / `RHS_AFRF` |

Для каждого доступны Co-op US и USSR на Easy/Medium/Hard: 54 логических
плана и 27 Standard. Отдельные 81 .conf/.meta, GUID и прежние aliases удалены.
Один AI Conflict entry передаёт выбор в native mission через runtime selection;
подробности и миграция — [SCENARIO_SETUP.md](SCENARIO_SETUP.md).

## Политика режима

`AICF_Stage1Config` один раз читает header, затем CLI. Immutable `AICF_MatchRules`
определяет число slots, default commander и применимость ticket defeat.
Co-op автоматически выбирает commander противоположной стороны; несовместимый CLI mode
отклоняется существующим ранним CONFIG_INVALID gate. Отдельного
`aicfPlayerOnlyFaction` CLI в этой реализации нет.

`AICF_FactionState` сохраняется для обеих сторон, включая ticket ledger и
экономику. Число slots — 0 у людей, 10 у противника. Initial spawn, replacements,
recruitment, equipment service и transport обходят фактические slots.
Ожидаемое число slots хранится отдельно: empty roster допустим только согласно
policy. Initial queue и ROSTER_READY ждут весь предусмотренный состав; обычный
режим по-прежнему требует 10+10 READY. Событие ROSTER_READY и его поля сохранены.

Минимум `aicfMaxManagedAgents` учитывает полные группы активных сторон:
200 в STANDARD, 100 в Co-op. Default — 220 / 120 соответственно.
Квоты строителей, logistics и FIA остаются у прежних владельцев. Усиление
армии противника за счёт дополнительных боевых slots не добавлено.

`AICF_VictorySystem` проверяет билеты только у faction state с
`UsesTicketDefeat()`. Территориальная проверка общая: все активные цели,
без contested/capture, исключая HQ и relay. В Co-op люди могут истощить
билеты противника; сторона людей проигрывает только по территории. Смерти игроков не списывают
билеты. Экономика и vehicle subsystem всегда включены.

Match mode реплицируется через RplProp штатного campaign entity; authority
меняет его с BumpMe только при изменении. Командная панель стороны игроков
не создаётся, HUD показывает Co-op/припасы/территориальную цель. Supply UI
сохраняется. Server handlers повторно определяют фракцию игрока и получают
slot из её state: несуществующий slot нельзя настроить или пополнить.
`AICF_CoopFactionMenu` исключает остальные стороны из native списка выбора,
его active buttons и случайного выбора. `CanRequestFaction_S` и `SetFaction_S`
на authority отдельно отвергают чужую сторону, включая прямой запрос клиента;
null для снятия affiliation сохраняет native обработку. STANDARD делегирует
штатному поведению. Глобальный `SCR_Faction.IsPlayable` не меняется: он также
нужен native Conflict для определения противников и HQ. Native заказ ИИ не запрещён.

Welcome/Deployment Setup — отдельный список `SCR_WelcomeScreenFactionContent`.
Его `FillFactionWidget` сначала выполняет native создание, затем скрывает
запрещённый tile. Элементы остаются в массиве: удаление или пропуск нарушил бы
соответствие sorted faction indices при обновлении числа игроков и playability.
Таким образом противник/FIA скрыты и в intro, и в последующем выборе стороны.

`AICF_CoopPlayerGroups` явно разрешает native меню групп и
`IsGroupLeaderVolunteeringAllowed` в Co-op. STANDARD делегирует обоим native
методам. Первый участник пустой штатной группы получает native лидерство;
остальные могут выдвинуться через штатное голосование. Звание GENERAL само по
себе не означает лидерство. `OnPlayerFactionChanged` после native обработки
планирует join/create для игрока без действующей группы. Callback повторно
проверяет authority/master, точный controller, affiliation и faction map;
существующую группу сохраняет, при OnGameEnd удаляется из callqueue.
Назначение лидера, состав slave group, лимит ИИ и найм остаются у native систем.
Автоматически забирать ИИ у других игроков, армии AICF или рабочих не разрешается.

Штатный список казармы включает `Group_USSR_FireGroup_Guard`: его четыре
`Character_USSR_*_Guard` имеют `Recruitable=0`, поэтому native Recruit отказывает
даже лидеру пустой группы. `AICF_BarracksRecruitment.OnEntityRequested` после native
обработки допускает только server/master, разрешённую MatchRules фракцию, troop provider
с `CanCommandAI` и текущий controlled entity игрока той же фракции.
Фракция определяется через `SCR_AIGroup.GetFaction`: штатный callback получает
null, поскольку `SCR_Faction.GetEntityFaction` ищет отсутствующий у группы
`FactionAffiliationComponent`. Ненулевое несовпадение callback faction отклоняется.
Server-only marker на конкретной группе позволяет выставить `SetRecruitable(true)`
существующим и новым async members при точном parent group и faction совпадении.
Неизменённые флаги не вызывают BumpMe. Обычные Guard, helpers и чужие группы
разрешение не получают. Купленные игроком группы допускаются также в STANDARD.
Найм выполняет native `RequestAddAIAgent`;
права лидера и штатный лимит **4 ИИ на player group** сохраняются.

`AICF_ConstructionPlanner.Commander` явно применяет
`AllowsAutomaticConstruction`: для стороны людей возвращает null до выбора типа, поиска
места и commit заказа. Тот же gate инвалидирует pending orders. Строительство
противника сохраняется. `AICF_BaseBuilderService` не отключается и продолжает достройку
оплаченных stock layouts, в том числе поставленных игроком.

## Каталог и запуск

Schema 2, четыре native targets, девять map/content descriptors и семь graphs
описаны в SCENARIO_SETUP.md. Update-AICFScenarioRegistry генерирует descriptors;
генератор отдельных Co-op ресурсов удалён. Режим и difficulty — runtime policy,
сервер передаёт их по RplProp, client/JIP применяет подтверждённый snapshot.
Launcher принимает Variant/MatchMode/Difficulty; StockCoopUSSR больше не alias.
Новый integration добавляет native target/policy, без набора mode headers.
Workshop name limit 50 остаётся; собственное имя entry — literal AI Conflict.
Static audits защищают roster/budget/victory/UI/authority и negative mutations.

`tests/fixtures/AICF_CoopScenarioProbe.c` копируется только в `Core/UI/`
изолированного stage. `-aicfCoopProbe US|USSR|STANDARD` проверяет реальный запуск,
число slots, readiness, ticket policy, helpers, budget, faction access,
допуск автоматического строительства по каждой стороне, стабильность roster и
replicated state. `-aicfCoopProbeDurationMs` задаёт интервал наблюдения.
`-aicfCoopProbeWinner US|USSR` в конце меняет владельца реальных целей и
проверяет production territorial end. Это подготовка fixture, не доказательство
физического захвата игроком. Клиент с тем же probe читает JIP snapshot и
закрывается. Fixture callbacks удаляются в OnGameEnd.

Client-флаг `-aicfCoopProbePlayer 1` сначала отправляет настоящие native запросы
на вступление к противнику и FIA и проверяет отрицательные ответы сервера. Затем
выбирает native loadout до respawn за сторону людей и ждёт controlled character.
Реальные faction keys берутся из content profile. `Test-CoopScenarioLog.ps1
-Client -RequirePlayer -RequireWelcome` требует native readback и видимость
только разрешённого tile в штатном Welcome. Deployment callback логируется,
если окно создано native UI; автоматический respawn его не открывает.
Он не обходит server validation и не использует GUI input.

AICF_CoopHeadersProbe.c теперь читает штатный target для каждого из 54
Co-op планов. Отдельные mode/difficulty headers и legacy FIA flags не читаются.
Полный выбор и retired GUID проверяет AICF_RuntimeScenarioProbe.c.
Native чтение логических планов не подтверждает 54 полных боевых кампании.
