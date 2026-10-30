# Задача игрока внутри MOB и требование выхода

`AICF_PlayerMobHoldPolicy.HasCurrentInternalTask` — read-only условие для
`AuditActiveFactionTasking`: текущая пешая задача игрока внутри MOB не требует
выхода из MOB. Оно не подтверждает прибытие, разрешённый idle, продвижение или
успешность восстановления приказа. Исходный `IsReachedCurrentHold` сохранён;
он по-прежнему требует прибытия всего живого пешего состава в native completion
radius. `ResolveAllowedIdleReason`, meaningful-task accounting, movement recovery
и исходные сроки не расширяются.

Условие требует authoritative server, готовый slot, текущий `PLAYER_COMMAND`
с совпадающими POSITION intent/target и `MOVE_AND_HOLD`, отсутствие pending/failed
recovery и промежуточного approach waypoint. Собственный текущий queued
`SCR_DefendWaypoint` находится у цели внутри MOB. Одна и та же exact native
`SCR_AIMoveActivity` либо `SCR_AIDefendActivity` должна быть одновременно current
и executed, `RUNNING`, без suspension/removal и с тем же related waypoint.
Живой состав проверяется целиком, в пределах `MAX_GROUP_SIZE`: реальные conscious
characters, свои agent/control bindings, отсутствие attached vehicle, compartment
и входа/выхода/смены места. Это физический пеший состав, а не доказательство
достижения точки. Group identity, spawn generation, assignment/intent revisions,
waypoint, utility/action и MOB owner/origin повторно проверяются после чтений.

Audit меняет только вычисление `mobPresenceRequiresEgress`. В прежний
`MOB_EGRESS_DEADLINE_MISSED` добавлены фактические `internal_task` и
`egress_required` этого прохода; периодический `SLOT_ACTIVITY` независимо читает
условие и полный физический MOB envelope. Старые события и поля сохраняются.
Политика не имеет cache, invokers, новых clocks, action writes или teleport.

## Причина и предел доказательства

В прежнем ordinary US383 native log POSITION target совпал с MOB owner origin;
это подтверждено native `MOBsSelected` и закреплённым `SCR_AnalyticsApplication`.
Для члена состава не далее36.5302м от цели любая новая точка ближе к этой цели
тоже находится внутри100м MOB. Одновременные требования «приблизиться к цели» и
«выйти за100м» геометрически несовместимы. Это ошибка выбора egress goal,
а не основание объявлять отряд прибывшим.

Старый `MOB_EGRESS_DEADLINE_MISSED` и полный FAIL сохранены. Точные pending/failed
flags того прохода не записаны: применимость нового условия к нему пока
**UNPROVEN**. Новый source contract исполняет actual predicate и audit expression,
проверяет stationary RUNNING с лидером16м/отстающим36м и оставляет arrival=false;
отдельные negative variants проверяют owner/intent/activity/physical guards.
Targeted WinPS5.1: **75 actual cases /11 executed unsafe variants PASS**;
прежние PlayerMobHold18controls, Stage35Static и Stage35RecoveryPolicy — PASS.
Первый запуск нового контракта сохранил FAIL: C# compiler отверг недостижимый
код у constant-false mutation. В harness использованы runtime bool replacements,
все11 unsafe вариантов реально исполнены и отклонены; production для этого не
менялся. Ошибка раннего CLI с comma-list вместо массива имён также сохранена:
checks тогда не выполнялись. Независимый Root replay75/11 также PASS.
Terminal production Stock/RHS/ArlandWCSRHS — **3/3 PASS**, native0/validation/
full3sibling fatal0. Raw384 bytes frozen V24 и V25 совпадают без drift;
fixture-only rename не требовал повторения неизменённого production graph.
Новые combined Soak390 и Multi388 отдельно compiled0/fullfatal0/sourceSHA0.
Native нового изменения — **NOT RUN**. Полный259 PASS относится к snapshot
до этой правки; новый frozen V25 завершён **261/261 PASS**, collector0/exact
union/Logistics1/1019SHA0/6actual sessions closed0. Позднее doc-only уточнение
вердиктов не входит в frozen1019; production/contract bytes совпадают.

Есть отдельное ограничение прежнего `MonitorGroupProgress`: для POSITION он
считает достижение по одному лидеру в20м. Такой leader-only результат не
подтверждает completion всего строя. Новое egress условие не исправляет и не
объявляет устранённым возможное зависание остальных участников. Native проверка
должна отдельно показать их реальное движение/прибытие либо сохранить этот
дефект открытым. Диагностический600с прогон не заменяет обязательный ordinary1800.

Evidence до правки и геометрия:
`.codex-runtime/issue78-finalization-20261030/player-mob-internal-task/`,
`.codex-runtime/issue78-finalization-20261030/mob-hold-readonly/native-mob-origin-geometry.json`.
Contract: `tests/contracts/Test-PlayerMobInternalTaskContracts.ps1`.
