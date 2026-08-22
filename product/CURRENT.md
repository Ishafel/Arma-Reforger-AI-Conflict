# Arma Reforger AI Conflict — текущее состояние

> Оперативный снимок для владельца и агентов. Это не исторический отчёт и не
> замена нормативным Stage-контрактам.

- Updated: `2026-08-20`
- Source branch: `main`
- Source baseline before operational-doc transformation:
  `650b0942fb51a8bdd879d5b9f530663a72989377`
- Target game line recorded by current evidence: Arma Reforger `1.8.0.10`
- Product owner: user
- Overall MVP status: `NOT READY FOR ACCEPTANCE`

## Product goal

Автономная война США против СССР поверх stock Conflict, которая продолжается
без участия игрока, сохраняет server-authoritative стратегическую модель и
может быть перенесена с Arland на другие карты без переписывания commander.

Каноническое видение: [`PROJECT_VISION.md`](../PROJECT_VISION.md).

## Recognized baseline

| Area | Recognized status | Boundary |
|---|---|---|
| Stage 0 | `PASS` | Исторический исследовательский runtime на записанном commit |
| Stage 1 | `PASS` | Пехотный vertical slice и два faction-oriented run |
| Stage 2 | implementation candidate | Полная runtime-матрица, M30 и S120 не завершены |
| Stage 3 | `ACCEPTED` — owner decision | Решение от 2026-08-15; исторические `FAIL/NOT RUN` не переклассифицированы |
| Stage 3.5 | `ACCEPTED` — owner decision | Принят rewrite baseline; отдельный technical runtime backlog сохранён |
| Stage 4 | implementation complete; runtime acceptance partial | Только `E1 PASS`; остальные economy/UI gates учитываются отдельно |

Owner decision для Stage 3/3.5:
[`docs/STAGE_3_3_5_OWNER_ACCEPTANCE_2026-08-15.md`](../docs/STAGE_3_3_5_OWNER_ACCEPTANCE_2026-08-15.md).

Эти статусы не означают, что source baseline автоматически повторно прошёл все
исторические проверки. Каждый новый verdict должен ссылаться на точный commit и
evidence.

## Current Stage 4 matrix summary

| Slice | Current status |
|---|---|
| `E0` economy-off baseline | `NOT RUN` |
| `E1` startup/calibration probe | `PASS` |
| `E2-E13` economy/runtime/soak | `NOT RUN` |
| `U0` economy-off HUD | first slice `FAIL`; `FIXED CANDIDATE, RETEST NOT RUN` |
| `U1-U11` map/command UI | `NOT RUN` |

Нормативный источник:
[`docs/STAGE_4_TESTING.md`](../docs/STAGE_4_TESTING.md).

## Open product gates

- Полная Stage 2 runtime acceptance.
- Standard 30-minute MVP run.
- Multi-client synchronization and JIP.
- Player death and redeployment.
- Stage 3/3.5 technical runtime backlog, не отменённый owner acceptance.
- Stage 4 `E0`, `E2-E13` и `U0-U11`.
- M30 и S120 с контролем entity/group/waypoint growth, memory и server FPS.
- Окончательная балансировка темпа войны.

## Active work item

- [`AICF-OPS-001 — Stage 4 U0 economy-off HUD retest`](work-items/AICF-OPS-001.md)
  — `READY`.

Цель пилота — получить воспроизводимый verdict и acceptance packet, не
смешивая runtime retest с исправлением product code.

## Required owner decisions

- Принять или отклонить evidence пилотного `U0` retest.
- При `U0 PASS` отдельно разрешить обновление нормативной Stage 4 matrix.
- При `U0 FAIL` решить приоритет новой defect work item.
- Определить следующий runtime slice после завершения пилота.

## Update rules

- Обновляй этот файл только компактным актуальным состоянием.
- Не копируй сюда полные логи и длинную defect history.
- `Recognized baseline` меняется только после явного owner decision или нового
  канонического evidence.
- Активная work item и открытые gates могут обновляться по факту выполнения.
- Исторические verdict меняются только в новом отчёте; старый evidence остаётся
  неизменным.
