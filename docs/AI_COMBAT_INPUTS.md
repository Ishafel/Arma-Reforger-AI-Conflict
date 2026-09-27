# Отмена боевого действия при устаревших входных данных

В RHS-сессии 26.09.2026 обнаружены две runtime-ошибки штатных BT-узлов:

- `SCR_AIUpdateTargetAttackData.EOnTaskSimulate`, строка 110 Script Diff
  `1.8.0.13`: `selectedWeaponComp` отсутствует, но первый update передаёт
  его в `WeaponHasBurstOrAutoMode(notnull BaseWeaponComponent, int)`.
  Зафиксированы четыре VM exception в 20:33:20–20:33:23.
- `SCR_AIGetSuppressionVolumeCenterPosition.EOnTaskSimulate`, строка 25:
  `No suppression volume provided!` в 20:14:23. `NodeError` вызывает
  `Debug.Error` до возврата `FAIL`.

`Forces/AICF_AICombatInputGuards.c` добавляет узкие проверки перед вызовом
штатных методов. Нет выбранного оружия при первом update — текущая ветка
возвращает `ENodeResult.FAIL`. `m_bFirstSimulate` остаётся у stock, поэтому
инициализация повторяется после выбора оружия. Поздние updates, режимы огня,
оценка целей и работа с существующим оружием остаются штатными.

Нет suppression volume — узлы центра и линии подавления возвращают штатный
результат `FAIL` без вызова `NodeError`. Для существующего volume выполняется
`super.EOnTaskSimulate`. Возврат `SUCCESS` и стрельба в вымышленные координаты
не используются. Узел линии включён как соседняя ветка с тем же контрактом;
в исходном логе ошибка зафиксирована только в узле центра.

Это общая совместимость с vanilla AI, поэтому проверки находятся в Core,
без RHS-зависимостей. Установленные скрипты игры и ресурсы не изменяются.
Нет новых событий, callbacks, timers или изменений репликации. Исправление
загружается при следующем запуске сервера; горячая подмена не применяется.

## Проверки

`tools/Test-AICombatInputContracts.ps1` проверяет guards перед native вызовами,
возврат `FAIL`, сохранность first-update состояния и native fallback; шесть
отрицательных мутаций должны отвергаться. Дополнительно выполняются
`Test-Stage35Static`, `Test-Stage3Static`, `Test-Stage4Static`,
`Test-RHSIntegrationStatic`, `Test-InfantryAdvanceContracts`,
`Test-BaseBuildersStatic` до и после изменения.

`tools/fixtures/AICF_AICombatInputProbe.c` используется только в отдельной
копии исходников с `-aicfCombatInputProbe 1`. После `ROSTER_READY` он проверяет
узлы на живых агентах обеих сторон. Для каждой стороны — по 64 вызова при
отсутствующем выбранном оружии, при пустом volume центра и линии. Ссылка
выбранного оружия восстанавливается синхронно; inventory не меняется.
Также проверяется сохранность first-update флага. Спустя 30 секунд fixture
печатает `COMBAT_INPUT_PROBE_FINISHED checks=12 failures=0` и закрывает сервер.
Каждый callback снимается в `Stop`.

Требуются раздельные Workbench Validate production/fixture для Stock и RHS,
canonical launcher и полные остановленные logs. Контролируемые пустые входы
не заменяют длительную боевую сессию с естественной сменой/потерей оружия.
Полноценная атака с валидным BaseTarget в этом fixture не задаётся; сохранение
native ветки проверяется статически. Клиентский визуальный gate — отдельно.

26.09.2026: шесть существующих static audits до/после — PASS, новый contract
audit с шестью отрицательными мутациями — PASS. Production/fixture Workbench
Validate на Stock и полном RHS — PASS. Оба isolated server запуска завершились
с exit 0, 12 проверками и 0 failures каждый; SCRIPT E/F, ENGINE F, VM/NULL
во всех console/error/script logs отсутствуют. Другие resource/world errors
сохранены. Длительный бой с естественной потерей оружия и новый клиентский
осмотр — NOT RUN. Локальный отчёт:
`.codex-runtime/error-spam-20260926-203409/result.md`.
