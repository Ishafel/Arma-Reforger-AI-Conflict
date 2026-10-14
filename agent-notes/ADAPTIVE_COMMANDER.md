# Адаптивный AI-командир

`AICF_AICommander` организует решение своей фракции; `AICF_AdaptiveCommanderPolicy`
собирает read-only snapshot, `AICF_AdaptiveScore.Choose/Evaluate` выполняют общую
production policy. `AICF_OrderPlanner` применяет intent/waypoint и сохраняет
readiness, muster, recovery, recruitment и vehicle boundaries. MatchController
не получает новой доменной ответственности. `SelectAttackTarget` сохранён для
совместимости и baseline; автоматические назначения используют `CollectAttackNodes`
и score, без роли как порядкового индекса кандидата.

## Данные и ограничения

| Фактор | Источник и актуальность | Fallback |
|---|---|---|
| Победа | `ObjectiveNode.IsObjective`, HQ исключён; RELAY не участвует в территориальной победе | Нет имён или координат важных баз; обычная точка имеет равный базовый вклад |
| Radio | Обход от собственного HQ через friendly nodes, native `IsValidTarget`; исходящие hostile направления; потеря достижимых friendly nodes при удалении anchor | Это connectivity, никогда физические метры; несовместимый/пустой граф не создаёт attack target |
| Подход | Позиция живого лидера/участника `GroupRuntime.ResolveAliveLeader`, `DistanceXZ` до базы | Это нижняя оценка расстояния, не navmesh length; отсутствие бойца запрещает решение. Штатный adapter не знает полной длины пути и применяет unknown-route cost |
| Route | Slot-local deferred routes и identity-fenced `HasFailedMovement` | Недоступный кандидат исключён независимо от расстояния. Известную длину pure snapshot может учитывать через `m_bKnownRoute`; live adapter её не выдумывает |
| Угроза | Только собственная база: `GetCaptureState`, `IsBeingCaptured`, `AreEnemiesPresent`, читаются заново при решении | Нет перечисления скрытых врагов. Native flags считаются текущим наблюдением базы; timestamp snapshot имеет TTL 15000 ms. Старое/неизвестное наблюдение не срочное, сопротивление остаётся ненулевым |
| Потеря | Смена наблюдавшегося friendly owner либо подтверждённый controller loss callback; history привязана к base reference и переносится при graph rebuild | TTL 90000 ms, затем обычная ценность. Усиление retake и соседнего friendly направления, без знания численности противника |
| Готовность | READY + живые agents; desired size и роль server slot | Не READY — без назначения/вклада. Менее 3 живых — без attack и с нулевым полноценным усилением; доступен безопасный HQ hold |
| Выделенные силы | Собственные действующие base intents/assignments; point intent в 150 м от базы; ручные и vehicle-suspended назначения | Recruiting, muster, retreat, pending/blocked recovery и failed/deferred route дают нулевой вклад. Ручной point вдали от базы остаётся занятой силой, но не усилением конкретной базы |
| Продолжение | Текущая цель; уменьшение расстояния от первого наблюдения той же generation/assignment identity | Progress сбрасывается при смене identity; arrival ≤100 м, прогресс ≥25 м дают ограниченный бонус |
| Неудачи | Существующий `DeferFailedRouteTarget`, только slot и generation | 180000 ms; новый group generation или истечение возвращают кандидат. Изменение графа само по себе не стирает неудачу. Recovery остаётся владельцем выхода из blocked episode |

Сила `alive * clamp(alive / max(1, desired), 0.5, 1)` при READY и alive≥3.
Для полного 10/10 это 10; для 3/10 — 1.5; для 2/10 и неготового — 0.
Неполный отряд не считается полным подкреплением. Выдачу боевого waypoint
до завершения initial muster дополнительно блокирует существующий planner.

Никаких случайных стратегических личностей или изменения целей каждый frame.
Snapshot воспроизводим; tie-break — меньший graph node ID, slot application —
штатный стабильный numeric order. US/USSR имеют отдельные policy/state/history.

## Оценка и gates

Константы принадлежат `Config/AICF_AdaptiveCommanderConfig.c`.

| Компонент | Вес / порог |
|---|---|
| Objective / RELAY | +60 / +15 |
| Открываемое hostile направление | +8, максимум 6 направлений |
| Теряемый friendly node | +20, максимум 3; HQ сохраняет downstream connectivity |
| Передний край DEFEND | +5 за hostile exit, максимум +20 |
| Подход | −0.015 за метр, максимум −90; неизвестный маршрут ещё −12 |
| Readiness | +min(10, effective force); attack ещё −2 за недостающего бойца |
| Потребность | Неизвестное сопротивление минимум 8; contested/loss 12; HQ threat 20 |
| Распределение | +40×max(0,1−coverage) −60×min(3,coverage); coverage=allocated/demand |
| Угроза | Неснабжённая силами HQ threat: urgency 3, +200; contested: urgency 2, +120 |
| Недавняя потеря | +30; её topology повышает важность без срочного обхода gates |
| Продолжение / прогресс | +18 / ещё +12 |
| Смена | Разница score должна быть ≥15 при одинаковой urgency |
| Обычный HQ hold атакующего | −100 при наличии пригодных attack candidates; при их отсутствии безопасный hold без штрафа |

Urgency сравнивается перед total. После покрытия demand срочность обнуляется,
поэтому насыщенная оборона не забирает автоматически все силы. Концентрация
допустима при высоком demand/стратегической ценности; уникальные цели не обязательны.
ATTACK может оборонять friendly базу через `AREA_SECURITY`; DEFEND сохраняет
`FORWARD_DEFEND`/`QRF`; RESERVE держит HQ или помогает срочной обороне.
Неизвестные противники не превращаются в нулевое сопротивление.

При обычной смене нужны `GetRoleMinimumDwellMs` (2 commander intervals, default
30000 ms) и stable candidate в один commander interval (default 15000 ms).
При потере допустимости/deferred current target и повышении urgency gates
обходятся. Возврат из обороны, смена между равными threatened anchors и обычный
retake сохраняют gates. Posture одного target также не меняется каждый tick.
При отсутствии attack кандидатов выбирается собственный HQ `AREA_SECURITY`
со штатным Defend waypoint; если нет живого бойца/безопасного anchor, новое
решение не применяется, существующий lifecycle/hold остаётся владельцем восстановления.

## Authority и диагностика

Решение повторно проверяет `Replication.IsServer`, reference slot в faction state,
group reference и immutable entity ID, spawn generation, graph revision,
assignment revision, faction и отсутствие ручного intent. Planner повторно
проверяет target validity перед write. Устаревшая decision отклоняется до мутаций.
При vehicle suspension применяется только стратегическое состояние; infantry
waypoint не создаётся. Economy/recruitment/transport/cleanup не переходят в policy.

`[STAGE3.5][ADAPTIVE_DECISION]`: faction, numeric_slot, generation,
graph_revision, assignment_revision, candidate/current_target, allocated/demand,
urgency, strategic/approach/threat/readiness/allocation/continuation/total,
ready/alive/desired/role, position, route_known/deferred, posture/reason.
Одинаковые identity/candidate/reason печатаются не чаще раза в 30000 ms;
смена identity/решения печатается сразу. `ADAPTIVE_CONTINUE` и
`ADAPTIVE_MARGIN_HELD` объясняют сохранение. Фактическая смена и удержание gates
видны в существующих `STRATEGIC_ASSIGNMENT`/`STRATEGIC_CANDIDATE_HELD`.
Существующие events/fields сохранены.

## Проверки

`Test-AdaptiveCommanderContracts.ps1`: authority, faction/slot/entity identity,
readiness, live position, allocation/manual intent, TTL, route gates, margin,
tie-break, trace throttle и suspension; отрицательные source mutations.
Stage35 distribution contract теперь проверяет adaptive score/allocation вместо
устаревшего A0/A1/A2 mapping; baseline до изменения сохранён отдельно.

`AICF_AdaptiveBaselineProbe.c` + `AICF_AdaptiveProbeWorld.c` запускаются на frozen
baseline stage: ordinary stock snapshot и controlled три допустимые цели.
Исходный selector возвращает 1/2/2 в двух одинаковых snapshot. Это техническое
воспроизведение фиксированной схемы, не доказательство исходного эпизода игрока.
Original commander/planner дополнительно применяет три настоящих назначения
и waypoint; baseline seed groups имеют fixture desired size 1. Full-force новая
матрица отдельно использует 10/10 и не сравнивается как балансный эксперимент.

`AICF_AdaptiveCommanderProbe.c` + `AICF_AdaptiveProbeWorld.c` — isolated stage
на Arland/Everon. Native radio eligibility/threat поданы fixture; парные случаи
уравнивают approach/ценность, demand/graph ownership меняются контролируемо.
Production score не копируется. Четыре настоящие группы 10/10 проходят async
RequestSpawn, agent/completion callbacks, exact alive/faction и observation age.
Проверяются настоящие commander/planner assignments и replicated waypoint entities,
same-cycle/manual allocation, concentration, graph cut/bypass, gates/urgent threat,
failure expiry, owner change, loss TTL/rebuild, identity rejection и suspension.
Часы gates/expiry ускорены fixture; actual transport trip не создаётся.
При Stop все собственные callbacks снимаются; fixture отсутствует в production.

`Test-AdaptiveCommanderLog.ps1 -LogPath <полный console.log> [-Baseline]`
требует полный Stop, все cases, отсутствие failures и SCRIPT/fatal, production trace.
Resource diagnostics сохраняются отдельно. Фактические команды, exits, hashes,
ограничения и полные logs: [TESTING.md](TESTING.md), `.codex-runtime/issue42/`.
