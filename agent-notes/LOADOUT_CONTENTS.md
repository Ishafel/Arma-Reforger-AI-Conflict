# Общее содержимое разгрузки

`AICF_LoadoutNavigation.ContentRows` строит представление поверх существующих
native адресов. При открытии одежды и контейнера с карманами редактор показывает
этот список по умолчанию; корень бойца и оружейные крепления используют Rows.
Кнопка «Крепления»/«Содержимое» переключает вид без изменения рецепта.

## Адреса и навигация

- В общий список входят `SCR_UniversalInventoryStorageComponent` и native
  `EquipmentStorageComponent` (включая RHS magazine pouches) в поддереве
  открытого path, кроме fixed slots. Path состоит из полных сегментов с `/`,
  поэтому проверка StartsWith не включает соседний slot с похожим номером.
- Каждый предмет проверяется по native parent storage и slot. Повторяющиеся
  storage/entity отбрасываются. Индексы locations и slots указывают на реальные
  исходные storage; UI не вводит синтетических адресов в рецепт/RPC.
- Сначала идут предметы, затем прежние строки добавления в конкретные карманы.
  Нумерация карманов локальна для открытого представления. Одинаковые prefab
  остаются разными строками: удалить можно именно выбранный экземпляр.
- Сами смонтированные подсумки скрыты в общем списке; их содержимое доступно
  без открытия каждого подсумка. Крепления можно менять в прежнем виде.
  Свободно лежащий контейнер остаётся предметом со своим вложенным содержимым.
- История хранит path и режим открытой страницы. Back возвращает к реально
  посещённой странице, даже если вход в контейнер пропустил несколько уровней.
  Смена цели/Close очищают историю, Rebuild очищает её при исчезновении пути.
- В общем списке смена кармана не сбрасывает поиск. Отложенные операции поиска
  отменяются при смене режима, прежние token/revision/RPC gates сохраняются.

## Восстановление после удаления

Исторический FAIL при удалении slot 0 описан в [TESTING.md](TESTING.md):
после пересоздания одежды пустая universal storage предлагала slot 0 вместо
сохранённого slot 1. Preview `Restore` теперь согласует snapshot с существующим
inventory через `RestoreLocal`, сохраняя совпадающие контейнеры и native IDs.
При замене самой одежды освобождаются только конфликтующие LoadoutArea;
server restore сохраняет прежний authority guard и `ClearForRestore` перед
native `ApplyEntityStorageString`. Номера slots в snapshot не перенумеровываются.
Свежие результаты issue #61 приведены в [TESTING.md](TESTING.md).

## Проверка

`AICF_LoadoutContentsInventoryProbe` с ClothingProbe проверяет исходный personal
kit без подмены разгрузки. Независимый oracle использует native `GetAll(false)`
и `FindItemSlot`, не читает navigation locations и не повторяет whitelist
типов `ContentRows`. Он сверяет каждую страницу с native subtree, экземпляры
одинаковых prefab, адреса, порядок, remove/Undo и rows после Capture/Restore.
Также повторяются существующие stock multi-area и RHS/WCS helmet cases.
Для ВС РФ WCS обязательны шесть исходных запасных магазинов АК-12.
Включение: `-aicfContentsInventoryProbe 1`; сервер закрывается после проверки.
Для direct-connect добавь серверу `-aicfContentsInventoryHoldServer 1`:
он ждёт до 180 секунд и закрывается после отключения подключившегося клиента;
клиент с тем же probe закрывается самостоятельно.
Client callback снимается в `OnGameEnd`.

`Test-LoadoutContentsInventoryLog.ps1 -LogPath <полный остановленный console.log>
-LauncherPath <launcher.txt> -NativeExitPath <exit.txt> [-WCS] [-Client]`
требует native exit 0, manifest, завершённые проверки обеих сторон и отсутствие
script/fatal/VM ошибок. `-Baseline -WCS` допускает только воспроизведённые
`MAG_ROWS_EXACT` failures исходного кода; это не PASS поведения.

`Test-AILoadoutStatic.ps1`: subtree/weapon boundary, parent identity, точные
адреса, default view, история и cleanup.
`AICF_LoadoutContentsProbe` в отдельном stage с ClothingProbe: обе фракции,
wearable TV-102 для RHS/WCS (не отдельный Pouch Preset [7] со снимка), независимый подсчёт всех native owned items, отсутствие
дубликатов/соседних поддеревьев, детерминированный порядок, remove/Undo,
сохранность остальных предметов, capture/restore/validation и сохранение рецепта.
Команды и фактические verdict находятся в [TESTING.md](TESTING.md).

Ручной gate: открыть разгрузку с несколькими заполненными подсумками, удалить
предметы из разных карманов прямо в общем списке, Undo, Save/reopen; переключить
«Крепления», открыть вложенный контейнер и проверить Back. Проверить также
пустые карманы, длинные RU/EN подписи и оружейную страницу. Без ручной проверки
визуальное качество и реальный клиентский input/save pipeline — NOT RUN.
