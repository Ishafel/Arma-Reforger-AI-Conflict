# Визуальная настройка экипировки ИИ

Реализация от 2026-09-15 и результаты проверок описаны в
[AI_LOADOUT_EDITOR.md](AI_LOADOUT_EDITOR.md). Ниже сохранён исходный текст
исследования; его статус «реализация ещё не выполнялась» относится к моменту
исследования, а не к текущему checkout.

Дата исследования: 2026-09-15. Статус: техническая возможность подтверждена
исходниками API; прототип и проверка в игре — **NOT RUN**.

Запрос: кнопка открывает визуального солдата, пользователь выбирает оружие,
одежду, броню и снаряжение, затем нажимает «Сохранить». Настройка лица,
голоса и состава отряда в этот запрос не входит.

## Вывод

Редактор с вращаемой 3D-куклой технически реализуем. Reforger уже содержит
предпросмотр персонажа с одеждой, оружием и обвесами, операции с инвентарём
и сериализацию комплектов. Нужна собственная интеграция AICF: интерфейс
редактирования, серверная проверка, сохранение шаблонов и выдача экипировки ИИ.
Готового редактора AICF, который можно просто открыть одной командой, сейчас нет.

Рекомендуемый вариант — отдельная форма «Экипировка» из карточки отряда
на карте. В центре находится 3D-кукла, слева категории, справа доступные
предметы и содержимое выбранного контейнера. Внизу — имя шаблона,
стоимость, «Отмена» и «Сохранить».

## Предлагаемое поведение

1. Выбрать отряд и позицию бойца: командир, медик, пулемётчик и т. д.
   Редактировать можно также ещё не набранную позицию.
2. Нажать «Экипировка». Открывается отдельный черновик комплекта.
3. Менять форму, шлем, бронежилет/разгрузку, рюкзак, оружие, совместимые
   обвесы, магазины, гранаты и прочие доступные предметы. Наружная экипировка
   отражается на кукле; содержимое карманов показывается списком с количеством.
4. Нажать «Сохранить». Сервер проверяет комплект и возвращает результат.
   Интерфейс показывает «Сохранено» только после подтверждения сервера.
5. В первой версии комплект получает следующий созданный боец этой позиции,
   включая пополнение и замену группы. Уже живой боец сохраняет свою экипировку.
   Это предлагаемая семантика, а не существующее поведение проекта.

Можно позднее добавить «Переэкипировать на базе»: физическое прибытие,
наличие арсенала и supplies, отсутствие боя/посадки и проверка конкретного
живого бойца. Это отдельная операция с оплатой и обработкой частичного отказа.

## Что подтверждено в исходниках

Основной reference — официальный Script Diff **1.8.0.13**, commit
`3d77cc212d5cda9922daf5f45635c7300d2d4cce`, уже находящийся в
`.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.13/`.
Пути ниже отсчитываются от его `scripts/`.

| Возможность | Найденная реализация | Практическое значение |
|---|---|---|
| 3D-предпросмотр | `Game/generated/InventorySystem/ItemPreviewManagerEntity.c`: `SetPreviewItem`, `SetPreviewItemFromPrefab`, `ResolvePreviewEntityForPrefab` | Движок может отображать entity или prefab в `ItemPreviewWidget` |
| Вращение и масштаб | `Game/generated/InventorySystem/PreviewRenderAttributes.c`: `RotateItemCamera`, `ZoomCamera`; пример в `Game/UI/Inventory/SCR_InventoryCharacterWidgetHelper.c` | Управление обзором куклы уже поддерживается |
| Сборка видимого комплекта | `Game/UI/Menu/GameMode/SCR_LoadoutPreviewComponent.c`: `SetPreviewedLoadout` | Stock-код создаёт локальные одежду, оружие и обвесы, прикрепляет их к слотам и обновляет preview |
| Проверка и изменение inventory | `Game/generated/InventorySystem/InventoryStorageManagerComponent.c`: `CanInsertResourceInStorage`, `TrySpawnPrefabToStorage`, `TryReplaceItem`, `TryDeleteItem` | Есть API для совместимости со storage и фактической выдачи предметов; результат нужно проверять |
| Снимок комплекта | `Game/GameMode/Loadout/SCR_PlayerArsenalLoadout.c`: `ReadLoadoutString(IEntity, SaveContext)`, `ApplyLoadoutString(IEntity, LoadContext)` | Методы принимают entity, а не обязательный player ID; это кандидат для внутреннего адаптера ИИ |
| Файл настроек | `Game/generated/Plugins/Serialization/Presets/JsonSaveContext.c`, `JsonLoadContext.c`: `SaveToFile`, `LoadFromFile` | Шаблоны можно хранить отдельно от сохранения кампании |

Сигнатуры предпросмотра и сериализации дополнительно сверены с официальной
онлайн-документацией: [ItemPreviewManagerEntity](https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceItemPreviewManagerEntity.html),
[SCR_LoadoutPreviewComponent](https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceSCR__LoadoutPreviewComponent.html),
[SCR_PlayerArsenalLoadout](https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceSCR__PlayerArsenalLoadout.html).
Для реализации приоритет остаётся у закреплённой версии, поскольку сайт обновляется.

### Почему нельзя просто открыть штатный арсенал для ИИ

- `SCR_InventoryMenuUI` получает персонажа через `GetControlledEntity()`
  локального player controller. Это интерфейс живого игрока с реальными
  inventory-операциями, а не готовый редактор произвольного черновика.
- `SCR_LoadoutPreviewComponent` в ветке пользовательского комплекта читает
  `SCR_ArsenalManagerComponent.m_LocalPlayerLoadoutData`. AICF нужен собственный
  источник данных, чтобы не подменять личный комплект игрока.
- `SCR_ArsenalManagerComponent.SetPlayerArsenalLoadout` сохраняет комплект
  по UUID игрока. Этим хранилищем нельзя обозначить постоянную позицию ИИ.
- `SCR_PlayerLoadoutData` содержит данные для изображения одежды, оружия и
  обвесов, стоимость и faction index. Это не полный формат вложенного inventory.
  Магазины, гранаты и содержимое рюкзака требуют отдельного представления.

### Ограничения stock-сериализации

`ApplyLoadoutString` рекурсивно меняет предметы и удаляет лишние. По коду
операция может завершиться ошибкой после части изменений; транзакционный
rollback этим методом не обеспечивается. Кроме inventory она восстанавливает
character labels и активное оружие. Идентификаторы storage включают класс
компонента и GUID его source prefab, поэтому перенос между разными персонажами
или stock/RHS нельзя считать автоматически совместимым.

Следовательно, сырой JSON от клиента не должен напрямую передаваться в этот
метод. Предпочтителен собственный ограниченный рецепт: разрешённые ресурсы,
слоты, количества, вложенные контейнеры и обвесы. Сервер проверяет рецепт,
а адаптер собирает экипировку и читает результат обратно. Применимость
stock-сериализатора внутри адаптера нужно отдельно доказать на ИИ.

## Точки интеграции AICF

| Область | Существующий код | Необходимое расширение |
|---|---|---|
| Открытие формы | [AICF_StrategicUI.c](../AIConflictCore/Scripts/Game/AIConflict/UI/AICF_StrategicUI.c), `AICF_StrategicUIController` | Кнопка для выбранного stable slot; отдельный владелец окна и preview |
| Запрос клиента | [AICF_StrategicCommandRpc.c](../AIConflictCore/Scripts/Game/AIConflict/UI/AICF_StrategicCommandRpc.c) | Аналогичный player-owned RPC для рецепта; server определяет игрока и faction |
| Долговечная привязка | [AICF_GroupSlot.c](../AIConflictCore/Scripts/Game/AIConflict/State/AICF_GroupSlot.c) | `faction + numeric slot + memberIndex`, ссылка на шаблон и revision; не `A0/D0/R0` и не entity ID |
| Разрешённые персонажи и предметы | [AICF_ContentProfile.c](../AIConflictCore/Scripts/Game/AIConflict/Content/AICF_ContentProfile.c), [AICF_RHSContentProfile.c](../AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Content/AICF_RHSContentProfile.c) | Каталог предметов через активный profile/faction, совместимость с исходной ролью и stock/RHS |
| Initial/replacement roster | [AICF_GroupSpawner.c](../AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_GroupSpawner.c), `AICF_MatchController.CompleteReadyDeployment` | Выдать комплект после materialization и необходимых readiness gates, до завершения deployment; не обходить spawn queue |
| Набор одного бойца | [AICF_InfantryRecruitSpawner.c](../AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_InfantryRecruitSpawner.c), [AICF_InfantryRecruitmentService.c](../AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_InfantryRecruitmentService.c) | Выдать комплект готовому donor-бойцу перед окончательной оплатой/передачей; отменять stale revision |
| Цена | [AICF_InfantryRecruitmentEconomy.c](../AIConflictCore/Scripts/Game/AIConflict/Economy/AICF_InfantryRecruitmentEconomy.c), `AICF_EconomySystem` | Согласованная стоимость и rollback; текущая цена роли не учитывает произвольный комплект |

Сейчас `BuildCharacterRoleCandidates` выбирает заранее заданные роли по
`memberIndex`, а `ResolveRecruitPrefab` находит соответствующий faction prefab.
Отдельно учитывается stock randomizer: concrete prefab живого бойца может
отличаться от запрошенного. При реализации нужно доказать соответствие позиции
конкретному бойцу и совместимость inventory после randomizer; одного сравнения
строк prefab недостаточно. Для многоместного deployment нельзя без проверки
считать порядок `GetAgents()` постоянной идентичностью позиции.

Новая ответственность должна находиться в профильных классах, например
`AICF_LoadoutEditor`, `AICF_LoadoutService`, `AICF_LoadoutStore` и
`AICF_LoadoutApplicator`. Это предлагаемые имена, таких классов пока нет.
`AICF_MatchController` остаётся composition root. Общая реализация находится
в Core; RHS-особенности добавляются через integration/content boundary.

## Сохранение, authority и совместимость

- Предлагается библиотека шаблонов на сервере в отдельном каталоге под
  `$profile:`. Формат включает `schemaVersion`, content profile, stable faction,
  исходный character recipe, items/attachments, имя и revision. Путь файла
  определяет сервер; пользовательское имя не становится произвольным путём.
- Библиотека переживает перезапуск при сохранении того же server profile.
  Canonical launcher создаёт свежие runtime profiles: перенос библиотеки между
  такими запусками нужно предусмотреть явно. Это не включение campaign save;
  текущие `MissionHeader` с `m_eSaveTypes 0` можно сохранить.
- Привязка шаблона к позиции действует в текущей кампании. Постоянные defaults
  для новой кампании — отдельная настройка, не следствие сохранения шаблона.
- Сервер проверяет право менять выбранную сторону/отряд, допустимость
  предметов, количества, глубину контейнеров, вместимость, боеприпасы и обвесы.
  Доступность в каталоге не доказывает пригодность предмета для ИИ.
- Stock и RHS имеют отдельные совместимые наборы. Удалённый мод или пропавший
  ресурс дают понятную ошибку и сохраняют предыдущую валидную настройку.
- Черновик и кукла локальны. Рабочую экипировку ИИ меняет только authority.
  Сохранение использует revision и request token; клиент получает подтверждение
  и актуальные данные, JIP — текущее состояние. Одноразовый RPC не заменяет
  долговечный snapshot. `Replication.BumpMe()` нужен только при изменении.
- Preview manager кеширует entities. Нужно проверить изоляцию черновиков от
  deploy/inventory preview и жизненный цикл локальных предметов. Закрытие карты,
  смена персонажа, отмена и `Stop()` снимают input handlers и callbacks.

## Объём и порядок реализации

| Вариант | Возможности | Относительная сложность |
|---|---|---|
| Готовые комплекты с 3D-просмотром | Выбрать один из заранее разрешённых наборов | Меньше; свободная настройка отдельных вещей отсутствует |
| Редактор с 3D-куклой | Выбор вещей, черновик, сохранение, выдача следующим бойцам | Рекомендуемый вариант, несколько связанных подсистем |
| Переэкипировка живого отряда | Редактор плюс обслуживание на базе, оплата и восстановление при отказах | Дополнительный этап после редактора |

Первый технический прототип должен доказать четыре вещи: создание изолированной
куклы, замену на ней одежды/оружия/обвеса, сохранение и повторное открытие рецепта,
выдачу того же рецепта одному настоящему ИИ с корректной репликацией.
Точную оценку сроков разумно делать после этого: наличие API не доказывает
совместимость всех inventories, preview caches и RHS-компонентов.

Проект сейчас строит UI программно и не содержит собственных layouts.
Сначала следует проверить возможность встроить preview в этот подход.
Необходимость собственного `.layout` пока не доказана; если он понадобится,
это потребует явного изменения ресурсной границы проекта и документации.

После прототипа: серверный каталог/валидатор и хранилище, форма, интеграция
initial/replacement/recruitment, экономика, stock/RHS и multiplayer-проверки.
Минимальная проверяемая версия должна покрывать все три пути появления бойцов.

## Проверки и статус исследования

До изменения production-кода сохранить новый baseline релевантных Stage,
AI commander и infantry recruitment audits из [TESTING.md](TESTING.md).
Затем нужны терминальный Workbench для затронутых addon graphs и runtime:

- initial spawn, recruitment, полная замена группы и сохранение stable slot;
- смена шаблона во время асинхронного spawn и устаревший callback;
- нехватка supplies, частичный отказ inventory и отсутствие повторного debit;
- повторное открытие окна, отмена, закрытие карты, сохранение после перезапуска;
- другой союзный клиент, недопустимая сторона, одновременное редактирование и JIP;
- stock и RHS: видимая одежда/броня, совместимые обвесы, количество предметов,
  стрельба, перезарядка и использование доступного ИИ снаряжения.

Визуальная проверка выполняется пользователем; до неё verdict **NOT RUN**.
Codex использует только терминальный workflow и полные остановленные logs.

В этом исследовании production-код не менялся; добавлен только этот документ.
Исследован dirty checkout с HEAD `ef768d6bfda17d403a2a4e781399f27edbe93ab7`;
существующие пользовательские правки не затрагивались.
Команды исследования: `git -c safe.directory=... status --short`, `rg`,
`Get-Content`, `git -c safe.directory=... diff --check`.
До документа `diff --check` — **PASS / 0**; предупреждения о LF/CRLF не являются
ошибками whitespace. После добавления документа `diff --check` для tracked
изменений — **PASS / 0**; проверка 11 локальных Markdown-ссылок и отдельная
PowerShell-проверка whitespace нового файла — **PASS**. У `git diff --no-index`
между `/dev/null` и новым документом exit `1` обозначает наличие различий;
вывода о whitespace errors нет.
Static audits, Workbench compile, server/client runtime, JIP и visual —
**NOT RUN**: реализация редактора ещё не выполнялась.
Документированный прежний `AI_COMMANDER_UI_STATE` **FAIL / 1** из
[TESTING.md](TESTING.md) не исправлялся и заново не проверялся.
