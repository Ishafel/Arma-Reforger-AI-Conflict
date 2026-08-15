# Stage 3.5 — Active Motorized Forces

Статус Stage 3.5: **ACCEPTED — OWNER DECISION 15.08.2026**. Статус полной runtime-матрицы: **NOT RUN**. Текущий rewrite принят как базовая линия для Stage 4 согласно [решению владельца](STAGE_3_3_5_OWNER_ACCEPTANCE_2026-08-15.md). Последний Transport run `stage1-server-11245` дошёл до vehicle runtime и завершился FAIL: обнаружены terminal route loop, waypoint reconciliation, bounded-clearance и exact-cargo defects. Их root cause и текущий fix-candidate задокументированы в [runtime fix report 13.08.2026](STAGE_3_5_RUNTIME_FIX_2026-08-13.md). Исторические результаты и незапущенные server/client repeat, 30-минутная матрица и двухчасовой soak сохраняются без переклассификации в отдельном [бланке приёмки](STAGE_3_5_TESTING.md).

Vehicle-lifecycle rewrite Stage 3.5 реализован, переключён вместе со Stage 3 и принят решением владельца. Нормативные требования этапа и подтверждённые положительные свойства не отменены; полная техническая runtime-матрица остаётся незавершённой. Этап не добавляет экономику, строительство или сложную логистику: цель — задействовать все существующие managed-группы и проверить устойчивость более крупного моторизованного состава.

Текущий dirty-working-tree fix-candidate прошёл `tools/Test-Stage3Static.ps1` и `tools/Test-Stage35Static.ps1`; negative fixtures подтвердили `COORDINATOR_SIDE_EFFECT`, `FLOW_CROSS_CALL`, `WAYPOINT_SIDE_EFFECT_OWNER`, `TRANSITION_OUTSIDE_CONTROLLER`, `TRANSITION_EFFECT_ORDER`, `WAITING_WITH_LEASE`, `HANDOFF_CLEARANCE_GATE`, `CLEANUP_CLEARANCE_OWNER`, `CLEANUP_IDENTITY_SAFETY` и `VEHICLE_LIVENESS_OWNERSHIP`. Финальный Workbench 1.8 validate `.cache/stage35-runtime-report-fix-validate-20260813-r4/console.log` создал Game `5719/11200`, CRC32 `859d2690`, при `SCRIPT(E/F)=0`, `ENGINE(F)=0`, VM `0`; 25 shutdown `RESOURCES(E)` сохранены как resource caveat, не runtime evidence.

Final-tree headless development smoke `.cache/Stage35-Rewrite-FinalSmoke-20260812-002210` был **BLOCKED** внешним backend до AICF bootstrap: `Game created=1`, `AICF=0`, `BACKEND(E)=12` (`SSL peer certificate`/`BAD_REQUEST`), `SCRIPT(E/F)=0`, `ENGINE(F)=0`, VM `0`. Он не дал roster/vehicle evidence, не является Repeat T, Repeat-T2 или M30 и не повышает статус этапа. Commit/SHA не записан: проверялось dirty working tree.

Post-cutover run `stage1-server-30017` на Reforger 1.8 завершился `INITIAL_GROUP_SPAWN_TIMEOUT` раньше vehicle phases. Отдельный fix-candidate заменил несовместимый direct `SpawnUnits()` на 1.8 `RequestSpawn(5)` и прошёл focused vehicles-off smoke: восемь `GROUP_ROSTER_READY` по `5/5`, общий `ROSTER_READY` за 5.986 с, без timeout/AICF/SCRIPT/ENGINE failures. Этот smoke снимает только initial-roster blocker и не является техническим runtime PASS transport rewrite; продуктовая приёмка Stage 3/3.5 зафиксирована последующим решением владельца. Полный разбор: [Stage 1 spawn regression](STAGE_1_SPAWN_CUTOVER_2026-08-13.md).

Реализационные решения:

- stable numeric slot остаются `0..3`, а role-local identity выводится как `A0/A1/A2/D0`;
- forward defender сохраняет базовую роль `DEFEND`, а `FORWARD_DEFEND` и `QRF` являются оперативными posture с hysteresis/minimum dwell;
- stock faction defender roster конфигурируется через `SCR_AIGroup`, после managed bind и generation-fenced observers передаётся в глобальную 1.8 spawn queue через `RequestSpawn(5)` и допускается в `READY` только при точном составе `5/5` и совпадении faction каждого бойца;
- `aicfActiveForcesRolesEnabled=0` оставляет проверочный baseline `2 ATTACK / 1 DEFEND / 1 RESERVE`, не возвращая старый размер группы;
- техника по-прежнему включается явно через `aicfVehiclesEnabled=1`; штатный срез Stage 3.5 задаёт transport `4`, armed-light `0` и active/reserved cap `4`;
- `[AICF][STAGE3.5]` пишет roster, strategic assignment/hysteresis, capacity fallback, per-slot activity и агрегированные managed waypoint/entity/vehicle/world-pool счётчики.

## Целевая структура сил

На каждой стороне остаются четыре устойчивых slot, но каждая группа увеличивается до пяти бойцов:

| Slot | Состав | Основная роль | Предпочтительная техника |
|---|---:|---|---|
| `A0` | 5 | основная атака | грузовой транспорт |
| `A1` | 5 | второе направление атаки | грузовой транспорт |
| `A2` | 5 | фланг / поддержка наступления | лёгкий транспорт достаточной вместимости |
| `D0` | 5 | передовая оборона / QRF | лёгкий транспорт достаточной вместимости |

Итоговая штатная численность — 20 managed AI на фракцию, 40 одновременно на две стороны. Целевой active vehicle cap — четыре управляемые машины на фракцию, по одной на каждый живой slot; functional world pool учитывается отдельно.

Целевое распределение ролей — `3 ATTACK / 1 FORWARD_DEFEND_QRF / 0 IDLE_RESERVE`. Все четыре группы должны выполнять полезную задачу. Нахождение на MOB дольше двух commander-интервалов допустимо только при непосредственной защите HQ, ожидании безопасного spawn, восстановлении состава или bounded vehicle lifecycle.

## Группы по пять бойцов

- Initial и replacement deployment создают ровно пять faction-correct бойцов.
- Replacement по-прежнему оплачивается как восстановление одной группы, а не пять отдельных списаний билетов.
- `aicfMaxManagedAgents` должен допускать восемь групп по пять бойцов и одну консервативную pending replacement-проекцию. Рекомендуемая нижняя граница — 48; стандартное значение 64 сохраняет запас.
- Formation, cohesion, stuck/recovery, map markers и heartbeat обязаны работать с фактическим количеством участников, без предположений о прежних трёх бойцах.
- Любая машина проходит role-compatible capacity preflight для всех живых членов группы. Политика остаётся `ALL_OR_FALLBACK`: нельзя отправить часть группы на машине, оставив остальных пешком.

## Активное использование ролей

Три ATTACK-группы не должны безусловно складываться на одну точку. Командующий распределяет их между основной целью, соседним достижимым направлением и поддержкой, сохраняя детерминированный выбор и bounded retarget.

`D0` защищает не MOB по умолчанию, а ближайшую к противнику безопасную дружественную базу. При `CONTESTED`, потере соседней базы или угрозе HQ она получает QRF-задачу; после стабилизации возвращается на передовую оборонительную позицию. Нужен hysteresis/minimum dwell, чтобы группа не меняла роль и waypoint на каждом commander tick.

## Моторизация всех групп

- Vehicle eligibility расширяется с ATTACK-only на все четыре managed slot.
- `A0/A1` предпочитают грузовики; `A2/D0` — лёгкий невооружённый транспорт, если его доступная вместимость не меньше числа живых бойцов.
- Для US исходные кандидаты: M923A1 и вместительный вариант M998; для USSR: «Урал» и UAZ-452 transport. Конкретный prefab принимается только после catalog/resource и compartment-capacity проверки.
- Если лёгкий кандидат не вмещает всю пятёрку, выбирается грузовик. Нельзя молча уменьшать группу или оставлять бойца снаружи.
- Authority перебирает несколько empty-terrain позиций и до создания entity отклоняет воду либо поверхность без устойчивой опоры для колёсной техники; отклонение не продвигает vehicle generation и не занимает entity/cap сверх уже существующей request reservation.
- Новый vehicle request разрешён при minimum combat-ready составе, штатно не менее трёх живых бойцов. Эта policy не отбирает уже назначенную исправную машину у группы, которая понесла потери.
- После обязательного crew будущие пассажиры получают атомарно зарезервированные точные `CargoCompartmentSlot` и отдельные bounded action-token; settled засчитывается только в назначенном compartment, а cancellation освобождает только собственную reservation.
- Normal dismount выводит физически застрявших с помощью bounded per-member movement guidance без relocation/teleport; принудительное перемещение остаётся только terminal/fallback fail-closed восстановлением.
- Прекращение vehicle control немедленно запускает восстановление meaningful infantry order. `order_restored` не зависит от `clearance_safe`: logical occupant, get-in/get-out transition, oriented-bounds или player blocker удерживает только lease release/delete, а terminal/`ABANDONED`/`DESTROYED` никогда не владеет движением группы.
- Armed-light остаётся отдельным опциональным классом и не подменяет транспорт для пятерых. Его состав/вместимость проверяются отдельным срезом.
- Группа сохраняет исправную машину при смене цели и использует `SAFE_REUSE`; новый transport не создаётся, пока прежний пригоден и достижим.
- После уничтожения или недоступности машины действует bounded request/recovery, затем группа продолжает пешком. Запрещены бесконечный spawn-loop, remote GetIn и teleport-in.
- Исправный abandoned vehicle освобождает active AI cap и переходит в player-safe world pool по действующему cleanup-контракту.

## Порядок приёмки реализации

1. Выполнить пехотный baseline `4 × 5` с прежними ролями, чтобы отделить ошибки размера группы от нового планирования.
2. Проверить `3 ATTACK / 1 FORWARD_DEFEND_QRF`, распределение целей и role hysteresis.
3. Выполнить controlled transport/capacity/reuse/recovery/cleanup-срезы для всех четырёх slot.
4. Выполнить 30-минутную headless-матрицу и двухчасовой soak как отдельную техническую проверку принятой базовой линии.

Команды, профили, таблицы доказательств и правила `PASS/FAIL/BLOCKED` находятся в [`STAGE_3_5_TESTING.md`](STAGE_3_5_TESTING.md).

## Критерии приёмки

1. Восемь initial-групп имеют `initial_agents=5`; штатный heartbeat показывает `managed_agents=40` без игроков.
2. Replacement каждого slot снова создаёт ровно пять бойцов и не превышает `aicfMaxManagedAgents`.
3. На каждой стороне три группы имеют активные ATTACK-задачи, а `D0` находится на передовой обороне/QRF; необъяснимого idle на MOB нет.
4. После owner/contested change новая осмысленная задача появляется не позднее двух commander-интервалов без waypoint/recovery churn.
5. Каждая eligible живая группа получает не более одной active/reserved машины; active cap не превышает четыре на фракцию. Новый request требует штатно не менее трёх живых бойцов, но уже назначенная пригодная машина сохраняется после потерь.
6. Для полного initial/replacement roster `BOARDING_COMPLETE` возможен только при settled `5/5`; после подтверждённых боевых потерь требуется `mounted=alive`. Недостаточная role-compatible вместимость приводит к выбору другого транспорта либо bounded infantry fallback.
7. Смена target использует ту же исправную машину, а потеря машины конечна: recovery/request или пеший fallback без duplicate spawn.
8. За 30 минут нет бесконечного роста групп, машин, waypoint и world-pool entries; server FPS и managed AI остаются в заданном бюджете.
9. Двухчасовой soak не выявляет зависших slot, необъяснимых idle-групп, unsafe cleanup или роста памяти/сущностей.

Решением владельца от 15.08.2026 Stage 4 разблокирован до завершения этой матрицы. Supply/economy реализуется отдельным opt-in контуром и при выключенной функции не должен маскировать или менять baseline-поведение командования, состава групп и транспорта.
