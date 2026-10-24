# Производительность редактора пресетов

Задача: [issue #27](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/issues/27).
Общий `AICF_LoadoutEditor` обслуживает личный preset и шаблоны бойцов отряда.
Текущие Save/Apply/Delete и squad semantics изменены в issue #66:
[LOADOUT_PRESETS.md](LOADOUT_PRESETS.md); ниже описана оптимизация каталога.

## Затратные пути и границы оптимизации

- `AICF_LoadoutCatalog.Find` раньше проходил весь faction catalog для каждого
  вызова Name/Cost/ItemType/Allows. Индекс prefab → bucket строится один раз
  вместе с экземпляром каталога. Bucket сохраняет все записи в исходном порядке;
  IsEnabled и наличие SCR_ArsenalItem проверяются при каждом Find. Состав
  m_aEntries — snapshot, как и прежде. Новый каталог нужен при смене faction/profile.
- List сохраняет полный проход с актуальными profile/blacklist/category/mode/source
  gates и сортировкой. Set заменяет поиск дубликатов в растущем выходном массиве;
  prefab отмечается только после включения подходящей записи.
- Поиск объединяет ввод до паузы 150 мс и читает последний текст widget. До
  фильтрации native compatibility применяется текстовый фильтр, а при пустом
  запросе дополнительный проход Name не нужен. Рецепт и имя пресета не откладываются.
- На время поиска старая выдача не принимает выбор/Apply. Callback содержит
  revision; Close, SelectTarget, Rebuild, Items и Request отменяют его и меняют
  revision. Перед RPC незавершённый поиск выполняется сразу: даже при отказе
  сервера видимая выдача соответствует введённому тексту.
- `AICF_LoadoutItemGrid` хранит prefab, привязанный к каждому видимому widget.
  Render меняет native binding только при смене prefab; подписи и состояние
  обновляются всегда. SetItems отменяет предыдущий отложенный Render.
  ReleasePreviews сбрасывает bindings; Rebuild/Close по-прежнему отсоединяют
  widgets перед уничтожением draft world.

Разрешения, blacklist, native compatibility и server validation не кэшируются.
Полная сборка draft, replay рецепта и rollback сохранены. Актуальные операции
RPC библиотеки и назначения описаны в LOADOUT_PRESETS.md.
Эта правка не обещает устранить стоимость пересоздания персонажа при смене
экипировки или загрузке нового ресурса: это отдельные потенциально затратные пути.

## Проверки

`Test-AILoadoutStatic.ps1` проверяет coalescing, cancellation/revision,
блокировку старой выдачи, flush перед RPC, порядок фильтров и thumbnail reuse.
Контракт SetItems изменён осознанно: повторный detach при каждой фильтрации
устранён, но lifecycle detach перед уничтожением мира остаётся обязательным.
`Test-PersonalLoadoutContracts.ps1` сохраняет server authority/save/readback gates.

`AICF_LoadoutPerformanceProbe.c` размещается только в отдельном stage Core.
Флаг `-aicfLoadoutPerformanceProbe 1`; canonical launcher, fresh profile,
после ROSTER_READY probe завершает сервер через RequestClose. Проверяет обе
фракции, identity результата Find, Name/Cost/ItemType/Allows для всех записей,
неизвестный prefab и точное равенство упорядоченных списков по источникам,
категориям и режимам. Legacy Find/List скопированы из `fb6f3e1`.
Шесть чередующихся замеров по пять List(0)+Name-проходов, с одинаковым checksum.
Замер прогретого каталога **не является временем отклика UI или render**.
Сохранение/валидацию отдельно проверяет существующий PersonalLoadoutProbe.
Свежие результаты и evidence — в [TESTING.md](TESTING.md).

## Ручной gate issue #27

Пользователь должен сравнить исходную и новую версии на одном сценарии,
одинаковых настройках и библиотеке. Проверить личный редактор и редактор отряда:
быстрый ввод/стирание поиска, источники/категории, прокрутку, смену слота во время
поиска, Apply/удаление/Undo, имя и Save сразу после ввода, Close/reopen.
Зафиксировать время отклика и паузы до/после, проверить последнее значение после
сохранения и повторного открытия. Плавность, native thumbnails и клиентский
save pipeline нельзя подтвердить dedicated benchmark или static audit.
До ручной проверки этот критерий остаётся **NOT RUN**, issue не считается закрытой.
