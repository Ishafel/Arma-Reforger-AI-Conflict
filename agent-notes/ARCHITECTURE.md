# Архитектура и навигация по исходникам

Проект расширяет stock Conflict скриптами Enforce. Gameplay resources — тонкие
inherited `MissionHeader`; они выбирают штатный сценарий и его настройки.
Собственных world/base/layout ресурсов нет. North ограничивает штатные базы
whitelist, оставляя ландшафт целым. UI редактора экипировки использует временный
in-memory world для preview; он не является миром кампании.

## Граф addon

| Addon | GUID | Ответственность |
|---|---|---|
| `AIConflictCore` | `9178E5822AFE48EA` | Независимые от карты механики, UI, диагностика |
| `AIConflictArland` | `B52C5F6AEDBF423E` | Общий bootstrap stock Conflict, radio normalization, Arland header |
| `AIConflictEveron` | `A4B2E62595F645A4` | Everon headers и policy выхода из изолированного radio-компонента |
| `AIConflictArlandRHS` | `9F88011DA22B471C` | RHS content profile и compatibility adapters |
| `AIConflictEveronRHS` | `FA9FDCCA428A43BA` | Объединение Everon policy и RHS profile, callsign pool adapter |
| `AIConflictArlandWCSRHS` | `A1CF260928100001` | WCS экипировка, арсенал и техника поверх RHS |
| `AIConflictEveronWCSRHS` | `A1CF261006100001` | Полный и северный Everon с общим WCS + RHS profile |

Vanilla dependency — `58D0FB3206B6F859`. Точный граф зависимостей принадлежит
`addon.gproj` каждого addon и `tools/Start-AICFRuntime.ps1`; GUID не меняются.
Core и stock integrations не должны получать обязательные RHS/WCS dependencies.

Общий вход — `AIConflictArland/Scripts/Game/AIConflictArland/Integration/`
`AICF_ArlandCampaignBootstrap.c`. Он допускает штатные Conflict worlds Arland
и Everon. Integration addons подменяют нужные factory boundaries, сохраняя
один controller и один lifecycle. Карта передаёт Core stock базы и radio graph;
имена и координаты конкретных баз в Core запрещены.

## Домены Core

Корень: `AIConflictCore/Scripts/Game/AIConflict/`. Начинай с владельца домена,
затем читай его конфигурацию и соседние классы.

| Каталог | Ответственность и основные владельцы |
|---|---|
| `Bootstrap/`, `Config/` | Composition root `AICF_MatchController`, запуск и настройки |
| `Command/` | `AICF_CommandAuthorityPolicy`, `AICF_AICommander`: разрешения и выбор стратегической задачи |
| `Objectives/`, `Orders/` | Граф целей, `AICF_OrderPlanner`: infantry intent, target, waypoint, recovery |
| `State/`, `Forces/` | `AICF_FactionState`, `AICF_GroupSlot`: stable slots, async roster, replacement; `AICF_GroupRuntime`: позиция живого бойца |
| `Forces/` | `AICF_InfantryRecruitmentService`, `AICF_InfantryRecruitSpawner`: посещение казармы и покупка пополнения |
| `Vehicles/`, `State/Vehicles/` | Transport trips, fleet, task handoff и cleanup; `AICF_FIAGarrisonService` — отдельный initial гарнизон FIA по сложности header; `AICF_FIAGarrisonPatrol` — локальные маршруты; `AICF_FIAGarrisonRecovery` — перенос существующей техники с player/identity/geometry fences; `AICF_FIAGarrisonCrew` — exact-seat возврат и высадка десанта |
| `Economy/` | `AICF_EconomySystem`: ticket/supply transaction; `AICF_RecruitmentSupplyForecast`: read-only прогноз |
| `Construction/` | Планирование платных layouts и отдельный worker lifecycle каждой базы |
| `Content/`, `Integration/` | Content profiles и узкие stock API adapters |
| `Loadouts/` | Catalog, draft, recipe, binding, применение и хранение экипировки |
| `Respawn/` | `AICF_SquadRespawnPolicy`: передача живого бойца игроку |
| `UI/` | Карта, командование, снабжение, экипировка и клиентские intents |
| `Victory/` | `AICF_VictorySystem`, `AICF_VictoryDiagnostics`: завершение матча и причины отказа |
| `Diagnostics/` | События `[AICF][STAGE...]`, читаемые анализаторами |

`AICF_MatchController` остаётся composition root. Новое поведение помещай в
профильный класс; контроллер связывает его с lifecycle, не забирая доменную логику.

## Authority и identity

Стратегия, spawn/delete, деньги, тикеты, ownership и match result принадлежат
authoritative server/master. Клиент передаёт намерение; RPC заново проверяет
игрока, faction, numeric slot, availability, лимиты и цель. Постоянные данные
JIP передаются через `RplProp`; `Replication.BumpMe()` вызывается только при
фактическом изменении authority state.

Идентичность силы — `faction + numeric slot`. Role-local имя и incarnation
группы могут измениться. Callback/retry обязан сохранять и сверять применимые
group/spawn/trip generation, assignment/intent/graph revision, request token и
immutable entity identity. Не обходи readiness после `RequestSpawn()`.
Origin `SCR_AIGroup` не означает положение бойцов: используй `AICF_GroupRuntime`.
Каждому subscription и повторному `CallLater` соответствует явный cleanup.

## Границы vehicle domain

| Владелец | Обязанность |
|---|---|
| `AICF_VehicleCoordinator` | Facade и scheduling |
| `AICF_TransportTripController` | Переходы trip и cross-domain effects |
| Acquisition/boarding/transit/dismount flow | Выполнение фазы и возврат `AICF_TripOutcome` |
| `AICF_VehicleSpawner` | Site/reservation и entity spawn, только из acquisition в его фазе |
| `AICF_FactionFleet` | Lease, generation и faction cap |
| `AICF_VehicleTaskHandoff` | Vehicle waypoint boundary |
| `AICF_VehicleCleanupManager` | Clearance, release, delete; может жить после окончания trip |

Flow не меняет фазу и не вызывает соседний flow, controller или cleanup.
Waypoint сначала снимается с группы, затем удаляется как replicated entity.
Перед vehicle delete повторяются identity, occupancy и clearance checks;
неопределённость означает fail-closed.

## Соседние контракты

Recruitment прогнозирует полную стоимость и время набора с учётом спроса других
визитов, но не резервирует supplies. Оплата проверяется перед каждым transfer:
[точная модель и evidence](RECRUITMENT_SUPPLY_PLANNING.md).

Construction planner оплачивает unfinished layout один раз; отдельный worker
базы подходит к проекту и выполняет stock progress. Recovery не создаёт второй
worker и не повторяет оплату. Копии runtime fixture в production не остаются.

Loadout templates назначаются slot/memberIndex и применяются к следующему
созданному бойцу. Personal recipes хранятся отдельно по backend identity,
faction и content profile. Server validation повторяется перед выдачей;
ошибки требуют проверяемого rollback. Подробнее о поведении: [GAMEPLAY.md](GAMEPLAY.md).

Не переименовывай events или `key=value` поля без синхронного обновления
анализаторов и этих заметок. Static, compile и runtime — отдельные gates:
[TESTING.md](TESTING.md).
