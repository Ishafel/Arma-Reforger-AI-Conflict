# Пресеты бойцов и отрядов (issue #66)

Редактор ИИ: карта → отряд → «Экипировка». Сохранение библиотеки и назначение
binding независимы. Личный редактор сохраняет прежний отдельный протокол.

- «Применить» назначает текущую numeric position, не пишет библиотеку.
- «Сохранить» записывает именованный пресет роли, не меняет назначения.
  Совпадающие kind/profile/faction/character/name обновляют один ID;
  точный повтор не пишет файл и не меняет library revision. Старые дубликаты
  остаются доступны для явного удаления.
- «Роль всем отрядам» назначает выбранную numeric position всем десяти
  managed slots стороны, включая временно неразвёрнутые позиции малого состава.
  Это позиция roster, а не имя ATTACK/DEFEND/RESERVE или incarnation группы.
- «Сохранить отряд» использует отдельное имя под preview и сохраняет назначения
  всех десяти позиций; неназначенная позиция получает исходный recipe своей роли.
  Сначала нужно применить локальные изменения. «[Отряд]» в библиотеке загружается
  кнопкой «Применить отряд» сразу во все позиции выбранного slot. Для обновления:
  применить отряд, отредактировать/применить роли, сохранить под тем же именем.
  Размер, тип, стратегия, roster, экономика и waypoint не меняются.
- «Удалить» удаляет выбранный member/squad preset из библиотеки. Назначения
  независимы от файла и сохраняются. Удаление освобождает capacity библиотеки.

Обычный игрок редактирует свою сторону. `SCR_Global.IsAdmin(playerId)` разрешает
администратору выбор US/USSR. В Co-op у администратора есть отдельный вход на
карте к противнику; боевое командование стороной людей остаётся скрытым.
Выбор стороны сбрасывает локальную память черновиков и строит соответствующий
catalog. Отзыв прав закрывает чужой editor и очищает server owner view.

## Владельцы и протокол

`AICF_LoadoutService` проверяет контекст и строит план, `AICF_LoadoutStore`
владеет файлами, `AICF_LoadoutBatch` — синхронным commit подготовленных bindings,
`AICF_GroupSlot` — revision и собственной копией каждого binding. Живое
пополнение по-прежнему принадлежит EquipmentService, spawn — FactionState.
MatchController только разрешает контекст и передаёт намерение сервису.

`AICF_RequestLoadout` передаёт targetFaction и libraryRevision. Server заново
проверяет player/controller identity, match/master/readiness, faction, numeric
slot/member, source prefab, bounds, права и текущие revisions. После Validate
снова сверяется revision исходного slot. Batch проверяет все target revisions
и допустимость recipes до первого field write, затем вызывает SetLoadout без
callbacks. SetLoadout копирует binding, а точный повтор не меняет revision.

| operation | ИИ | Личный editor |
|---|---|---|
| 0 | Read binding/library | Прежний Read |
| 1 | Apply member recipe | Прежний Save |
| 2 | Load member draft | Прежний протокол |
| 3 | Save member preset | Прежний Select |
| 4 | Delete member/squad preset | Прежний Default |
| 5 | Apply position to all slots | Запрещён |
| 6 | Save squad by name | Запрещён |
| 7 | Apply saved squad | Запрещён |

Library operations 2/3/4/6/7 требуют актуальную global libraryRevision из owner
snapshot. После конфликта UI обновляет список и сохраняет черновик. Любое
библиотечное изменение обновляет viewers даже без изменения slot revision.
Save/Delete/SaveSquad не подменяют локальный recipe ответом текущего binding.
Для dirty comparison UI повторно кодирует полученный recipe локально:
server может сериализовать ResourceName как GUID, client — как `{GUID}path`.
Это одна identity, сравнение сырых peer JSON неверно.

## Persistence и импорт

Каталог: `$profile:AICF_Loadouts`, максимум 256 активных member+squad presets.
Legacy `template_N.json` читается без изменения исходного файла. Новые записи:
`template_N_0.json` и `template_N_1.json`, envelope schemaVersion=2 с version,
kind, context, name, data и deleted. Загружается старшее корректное поколение;
неполное поколение не уничтожает предыдущее. Успех записи требует полного
readback Encode equality до замены server state и revision. Tombstone имеет
приоритет над legacy и переживает restart. Это логическое удаление; прежнее
поколение может оставаться на диске до переиспользования ID.

Squad payload schemaVersion=1 содержит десять полных ограниченных recipes,
profile/faction/name; Decode ограничен 300000 символов. Member recipe сохраняет
лимит 24576. Имя — непустое, не только пробелы, до 80 символов, без CR/LF.

`Start-AICFRuntime.ps1 -LoadoutLibraryPath <каталог>` импортирует legacy и оба
canonical поколения 0..255, включая tombstones; manifest фиксирует hashes.
Legacy file limit 24576 bytes, envelope limit 600000 bytes, reparse points
отклоняются. Неканонические ID, generation и посторонние файлы не импортируются.

## Проверки

- `Test-AILoadoutStatic.ps1`, `Test-LoadoutQolContracts.ps1`,
  `Test-PersonalLoadoutContracts.ps1`, `Test-CoopScenarioStatic.ps1`,
  `Test-RuntimeLauncherStatic.ps1`, `Test-LocalizationStatic.ps1`.
- `Test-LoadoutQolLogContracts.ps1`: позитивные controls и отрицательные мутации
  полноты runtime evidence, denials, соседнего script log, exit и readiness.
- `AICF_LoadoutQolProbe.c` только в isolated Core/UI как
  `AICF_ZLoadoutQolProbe.c`; canonical server/client с `-aicfLoadoutQolProbe 1`.
  Проверяет обе стороны, legacy/update/dedupe/reload/partial write/tombstone,
  batch atomicity и 28 последовательных player-owned RPC. Fixture меняет
  native SESSION_ADMINISTRATOR|GAME_MASTER единым mask, иначе stock editor
  синхронно меняет GAME_MASTER внутри role invoker и вызывает recursion.
  Это тестовое изменение native roles, production не выдаёт права игроку.
- `AICF_LoadoutFilesProbe.c` добавить после него как
  `AICF_ZZLoadoutFilesProbe.c`, flag `-aicfLoadoutFilesProbe 1`. Read-only wrapper
  до/после каждого Apply читает через FileHandle реальные bytes и наличие всех
  canonical файлов библиотеки; 10 FILE_IMAGE_UNCHANGED cases. В анализаторе
  добавить `-Files`, иначе этот дополнительный coverage не требуется.
- `Test-LoadoutQolLog.ps1 -LogPath <console.log> -LauncherPath <launcher.txt>
  -NativeExitPath <exit.txt> [-Client]` читает полные остановленные и соседние
  logs, требует native 0, exact CLI/manifest, readiness и все случаи.

Статус текущих запусков/evidence — в TESTING.md. Физический ввод, внешний вид
и удобство RU/EN редактора требуют игрока: открыть редактор, выполнить Save /
Apply / Delete / Role all / Save squad / Apply squad, сменить сторону и роли,
проверить Close/reopen. Без этого визуальный/input результат — NOT RUN;
программный RPC probe его не заменяет. Новый JIP, packaged build и долгий бой
также проверяются отдельно.
