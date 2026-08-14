# Stage 3.5 — Transport, автономный прогон 2026-08-12

## Метаданные

- Профиль: `C:\Users\retar\AppData\Local\AICF\Stage35-T-20260812-064517`
- Лог: `C:\Users\retar\AppData\Local\AICF\Stage35-T-20260812-064517\logs\logs_2026-08-12_06-45-17\console.log`
- Run: `stage1-server-11529`
- Клиент: не запускался
- Режим: Transport Stage 3.5, `worldTime=8`
- Начальный roster: 8 групп × 5 бойцов, `managed_agents=40`

## Текущий результат

Статус прогона: **FAIL**.

Главная новая проблема — отклонение согласования транспортного waypoint:

`TRANSIT_WAYPOINT_RECONCILIATION_REJECTED` → `FAILED_CLOSED` → пехотный fallback → `MOB_IDLE_DEADLINE_MISSED`.

Подтверждённые события:

- 5 фактических `MOB_IDLE_DEADLINE_MISSED`: US A2, US A1, USSR A1, USSR A2 и US A0. Каждое событие дополнительно отражается через `CORE_ERROR_BRIDGE`.
- `ACCEPTANCE_FAILURE_LATCHED reason=TRANSIT_WAYPOINT_RECONCILIATION_REJECTED` для US A2.
- US A0/A1/A2 наблюдались в `FAILED_CLOSED` с terminal reason `TRANSIT_WAYPOINT_RECONCILIATION_REJECTED`.
- 24 диагностических `MEANINGFUL_TASK_LOST`. Многие возникли внутри штатных `BOARDING`/`TRANSIT`, когда транспортный lifecycle ещё выполнялся; аудит выглядит чрезмерно чувствительным.
- 1 `BOARDING_TIMEOUT`: US A0, сели 3/5; два пассажира оставались примерно в 11 и 60 м.
- 4 `GROUP_STUCK_DETECTED`; все четыре завершились успешным `GROUP_STUCK_RECOVERY`.
- 5 `ORDER_RESTORE_REQUESTED` и 5 успешных `ORDER_RESTORE_RESULT`.

В этом прогоне пока отсутствуют:

- `DISEMBARK_TIMEOUT`;
- `FALLBACK_DISEMBARK_FAILED`;
- `BOARDING_APPROACH_MEMBER_STALLED`;
- `DRIVER_RECOVERY_EXHAUSTED`;
- превышение vehicle cap;
- duplicate spawn;
- `SCRIPT (F)`.

## Состояние войны

Последний подтверждённый билетный срез:

- US: 19 билетов;
- USSR: 18 билетов;
- боевые группы: 4 US и 4 USSR после replacements;
- управляемые бойцы в срезе: 36, затем US A0 начал replacement на 5 бойцов.

US сейчас формально немного впереди по билетам, но разница составляет только один билет, поэтому победитель ещё не определён.

## Дефекты для доработки

### N35-T-1 — TRANSIT waypoint reconciliation переводит исправную группу в FAILED_CLOSED

- Воспроизводится как минимум у US A0, A1 и A2.
- Trip имеет цель и транспорт, но reconciliation отклоняет identity/waypoint и закрывает lifecycle.
- Требуется логировать expected/actual waypoint identity, owner, trip generation, assignment revision, vehicle lifecycle ID и точное нарушенное условие.
- После fail-closed пехотный waypoint должен немедленно стать исполняемым и не приводить к MOB idle deadline.

### N35-T-2 — массовый ложный или преждевременный MEANINGFUL_TASK_LOST

- Событие неоднократно появляется в `BOARDING` и `TRANSIT`, хотя lifecycle считается bounded и позднее получает vehicle waypoint.
- Следует различать краткий атомарный переход между waypoint и реальную потерю исполняемой задачи.
- Нужны suppression reason и transition grace с operation/trip correlation.

### N35-T-3 — MOB idle после FAILED_CLOSED

- Пять групп нарушили 30-секундный deadline.
- Наличие `waypoint=1` не гарантировало фактическое продолжение движения.
- Нужна проверка current/in-queue/durable waypoint и AI action, а не только агрегатного наличия waypoint.

### N35-T-4 — неполная посадка US A0

- Passenger phase завершилась timeout при `mounted=3`, `alive=5`.
- Два пассажира потеряли исполняемую посадочную задачу, находясь примерно в 11 и 60 м.
- Требуется проверить lifecycle exact-seat action после reconciliation/retarget и исключить потерю per-member boarding token.

## Положительные изменения

- Старые ошибки высадки и forced-exit в текущем окне не повторились.
- Все зарегистрированные order restore завершились `success=1`.
- Stuck recovery восстановил 4/4 обнаруженных случаев.
- Vehicle cap соблюдается: максимум 4 на фракцию.
- FPS стабильно 60 без подключённого игрока.

## Финальный срез перед остановкой

Время последнего полного heartbeat: 06:57:29. Сервер остановлен принудительно после чтения логов, поэтому отсутствие финального match/result event не считается отдельным дефектом.

Финальные счётчики прогона:

- 8 фактических `MOB_IDLE_DEADLINE_MISSED`; в сыром поиске присутствуют 16 строк, потому что каждое событие повторяется через `CORE_ERROR_BRIDGE`;
- 1 `ACCEPTANCE_FAILURE_LATCHED` — `TRANSIT_WAYPOINT_RECONCILIATION_REJECTED` для US A2;
- 1 `BOARDING_TIMEOUT` — US A0, `mounted=3/5`;
- 0 `DISEMBARK_TIMEOUT`;
- 0 `FALLBACK_DISEMBARK_FAILED`;
- 0 `BOARDING_APPROACH_MEMBER_STALLED`;
- 0 `DRIVER_RECOVERY_EXHAUSTED`;
- 32 `MEANINGFUL_TASK_LOST`;
- 7 `ORDER_RESTORE_REQUESTED` и 7 `ORDER_RESTORE_RESULT`;
- 6 `GROUP_STUCK_DETECTED` и 6 successful `GROUP_STUCK_RECOVERY`;
- 0 duplicate spawn, 0 concurrent spawn, 0 `SCRIPT (F)`.

Позднее развитие дефектов:

- `TRANSIT_WAYPOINT_RECONCILIATION_REJECTED` распространился с US A0/A1/A2 на USSR D0 после `SAFE_REUSE_RETARGET`. D0 перешёл `TRANSIT -> FAILED_CLOSED`, получил успешный infantry order restore, но затем всё равно нарушил MOB idle deadline.
- Дополнительные MOB idle errors появились у USSR D0, replacement US A0 и replacement US D0. Наличие `waypoint=1` и `ORDER_RESTORE_RESULT success=1` не гарантировало своевременного фактического продолжения движения.
- USSR A0 завершила транспортный lifecycle с новым terminal reason `DISMOUNT_TRANSITION_REJECTED`. Это отдельная проблема state-machine/identity transition, даже при отсутствии старого `DISEMBARK_TIMEOUT`.
- В `MEANINGFUL_TASK_LOST` попадали также уничтоженные группы с `alive=0` и группы внутри `BOARDING`/`TRANSIT`. Диагностический аудит создаёт warning churn и требует фильтрации по alive и bounded transition grace.

Финальное состояние групп:

| Слот | Alive | Состояние |
|---|---:|---|
| US A0 | 5 | Replacement generation 2, пехотный приказ |
| US A1 | 5 | `FAILED_CLOSED`, пехотный приказ |
| US A2 | 5 | `TRANSIT` |
| US D0 | 5 | Replacement generation 2, пехотный приказ |
| USSR A0 | 5 | `FAILED_CLOSED`, terminal `DISMOUNT_TRANSITION_REJECTED` |
| USSR A1 | 5 | Пехотный приказ |
| USSR A2 | 5 | Пехотный приказ |
| USSR D0 | 5 | `FAILED_CLOSED`, terminal `TRANSIT_WAYPOINT_RECONCILIATION_REJECTED` |

Финальный heartbeat восстановился до 8 групп и 40 managed agents. Vehicle cap соблюдался: US active 3/4, USSR active 2/4; world pool — US 1, USSR 3. FPS оставался 60.

Состояние войны перед остановкой:

- US захватили базу 47;
- USSR захватили базу 29;
- US потеряли 2 билета, USSR потеряли 2 билета;
- итоговый счёт: `US=18`, `USSR=18`;
- победитель не определён, матч оставался `RUNNING`.

Финальный итог прогона: **FAIL**. Основной регресс — ошибочное согласование waypoint/identity в транспортной state machine с переходом в `FAILED_CLOSED` и последующим MOB idle даже после успешного восстановления приказа. Дополнительно подтверждены неполная посадка 3/5, чрезмерный `MEANINGFUL_TASK_LOST` warning churn и новый `DISMOUNT_TRANSITION_REJECTED`.
