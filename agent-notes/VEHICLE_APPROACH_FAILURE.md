# Native UNKNOWN на пути к staging waypoint

`SPAWN_SITE_APPROACH_MOVE_FAILED_UNKNOWN` — неуспешный acquisition с bounded
fallback. Это не успешная доставка, не предел вместимости и не доказательство
внешней причины отказа навигации.

В Script Diff `1.8.0.13` `SCR_AIProcessFailedMovementResult` вызывает
`NodeErrorOnce` для `UNKNOWN + IsWaypointRelated`, когда activity ещё имеет
waypoint. `NodeError` вызывает `Debug.Error`; это намеренный native debug stall,
а не null pointer. Общий перехват исключения или подавление всех `UNKNOWN`
недопустимы. Ранее project wrapper обрабатывал пеший slot/recruitment/builder,
но штатный motorized slot со staging waypoint не соответствует ни infantry
unit type, ни его waypoint identity.

Новая граница в `AICF_AINodeLifecycle` принимает только server `UNKNOWN`,
`related=true`, все доступные input ports и существующий infantry-foot handler
classifier. Зарегистрированный vehicle sentinel `-1` остаётся неоднозначным
и безусловно отклоняется. Для нового пути дополнительно проверяются все живые
члены группы: authority, принадлежность группе, отсутствие compartment,
переходов посадки/выхода и `IsSwitchingSeatsAnim`.

`MatchController` только находит актуальный state/slot/faction и coordinator.
Coordinator находит текущий trip и создаёт snapshot с текущим graph revision;
controller проверяет identity и передаёт report acquisition flow. Только
`APPROACHING_SITE` без lease, текущие assignment/intent/group/trip/request
generation, неизменные plan/reservation/preset, ещё действующий deadline,
group entity/Rpl identity и точный native activity/executed action позволяют
записать `AICF_SpawnApproachFailure` на конкретный plan. Связанный waypoint
обязан быть текущим, находиться в очереди и совпадать с phase-owned approach;
location должна быть конечной и не дальше 1 м от его origin.

Report сохраняет receipt и печатает новое событие
`[VEHICLE_SPAWN_APPROACH_MOVE_FAILED]` с прежним общим identity context,
`reason=SPAWN_SITE_APPROACH_MOVE_FAILED_UNKNOWN`, `waypoint`, `location`,
`pending_tick=1`. Он не меняет phase, не создаёт/удаляет entity, не выполняет
cleanup и не восстанавливает infantry order. Native `OnMoveFailed` вызывается
ровно один раз. После его invoker повторно проверяется полный source context;
синхронно заменённый action/assignment не завершается. Только подтверждённый
receipt позволяет завершить захваченный старый action.

Следующий обычный acquisition Tick заново проверяет receipt и возвращает
существующий `EndRequest` / `FallbackToFoot`. Переход, release plan, removal
waypoint и восстановление приказа выполняет существующий controller/handoff.
Неподтверждённый или устаревший receipt отбрасывается. Новый приказ или новый
trip не получает failure старого operation. Обычные functional auditors
продолжают считать такой trip неуспешным; accounting не получает исключение
`NONE`, применяемое отдельно к подтверждённому `VEHICLE_MOVEMENT_DESTROYED`.

## Проверки и controlled runtime

`Test-SpawnApproachFailureContracts.ps1` выполняет текущие production тела
receipt, report, node handler и Tick decision с native adapters. Проверяются
текущий и чужой/stale context, переходы посадки, отсутствие/замена component,
неверные location, callback replacements, повторная доставка и изменённый
operation перед следующим Tick. Это offline gate; он не заменяет compile
или native runtime.

Для отдельного controlled case:

```powershell
& tools/Invoke-AICFSpawnApproachFailure.ps1 `
  -EvidenceRoot .codex-runtime/issue78-continue-20261027/approach-controlled-v1 `
  -ServerPort 2588
```

Runner создаёт неизменную stage, сохраняет source hashes и полные команды,
компилирует её через Workbench и запускает canonical server launcher. Только
`-aicfSpawnApproachFailureProbe 1` подключает injection fixture. Дополнительная
группа Scenario fixture делегирует свою exact current state/coordinator через
super-first resolver к тому же production owner. Она не расширяет production
slot lookup.

Injection передаёт controlled `UNKNOWN` через тот же production node handler
при реальных current trip/activity/action/waypoint/handler. Phase, clocks,
identity и budgets не изменяются. Проверяются native callback, новый Tick,
настоящий typed fallback, release plan, сохранение roster и восстановление
приказа. Это `CONTROLLED_FAULT_INJECTION`, `normal_arrival_proof=0`; результат
не доказывает воспроизведение естественного navigation failure или успешную
обычную поездку. `Test-SpawnApproachFailureLog.ps1` читает полные остановленные
sibling logs и отделяет targeted functional outcome от независимого strict
native resource gate. `NOT RUN` и сохранённый compile/runtime FAIL остаются
отдельными исходами до свежих полных проверок.
