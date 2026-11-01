# Единая ветка работ по #78 — 01.11.2026

Canonical branch: `codex/issue-78-vehicle-presets`, существующий
[draft PR #79](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/pull/79),
`Refs #78`. Issue остаётся OPEN, PR draft. Объединение веток не закрывает
сохраняющиеся FPS/runtime/resource и исходные native-only blockers.

## Что объединено

Большинство продолжений уже представляли последовательную историю.
Локальная issue-ветка fast-forward с `c5ccf6e` до `c212de5` включает UI,
Co-op, подписи, цены, FPS/census, registration, WCS и поздние проверки.
`origin/main`45d0431 уже является предком этого среза.

Отдельная `codex/ai-combat-vehicle-prices` содержала два оригинальных коммита
`4c15828`/`62f9c1b`, patch-equivalent уже перенесённым `d5df998`/`0787f1a`.
Merge `4ee7ec7` добавляет исходную ветку в ancestry без повторного изменения
цены. Его родители: c212de5 и62f9c1b. Конфликты только в
`agent-notes/AI_VEHICLE_PRICES.md` и `agent-notes/README.md`:
сохранены актуальный проверенный путь архива вместо удалённого worktree,
полный указатель и integer guard. Игровых конфликтов не было.

Полное дерево merge равно c212de5:
`14fe9eb3203fb364457adea914a5601ab65e9865`.
Production/tests/tools changes0. Следующий commit добавляет только этот
отчёт и ссылки/статус agent-notes. Старые ветки не удаляются.

15local branches и дополнительный remote checkpoint проверены через
`git merge-base --is-ancestor <captured SHA> HEAD`, все16/16 включены:

| Ref до объединения | Captured HEAD |
|---|---|
| codex/issue-78-vehicle-presets | c5ccf6e |
| codex/ui-preset-reformat | 7bdbc10 |
| codex/vehicle-seat-layout-fix | 9af7881 |
| codex/preset-editor-opening-fix | 629cdfe |
| codex/coop-fleet-recovery | 2471562 |
| codex/coop-enemy-only | b113151 |
| codex/group-vehicle-preset-label | 6941517 |
| codex/ai-combat-vehicle-prices | 62f9c1b |
| codex/server-fps-census | b44b00f |
| codex/fps-followup | f68ae93 |
| codex/native-boarding-registration | c9a731d |
| codex/wcs-assignment-scope | 52ab958 |
| codex/wcs-highload-soak | 4871e7a |
| codex/language-audit-format-scope | 0fa8fbe |
| codex/addon-version-audit | c212de5 |
| origin/codex/issue-78-vehicle-presets | 6941517 |

Issue body, все12comments и PR metadata прочитаны. Упомянутые в комментариях
issue/UI/seat branches присутствуют в таблице. Дополнительных расходящихся
локальных веток с работой после25.10, кроме ценовой, не найдено.
Нерелевантные старые ветки других issues не вливаются в #78.

## Проверки и ограничения

Свежий baseline5/5PASS0 и после merge5/5PASS0:

- `Test-BoardingRegistrationContracts.ps1`;
- `Test-VehicleSubgroupDestinationContracts.ps1`;
- `Test-AICombatVehiclePriceContracts.ps1`;
- `Test-LanguageAuditContracts.ps1`;
- `Test-CoopFleetDefaultsContracts.ps1`.

`git diff --check`PASS; unmerged entries0. Exact tree equality и ancestry
проверены отдельно, не подменены одинаковыми именами веток или patch counts.

Это не новый полный offline293, Workbench или native запуск. Их повтор
в merge-only задаче **NOT RUN**, поскольку production/tests/tools идентичны
последнему проверенному c212de5. Предыдущие результаты имеют прежний scope:
293unique offlinePASS после двух исправленных failures/rechecks;
production WB Stock/RHS/WCS compilePASS, resource gatesFAIL.

Последний full WCS бой32min server/30min client остаётся **FPS/runtimeFAIL**:
server connected-highload avg16,30/min0,6FPS, client114qualified frontline
windows avg52,74/min29;4meaningful-task deadlines и4cleanup retained после
client exit. Original registration error и rejected ASSIGNMENT_SCOPE0;
пять машин реально двигались, same-vehicle tactical resumes подтверждены.
Подробности — [WCS_HIGHLOAD_SOAK.md](WCS_HIGHLOAD_SOAK.md).

Native-only8 persistence и resource blockers #78, human UI/input,
исходные window/OOM и непроверенные physical модели не объявлены исправленными.
Новый human visual тест NOT RUN по terminal-only policy. PR остаётся draft;
слияние в main, ready, release и закрытие issue не выполняются.

## Evidence

`.codex-runtime/issue78-consolidation-20261101/`:
`refs-before.txt`, `branches.json`, `ancestry.json`, `merge-verdict.json`,
`price-patch-equivalence.txt`, `before/`, `after/`, исходные issue/comments/PR
snapshots, `REPORT.md`. Push/remote HEAD и сохранение draft/OPEN проверяются
при передаче результата и записываются отдельно.

