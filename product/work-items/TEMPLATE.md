---
id: AICF-XXX-000
title: Краткий проверяемый результат
status: DRAFT
owner: Product Owner
type: implementation
created: YYYY-MM-DD
updated: YYYY-MM-DD
baseline_branch: TO_BE_RECORDED
baseline_commit: TO_BE_RECORDED
target_game_version: TO_BE_RECORDED
writer: UNASSIGNED
parallel_work: forbidden
---

# `AICF-XXX-000` — название

## Outcome

Опиши один наблюдаемый результат. Формулируй состояние продукта после работы,
а не перечень действий агента.

## Product context

- Почему это нужно пользователю или продукту.
- Какой accepted baseline затрагивается.
- На какой Stage/gate/defect ссылается работа.

## Scope

### Included

- Чётко ограниченное изменение или проверка.

### Allowed paths

- `path/to/allowed/file-or-directory`

### Forbidden paths

- `path/to/forbidden/area`

### Non-goals

- Соседние улучшения и рефакторинги, которые не входят в outcome.

## Requirements

1. Проверяемое требование.
2. Проверяемое требование.

## Acceptance matrix

| ID | Gate | Method | Required verdict | Evidence |
|---|---|---|---|---|
| A1 | Пример поведения | static/runtime/manual | `PASS` | path/event/screenshot |

## Required context

Читать:

- [`product/CURRENT.md`](../CURRENT.md)
- [`docs/CONTEXT_INDEX.md`](../../docs/CONTEXT_INDEX.md)
- Точный нормативный раздел Stage-контракта.

Не читать по умолчанию:

- Несвязанные Stage-документы.
- Полные исторические логи.
- Repeat/defect reports без ссылки из этой карточки.

## Verification commands

```powershell
# Добавь только релевантные команды.
```

Если Workbench/runtime недоступен, не подменяй его static audit. Зафиксируй
`NOT RUN` или `BLOCKED` согласно `AGENTS.md`.

## Evidence requirements

- Exact commit SHA и dirty-tree status.
- Версии Game/Tools/Server.
- Параметры запуска и чистый profile.
- Start/end timestamps и exit codes.
- Пути к полным logs и hashes значимых артефактов.
- Screenshots/video для визуальных критериев.
- Компактный acceptance packet в `product/acceptance/<ID>.md`.

## Stop conditions

- Scope требует изменения `Non-goals` или `Forbidden paths`.
- Обнаружен конфликт с owner decision или нормативным Stage-контрактом.
- Нельзя воспроизвести требуемую environment/version baseline.
- Product code изменён после runtime evidence и run нужно повторить.
- Требуется новое продуктовое решение.

## Delivery rules

- Один writer.
- Subagents запрещены, если `parallel_work` не разрешает их явно.
- Независимый review выполняется read-only.
- Агент завершает работу в `READY_FOR_ACCEPTANCE`.
- Только владелец переводит карточку в `ACCEPTED` или `REJECTED`.
- При `BLOCKED` используй переходы из `AGENTS.md`; возобновление всегда идёт
  через `READY`, а не напрямую в активное или проверочное состояние.

## Blocked record

Заполняй только при work-item status `BLOCKED`.

| Field | Value |
|---|---|
| Entered from | `READY / IN_PROGRESS / VERIFYING` |
| Entered by | writer |
| Blocker | |
| Evidence | |
| Last completed gate | |
| Resume condition | |
| Resume actor | `writer / Product Owner` |

## Status history

| Date | From | To | Actor | Reason |
|---|---|---|---|---|
| YYYY-MM-DD | — | `DRAFT` | Product Owner | Initial draft |
