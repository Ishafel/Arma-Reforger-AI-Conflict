# Co-op СССР на ванильном Арланде

Новые inherited headers в `AIConflictArland/Missions/`:
`AICF_Conflict_Arland_Coop_USSR.conf`, `_Medium.conf`, `_Hard.conf`.
Основной ресурс `{A1CF261018460001}` наследует существующий Arland header;
Medium/Hard имеют GUID `{A1CF261018460002}` / `{A1CF261018460003}`.
World, базы, layouts, идентификаторы addon и dependency graph сохраняются.

`SCR_MissionHeaderCampaign.m_eAICFMatchMode` задаёт `AICF_EMatchMode`:
`STANDARD=0`, `COOP_US=1`, `COOP_USSR=2`. Пункт меню добавлен только для
ванильного Arland / СССР. `COOP_US` — симметричная Core policy без нового header.
Обычные ресурсы наследуют default `STANDARD`.

`AICF_Stage1Config` один раз читает header, затем CLI. Immutable `AICF_MatchRules`
определяет число slots, default commander и применимость ticket defeat.
Co-op СССР автоматически выбирает commander `US`; несовместимый CLI mode
отклоняется существующим ранним CONFIG_INVALID gate. Отдельного
`aicfPlayerOnlyFaction` CLI в этой реализации нет.

`AICF_FactionState` сохраняется для обеих сторон, включая ticket ledger и
экономику. Число slots — 10 у США, 0 у СССР. Initial spawn, replacements,
recruitment, equipment service и transport обходят фактические slots.
Ожидаемое число slots хранится отдельно: empty roster допустим только согласно
policy. Initial queue и ROSTER_READY ждут весь предусмотренный состав; обычный
режим по-прежнему требует 10+10 READY. Событие ROSTER_READY и его поля сохранены.

Минимум `aicfMaxManagedAgents` учитывает полные группы активных сторон:
200 в STANDARD, 100 в Co-op. Default — 220 / 120 соответственно.
Квоты строителей, logistics и FIA остаются у прежних владельцев. Усиление
армии США за счёт дополнительных боевых slots не добавлено.

`AICF_VictorySystem` проверяет билеты только у faction state с
`UsesTicketDefeat()`. Территориальная проверка общая: все активные цели,
без contested/capture, исключая HQ и relay. В Co-op люди могут истощить
билеты США; СССР проигрывает только по территории. Смерти игроков не списывают
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
Таким образом US/FIA скрыты и в intro, и в последующем выборе стороны.

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
`AllowsAutomaticConstruction`: для СССР возвращает null до выбора типа, поиска
места и commit заказа. Тот же gate инвалидирует pending orders. Строительство
США сохраняется. `AICF_BaseBuilderService` не отключается и продолжает достройку
оплаченных stock layouts, в том числе поставленных игроком за СССР.

## Запуск и проверки

```powershell
& tools/Start-AICFRuntime.ps1 -Role Server -Variant StockCoopUSSR -Difficulty Easy
# Клиент — из отдельной терминальной сессии, тот же Difficulty:
& tools/Start-AICFRuntime.ps1 -Role Client -Variant StockCoopUSSR -Difficulty Easy -ServerProfileRoot <fresh-server-profile>
```

Launcher выбирает правильный header для Easy/Medium/Hard, штатный stock graph
и commander US. Явный несовместимый `-AICommanderMode` отклоняется. Direct
header launch из меню получает ту же policy без CLI.

`Test-CoopScenarioStatic.ps1` защищает roster/budget/victory/UI/replication gates
и проверяет отрицательные mutations. Существующие construction/commander
аудиты сохраняют READY и identity guards, заменяя обязательные 20 slots на
предусмотренное policy число. Runtime launcher audit проверяет новые
Medium/Hard headers для обеих ролей.

`tests/fixtures/AICF_CoopScenarioProbe.c` копируется только в `Core/UI/`
изолированного stage. `-aicfCoopProbe USSR|STANDARD` проверяет реальный запуск,
число slots, readiness, ticket policy, helpers, budget, faction access,
допуск автоматического строительства по каждой стороне, стабильность roster и
replicated state. `-aicfCoopProbeDurationMs` задаёт интервал наблюдения.
`-aicfCoopProbeWinner US|USSR` в конце меняет владельца реальных целей и
проверяет production territorial end. Это подготовка fixture, не доказательство
физического захвата игроком. Клиент с тем же probe читает JIP snapshot и
закрывается. Fixture callbacks удаляются в OnGameEnd.

Client-флаг `-aicfCoopProbePlayer 1` сначала отправляет настоящие native запросы
на вступление в US и FIA и проверяет отрицательные ответы сервера. Затем
отправляет faction/respawn intents за СССР и ждёт реального controlled character.
`Test-CoopScenarioLog.ps1 -Client -RequirePlayer` требует этот native readback.
Он не обходит server validation и не использует GUI input.

`AICF_CoopRecruitProbe.c` дополняет этот probe: копируется в том же UI stage
под именем `AICF_ZCoopRecruitProbe.c`, чтобы загружаться после основного probe.
Fixture выбирает штатное отделение из faction GROUP catalog, ждёт асинхронного
spawn, передаёт ID бойца через test-only RplProp, выбирает native loadout перед
spawn игрока и проверяет штатный `RequestAddAIAgent`. На сервере проверяются
IsRecruited, slave membership и фактический parent AIAgent; клиент подтверждает
membership и доступность native команды. Она также снимает лидерство как
test setup и запрашивает штатное голосование из состояния обычного участника.
Это проверка native найма и лидерства, **не проверка оплаты заказа из казармы**.
Auto construction останавливается fixture для изоляции от посторонней stock
mesh-offset диагностики; production construction policy этим не проверяется.
`Test-CoopScenarioLog.ps1 -RequireRecruit` требует полный stopped log обеих ролей,
точный manifest, readiness клиента, intro visibility и native readbacks.

`AICF_CoopBarracksProbe.c` проверяет другой, реальный путь покупки: готовит
штатную казарму и звание как fixture setup, затем клиент вызывает
`RequestEnterBuildingMode`, выбирает native вкладку групп и `CreateEntity`
для Guard. Он не вызывает lifecycle редактора вручную и не выставляет
Recruitable/Recruited. Server resource invoker сверяет цену с балансом изменений
агрегата consumer; `debited`/`credited` включают его пересборку и не являются
журналом денежных транзакций. Каждый Insert снимается при OnGameEnd. После заполнения
группы клиент штатно нанимает всех четырёх бойцов; сервер сверяет slave membership
и настоящий AIAgent parent. Отдельные неоплаченные Guard сохраняют запрет,
native Recruit отказывает при заполнении лимита 4. Клиент закрывает редактор
до RequestClose, чтобы завершение не оставляло pending editor callbacks.
`Test-CoopScenarioLog.ps1 -RequireBarracks` требует эти readbacks, цену/списание,
полные остановленные логи, manifest и readiness. Он не заменяет прежний
probe голоса лидера/intro; `-RequireRecruit` и `-RequireBarracks` взаимоисключаются.
Этот же barracks probe допускает STANDARD, `-aicfBarracksFaction US|USSR`
(default USSR). Для US log audit требует `-BarracksFaction US`; фракция сверяется
с manifest и authority readback. Механика общая: [GAMEPLAY.md](GAMEPLAY.md#найм-купленных-в-казарме-бойцов).

Результаты свежих проверок, полные логи и ограничения — в [TESTING.md](TESTING.md).
