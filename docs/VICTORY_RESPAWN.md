# Победа и восстановление отрядов — 2026-10-03

Продолжение от 2026-10-04: [диагностика отказа победы, последний захват и
восстановление приказов](ENDGAME_RECOVERY.md), с отдельными verdict static,
Workbench и stock/WCS runtime.

Победа наступает при одном из условий:

- тикеты противника равны нулю; его оставшиеся живые отряды не блокируют результат;
- все активные точки захвата принадлежат стороне и ни одна не оспаривается.

Условия соединены **ИЛИ**. Для территорий проверяются владелец, `GetCaptureState`,
`IsBeingCaptured` и `AreEnemiesPresent`. HQ не захватываются, RELAY не входят
в objective nodes. Неактивные базы вне графа сценария не учитываются. Пустой
или перестраиваемый граф не даёт территориальной победы. Одновременное выполнение
условий у обеих сторон не выбирает победителя произвольно. Stock countdown остаётся
отключённым; матч завершает authoritative `AICF_VictorySystem`.

После полного уничтожения отряда сохраняется прежняя транзакция: по умолчанию
один тикет и 50 supplies за seed нового воплощения того же numeric slot.
Пополнение живого отряда по-прежнему оплачивается supplies. Место замены —
ближайшая к последнему погибшему бойцу допустимая stock spawn point союзной базы
с `ONLINE BARRACKS`. Без точной позиции смерти используется последняя известная
позиция живого участника. Без обоих источников attempt блокируется.

Сравнивается расстояние XZ, равенство разрешается по node id. Ближайшая база,
которая оспаривается, не имеет припасов, казармы или доступного spawn point,
пропускается. Если допустимых баз нет, отряд ждёт. Начальные бесплатные seed
не требуют казармы: иначе автономное строительство не сможет начаться.
При retry не теряется позиция гибели. Owner, safety и наличие казармы проверяются
повторно перед спавном и commit. Ошибка сохраняет прежний rollback ticket/supplies.

`VICTORY` сохраняет `winner`, `loser`, `loser_tickets`, `reason`. Причины:
`ENEMY_TICKETS_EXHAUSTED` и `ALL_OBJECTIVES_UNCONTESTED`. `BASE_SELECTED`
сохраняет прежние поля и добавляет `death_position`, `death_distance_sq`, `barracks`.

## Проверки

Evidence хранится в `.codex-runtime/victory-respawn-20261003/`. Команды static:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage3Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage35Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage4Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-AICommanderModeStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-InfantryRecruitmentStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-VictoryRespawnStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LocalizationStatic.ps1
```

До/после: Stage3, Stage35, AICommanderMode, InfantryRecruitment — PASS / 0.
Stage4 — прежний FAIL / 1, только `STAGE4_ATTACKED_BASES`. Новые проверки
VictoryRespawn — PASS / 0, 12 отрицательных мутаций; Localization — PASS / 0.
Stage4 ranking assertions изменены по новому контракту ближайшей базы, существующий
не связанный с задачей failure не изменялся.

Production Workbench Validate/Compile: Arland и полный EveronRHS — PASS,
native exit 0, `Script validation successful`, без SCRIPT E/F и ENGINE F.
Первый sandbox-запуск создал Game, но SteamAPI_Init не позволил завершить Validate;
он не засчитан. Полные логи и exact args последующих запусков сохранены отдельно.
Исправленная fixture также прошла оба графа; первый fixture compile выявил
неверный downcast `Faction` и сохранён как неуспешный, после чего cast исправлен.

Runtime fixture копируется только в изолированный source stage как
`AIConflictCore/Scripts/Game/AIConflict/Victory/AICF_VictoryRespawnProbe.c`.
После Workbench этого stage запуск:

```powershell
./tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -RepositoryRoot '<абсолютный путь stage>' -ServerPort 2011 `
  -AdditionalArguments @('-aicfVictoryRespawnProbe','1','-aicfRequirePlayerForResult','0')
```

Fixture создаёт казарму, убивает seed US slot 0, ожидает production replacement,
проверяет позицию, generation и списание одного тикета. Затем проверяет
территориальный predicate на минимальном графе реальных баз с контролируемым
enemy-presence input и завершает настоящий матч, исчерпав тикеты USSR при
сохранившихся живых группах. Это не длительный бой и не ручной захват всей карты.
Новый server profile и `AICF_RUNTIME_MANIFEST_JSON` сохраняются launcher.

Результат stock server: native exit 0, самостоятельная остановка fixture,
12/12 cases PASS. В полном остановленном `console.log` 740 строк,
одна reservation и один commit, US slot 0 generation 1 → 2, ticket 12 → 11,
supplies debit 50. `VICTORY reason=ENEMY_TICKETS_EXHAUSTED` при живом USSR slot 0,
повторный вызов не завершает матч второй раз. Территориальная проверка:
`ALL_OWNED_HQ_EXCLUDED`, `OTHER_OWNER`, `CONTESTED_BLOCKS` — PASS.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage4Log.ps1 `
  -LogPath '.codex-runtime/victory-respawn-20261003/server-logs/logs_2026-10-03_19-20-21/console.log'
git diff --check
```

Обе команды — PASS / 0. Server profile:
`C:\Users\retar\AppData\Local\AICF\Server-20261003-192020-961`.
Полный комплект server logs скопирован в evidence `server-logs/`, manifest —
`runtime-manifest.txt`. `runtime-all-errors.txt` сохраняет 14 engine/resource
диагностик: неверный stock Gizmo GUID, семь WORLD unknown-keyword сообщений,
четыре duplicate Hierarchy и две строки resource leak. SCRIPT E/F, ENGINE F,
VM errors и AICF ERROR отсутствуют. Functional cases и Stage4Log — PASS;
полностью чистый общий runtime gate не заявляется. Эти 14 диагностик не
объявляются доказанным pre-change baseline этого изменения.

## Изменённые файлы

- Core `Victory/AICF_VictorySystem.c`: два независимых условия, territories predicate.
- Core `State/AICF_GroupSlot.c`: snapshot места гибели и lifecycle observer.
- Core `Bootstrap/AICF_MatchController.c`: обновление snapshot, Stop и актуальный граф.
- Core `Integration/AICF_ConflictAdapter.c`: admission по ONLINE BARRACKS.
- Core `Economy/AICF_ReinforcementBaseSelector.c`: расстояние от гибели и node tie-break.
- Core `Economy/AICF_EconomySystem.c`: повторная проверка казармы при spawn/commit.
- Core `Forces/AICF_ReinforcementSystem.c`: проверка непосредственно перед SpawnGroup.
- Arland `Integration/AICF_ArlandVictoryPolicy.c`: комментарий о новом контракте.
- `AIConflictCore/Language/AICF_Localization.st` и две runtime language tables:
  актуальное описание победы северного Эверона.
- `tools/Test-Stage4Static.ps1`, `tools/Test-VictoryRespawnStatic.ps1`,
  `tools/fixtures/AICF_VictoryRespawnProbe.c`: audits и изолированная runtime fixture.
- `.gitignore`, `README.md`, `docs/VICTORY_RESPAWN.md`: описание и включение новых
  проверок/отчёта в Git. Локальный игнорируемый `docs/ARCHITECTURE.md` также обновлён.

Client/JIP, визуальный экран победы, длительный бой, полный захват всех точек,
runtime RHS/WCS и Workshop packaging — NOT RUN. Выбор между несколькими
реальными казармами, потеря казармы между reserve/commit и полное завершение
матча по территориям при положительных тикетах не проверены runtime.
