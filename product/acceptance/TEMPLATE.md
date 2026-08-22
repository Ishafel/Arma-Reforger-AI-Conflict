---
work_item_id: AICF-XXX-000
status: PENDING_OWNER_DECISION
prepared_at: YYYY-MM-DD
prepared_by: Codex
baseline_commit: TO_BE_RECORDED
tested_commit: TO_BE_RECORDED
---

# Acceptance packet — `AICF-XXX-000`

## Outcome

Одним абзацем опиши фактически полученный результат. Не повышай технический или
продуктовый статус сверх приложенного evidence.

## Delivery verdict

| Field | Value |
|---|---|
| Work item status | `READY_FOR_ACCEPTANCE / BLOCKED` |
| Delivery verdict | `PASS / FAIL / BLOCKED` |
| Product gate verdict | `PASS / FAIL / BLOCKED / NOT RUN / N/A` |
| Tested commit | `<full SHA>` |
| Dirty tree during tests | `no / yes: details` |

`Delivery verdict` оценивает выполнение самой work item. `Product gate verdict`
оценивает проверяемое поведение продукта. Успешно выполненный defect/retest
может иметь delivery `PASS` и product gate `FAIL`.

Если `Work item status` равен `BLOCKED`, отдельно заполни:

| Field | Value |
|---|---|
| Entered from | `READY / IN_PROGRESS / VERIFYING` |
| Blocker and evidence | |
| Last completed gate | |
| Resume condition | |
| Resume actor | `writer / Product Owner` |

Work-item status `BLOCKED` и технический verdict конкретного gate не
взаимозаменяемы. Возобновление карточки выполняется только через `READY` по
правилам `AGENTS.md`.

## Environment

| Item | Value |
|---|---|
| Branch/worktree | |
| Baseline commit | |
| Tested commit | |
| Arma Reforger | |
| Arma Reforger Tools | |
| Arma Reforger Server | |
| World | |
| Server profile | |
| Observer client profile | |
| Stimulus client profile | `N/A` или exact path |
| CLI options | |
| Start time | |
| End time | |

## Changed files

- None, либо точный перечень с назначением каждого изменения.

Подтверди, что unrelated user changes не включены.

## Verification

| Gate | Verdict | Command/method | Evidence | Notes |
|---|---|---|---|---|
| A1 | `PASS/FAIL/BLOCKED/NOT RUN` | | | |

## Evidence manifest

| Artifact | Absolute path or durable link | Size/hash | Purpose |
|---|---|---|---|
| Server log | | | |
| Client log | | | |
| Workbench log | | | |
| Screenshot/video | | | |
| Analyzer output | | | |

Большие raw logs не вставляй в этот файл. Добавляй только пути, hashes, counts и
небольшие релевантные excerpts.

## Error and event summary

| Pattern/event | Count | Interpretation |
|---|---:|---|
| `[AICF][...][ERROR]` любой фазы | | |
| `SHIPMENT_BALANCE_FAILED` | | |
| `SCRIPT (E/F)` | | |
| `ENGINE (F)` | | |
| `Virtual Machine Exception` | | |
| null-pointer | | |

Добавь task-specific events из активной work item.

## Deviations and risks

- Перечисли отклонения от expected environment, test plan или acceptance
  criteria.
- Укажи, какие gates остались `NOT RUN` и почему.
- Не объявляй известный риск устранённым без отдельного evidence.

## Agent recommendation

- `READY_FOR_ACCEPTANCE`, `BLOCKED` или требуется отдельная defect work item.
- Какое конкретное решение должен принять владелец.

## Owner decision

> Этот раздел заполняет только владелец продукта.

- Decision: `PENDING / ACCEPTED / REJECTED`
- Decision date:
- Accepted commit:
- Accepted scope:
- Accepted deviations:
- Required follow-up:
- Owner note:

Owner `ACCEPTED` не изменяет исторические `PASS/FAIL/BLOCKED/NOT RUN`, если это
не указано отдельным явным решением.
