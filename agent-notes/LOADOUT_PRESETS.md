# Пресеты бойцов и отрядов (issue #66)

Редактор ИИ: карта → отряд → «Экипировка». Сохранение библиотеки и назначение
binding независимы. Личный редактор использует тот же Inspector и отдельный
authoritative personal protocol; библиотека ИИ не открывается личному editor.

AI editor использует выбранный макет «Инспектор»: слева десять numeric roles
и имена назначенных комплектов, в центре preview и прокручиваемые slots,
справа catalog с прежними source/category/search и inventory действиями.
Центральная колонка занимает 70% прежней ширины (anchors 0.25..0.502),
catalog начинается с 0.532 и показывает три колонки карточек. Обе grids
прокручиваются колесом и вертикальным native SliderWidget с отдельным
видимым thumb; fixed-capacity virtual rows и selection сохраняются.
Имена приходят в owner snapshot `assignedNames` из bindings разрешённого slot,
а не из текущего черновика или списка библиотеки. Save/Delete их не подменяют;
удалённый пресет оставляет имя у уже назначенного binding.
Текущая роль выделена, черновик имеет текстовую отметку. Неактивные позиции
малого состава видны, но не принимают выбор. Библиотека, имя member/squad preset
и подтверждения — отдельные modal panels. Escape сначала закрывает modal;
выход из editor с черновиками требует подтверждения. Подтверждение Delete/Load/
ApplySquad повторно сверяет captured library и slot revisions, предотвращая
операцию по переиспользованному preset ID после обновления списка.
Пока AI editor захватил ввод, автоматический read личного меню появления
приостанавливается: оба UI используют один owner snapshot/token. Close снимает
этот gate; серверный протокол и выдача личного комплекта сохраняются.
Modal sections — interactive frames с z=10 выше AICF_RectInput родителя (z=1).
Workspace AddModal/RemoveModal ограничивают focus; Hide/Close сначала закрывают
вложенные combo lists, снимают modal и затем восстанавливают внешний focus.
Секция с IGNORE_CURSOR или z=0 блокировала Save/RoleAll/Library, оставляя
работающей отдельную кнопку Close. Guards и native hidden-tree probe проверяют
эту границу; они не заменяют физическую проверку ввода.

- «Применить» назначает текущую numeric position, не пишет библиотеку.
- «Сохранить» записывает именованный комплект, не меняет назначения.
  Совпадающие kind/profile/faction/character/name обновляют один ID;
  точный повтор не пишет файл и не меняет library revision. Старые дубликаты
  остаются доступны для явного удаления.
- AI member presets видны и загружаются на любую роль той же faction/profile.
  Старые файлы читаются без миграции: `character` задаёт исходный prefab сборки,
  а `member` остаётся metadata. Сервер допускает только известные recruit sources
  активного profile и заново проверяет совместимость с выбранным получателем.
- «Всем бойцам…» назначает текущий комплект всем десяти позициям выбранного
  отряда одной atomic operation9, включая неактивные позиции малого состава.
  Все контексты/совместимость проверяются до commit; роли и состав сохраняются.
- «Роль всем отрядам» назначает выбранную numeric position всем десяти
  managed slots стороны, включая временно неразвёрнутые позиции малого состава.
  Это позиция roster, а не имя ATTACK/DEFEND/RESERVE или incarnation группы.
- «Сохранить отряд» открывает отдельное окно имени и сохраняет назначения
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
Если выбранная сторона не имеет managed slots (в частности, сторона людей в
Co-op), selector возвращается к предыдущей стороне, editor и черновик остаются
открытыми. Пояснение выводится в status; пустая сторона не вызывает Close.

## Личный «Мой пресет»

Оба editor строятся через CreateInspector: virtual grids, колесо/vertical slider
и modal sections с общим focus lifecycle. В личном editor слева крупный preview
(0.025..0.225, y=0.22..0.79), в центре восемь строк slots. Избыточные active
карточка и help убраны; название комплекта остаётся в верхней строке. Выбор стороны,
отряда, RoleAll и squad actions скрыты. «Библиотека», naming, Load/Delete и
выход с несохранённым draft устроены одинаково. Применение выбирает personal
recipe для следующего появления; штатный Default loadout остаётся доступным.

## Предпросмотр библиотеки и неполные комплекты

Пресеты могут не содержать оружия или магазинов. Validate проверяет inventory
compatibility, personal blacklist и независимый Capture/Restore/readback;
боеготовность не является условием сохранения или применения.
EquipmentPlan поддерживает такие binding и при переэкипировке уже живого ИИ:
новая revision доставляется через штатный armory, target0 не вызывает ammo loop.
Встроенный несъёмный заряд — часть предмета; native CanRemove и visibility
guards запрещают его отдельное удаление/замену. Съёмные magazines и оружие
остаются необязательными. Native live checks и границы — [EQUIPMENT_SERVICE.md](EQUIPMENT_SERVICE.md).
Формат файлов, идентичность и upsert старых библиотек не меняются.

Общий Library modal показывает выбранный комплект в отдельном
`AICF_LoadoutLibraryView`: собственные draft/world, модель и текстовая сводка
`AICF_LoadoutSummary`. Сводка читает готовый inventory, включая defaults,
карманы и attachments; fixed slots имеют подписи, прочие предметы сгруппированы
по prefab с количеством и категорией. Item labels и категории проходят Resolve
до объединения текста; невидимые native items исключены через IsVisible.
Видны девять строк, wheel/native slider открывают остальные. Видимый thumb
совпадает по ширине с track (grid 0.017, summary 0.02 от ширины); отдельная
более широкая невидимая область ввода сохраняется.
Для squad доступны все десять позиций, включая позиции вне текущего состава.

Owner operation8 — только inspect. AI payload `<id>:<position>`, personal —
`<id>`. Сервер повторно проверяет actor/context, revisions, тип/источник;
inner ResolveLoadoutContext допускает неактивные squad positions, не меняя roster.
Owner snapshot сохраняет контекст исходной позиции/списка. Editor обрабатывает
ответ8 до общей загрузки рецепта и не меняет m_Recipe, applied или draft memory.
Быстрый выбор заменяет отложенный SendInspection. Все операции editor идут через
общую очередь DispatchRequest с интервалом не менее1000ms; intent содержит
captured slot/member/faction, обе revisions и payload. Пока RPC ещё не отправлен,
нулевой token не может принять старый owner snapshot. Только явный RATE_LIMITED
повторяется до двух раз с новым token; неизвестный timeout не повторяет запись.
Hide/Request снимают inspection callback, Close также снимает DispatchRequest.
Закрытый modal/сменившийся ID/revision запрещает
поздний bind. Hide отсоединяет
RenderTarget перед очисткой миров, Close также снимает handlers.

Основной preview теперь подбирает расстояние по высоте модели (90% исходного
вертикального FOV), а не диагонали bounds с оружием. После центрирования модели
target поднят на18% высоты bounds к верхней части корпуса. Orbit/pan/zoom сохранены.
Action buttons и item cards имеют отдельные hover/pressed цвета; disabled
контролы не подсвечиваются, leave/release сбрасывают transient state.

При переносе AI комплекта `AICF_LoadoutTransfer.Fits` создаёт независимый draft
целевой роли и требует точного совпадения inventory signature после Restore.
Перед Capture/Signature в Validate только серверная копия уплотняет slot IDs
в SCR_UniversalInventoryStorageComponent, сохраняя порядок и entities; fixed slots,
editable recipe и его paths не меняются. Restore в private world сначала удаляет
всё несовпадающее с snapshot по всему дереву, затем вставляет вещи: это исключает
промежуточное переполнение default kit другого получателя. Live master restore
использует прежний native путь и authority/rollback guards. Личные presets
сохраняют отдельный identity/profile/faction/character scope; operation9 запрещён.

`AICF_PersonalPresetLibrary` использует AICF_LoadoutStore в personal root с
приватным prefix `<server identity>_<profile>_<faction>_library_preset_`.
Имена: `$profile:AICF_PersonalLoadouts/<prefix><id>_<generation>.json`.
Плоские файлы не требуют native создания подкаталогов через persistent junction.
Member presets имеют два поколения, именованный upsert, tombstones и readback.
Identity/безопасный путь вычисляет AICF_PersonalLoadoutStore.Path на сервере.
Без backend identity используется private store конкретного controller в памяти,
без чтения общей библиотеки; UI сообщает ограничение текущим подключением.

Старые `..._<profile>_<faction>_0/1.json` остаются active storage без изменения
формата. Их содержимое открывается прежним Read; для именованной копии можно
нажать «Сохранить». Library Save/Delete не перезаписывают active. Load открывает
draft, а «Применить» использует прежний personal operation1 (запись active с
readback и выбором). Неизменный active выбирается operation3 без новой записи.
После закрытия editor personal spawn UI запрашивает active Read с обычным retry
gate, чтобы временный library draft не оставался в native spawn preview.

Personal operations5/6/7 — Save named / Load draft / Delete. Они проверяют
active revision и library revision, profile/faction/character, personal blacklist
и inventory policy. Load повторно валидируется, Delete проверяет тот же context.
Owner snapshot объединяет ids/names/kinds/revision и прежний denied list.
Библиотека кэшируется только в SCR_PlayerController по серверному пути; новый
controller без identity не наследует session store другого подключения.

## Владельцы и протокол ИИ

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
| 1 | Apply member recipe | Apply active с readback |
| 2 | Load member draft | Запрещён |
| 3 | Save member preset | Прежний Select |
| 4 | Delete member/squad preset | Прежний Default |
| 5 | Apply position to all slots | Save named personal variant |
| 6 | Save squad by name | Load personal draft |
| 7 | Apply saved squad | Delete personal variant |

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

Обычный игровой profile сохраняет эту библиотеку после выхода. Назначения
позиций принадлежат конкретному матчу; сохранённый preset можно применить в новом.
Библиотека принадлежит хосту и content profile; переход на другой сервер или
другой контент не переносит и не конвертирует recipes.

`Start-AICFRuntime.ps1 -Role Frontend` по умолчанию использует постоянный
`%LOCALAPPDATA%/AICF/LoadoutLibrary`. `profile/AICF_Loadouts` каждого свежего
runtime profile — junction только к этой библиотеке, settings/logs остаются
в свежем profile. Launcher держит exclusive file lease до native exit;
другой launcher с тем же каталогом отказывает. Это не блокировка отдельной
игры, запущенной вне launcher. `-PersistentLoadoutLibraryPath <каталог>` выбирает
такой каталог явно для Frontend/Server; `-IsolatedLoadoutLibrary` отключает
default Frontend library для диагностического запуска. Client эти файлы не меняет.

Та же exclusive lease защищает `personal/` внутри постоянной библиотеки.
`profile/AICF_PersonalLoadouts` fresh Server/Frontend profile — отдельный junction
к этому каталогу. При первом Frontend пустое хранилище копирует canonical active
и named поколения из штатного Documents personal profile с SHA-256 readback;
источники не меняются. Непустое хранилище автоматически не сливается с legacy.
Collector не проходит подкаталоги, сохраняет только personal US/USSR active
и flat named generations IDs0..255; reparse sources отклоняются.
Settings/logs остаются свежими.

При первом Frontend, если постоянная библиотека пуста, canonical файлы обычного
`Documents/My Games/ArmaReforger/profile/AICF_Loadouts` копируются с hash readback.
Старые исходники не переписываются; наличие любого поколения/tombstone запрещает
повторный автоматический импорт. Разные библиотеки не сливаются по совпадающим ID.

`Start-AICFRuntime.ps1 -LoadoutLibraryPath <каталог>` для Server/Frontend импортирует legacy и оба
canonical поколения 0..255, включая tombstones; manifest фиксирует hashes.
Legacy file limit 24576 bytes, envelope limit 600000 bytes, reparse points
отклоняются. Неканонические ID, generation и посторонние файлы не импортируются.

## Проверки

- `Test-AILoadoutStatic.ps1`, `Test-LoadoutQolContracts.ps1`,
  `Test-PersonalLoadoutContracts.ps1`, `Test-CoopScenarioStatic.ps1`,
  `Test-RuntimeLauncherStatic.ps1`, `Test-LocalizationStatic.ps1`.
- `Test-LoadoutQolLogContracts.ps1`: позитивные controls и отрицательные мутации
  полноты runtime evidence, denials, соседнего script log, exit и readiness.
- `AICF_LoadoutInspectorProbe.c` после Qol/Files probes, flag `-aicfInspectorProbe 1`:
  31 client case на невидимом widget tree перед QolClose. Проверяет anchors,
  z/input flags четырёх modal sections, native scrollbar bounds/sync, reveal,
  disabled/empty и cleanup; fake read model проверяет наличие/отсутствие AI slots.
  Не вызывает ShowModal/AddModal, не меняет focus и не имитирует ввод.
- `Test-LoadoutLibraryContracts.ps1`: два свежих profiles с общей библиотекой,
  exclusive lease, legacy/generations/tombstone hashes и отказ конфликтного импорта.
  Junction требуют обычного Windows filesystem доступа; ограниченный процесс
  Codex может дать Access denied, такой запуск не является PASS.
- `AICF_LoadoutPersistenceProbe.c` в isolated Core/UI после остальных probes:
  `-aicfPersistenceProbe seed`, затем `reload` через два Server launcher запуска
  с разными свежими profiles и одним `-PersistentLoadoutLibraryPath`. Проверяет
  native FileIO, member/squad restart, tombstone и сохранность legacy.
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
