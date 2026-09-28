# AI Conflict Arland WCS + RHS

Отдельный source addon `A1CF260928100001` поверх `AIConflictArlandRHS`.
Сценарий `{A1CF260928100002}Missions/AICF_WCS_RHS_Conflict_Arland.conf`
наследует существующий AI Conflict RHS Arland. World, базы, controller,
readiness, экономика и lifecycle остаются у прежних владельцев.

Реализованы пехотные комплекты WCS, общий baseline редактора и бойца, вкладки Vanilla/RHS/WCS, дополнение арсенала и реестра строительства техники.
Server-only проверки подтверждают inventory и catalogs; визуальная проверка,
client/JIP и покупка техники через меню остаются `NOT RUN`.

## Пехота

Исходные персонажи, роли и faction identity остаются RHS USMC/MSV. После native
`SpawnGroupMember` новый AI-боец получает комплект до применения сохранённого
пользовательского loadout. Этот boundary общий для начального roster,
замены группы и donor пополнения. Callback/timer и второго campaign loop нет.
Новый персонаж игрока с обычным faction loadout получает комплект командира ИИ
той же стороны через `SCR_SpawnPointSpawnHandlerComponent.PrepareEntity_S`,
до передачи управления. Исходный commander prefab выбирается тем же roster
resolver, а inventory собирается общим draft. Сохранённый arsenal loadout
исключён; при принятии управления существующим AI остаётся его inventory.

| Сторона | Форма и шлем WCS | Оружие по роли |
|---|---|---|
| НАТО | Crye G3 Multicam, JPC AOR2 с подсумками по роли, Ops-Core XP, Mechanix, DEFCON; ESS/Oakley | M4A1 URGI, M4A1 Block II/M203, M240 MDO, M249 HAMR |
| РФ | ВКПО 3 EMR, TV102, 6Б47/6М2-1, Mechanix, Luma; 6Б50 «Ратник» | АК-12 EOTech, АК-12 с подствольником у GL, АК-105 у медика, ПКП NPZ, РПК-74М Specter |

Основное оружие и обвес берутся целыми native prefab из `WCS_Weapons` и
`WCS_RHS_Weapons`. Форма, бронежилет, шлем, перчатки, обувь и рюкзак — WCS.
ПНВ отсутствуют во всех базовых комплектах, включая вложенные NVG шлемов.
Они остаются доступными для пользовательской настройки через арсенал/редактор.
РФ: у SL/SR/Medic 6Б50 подняты на шлем, у остальных надеты; используются native
варианты `wear_on_6b47`/`strap_on_6b47`. США: Oakley Gascan у SL/TL, прозрачные
ESS Crossbow у медика, затемнённые ESS у остальных. RHS хранит очки в helmet
node даже в опущенном положении; проверяются и prefab, и реальный слот.

Командиры и обычные стрелки без рюкзаков. Rush12 MC оставлен у US Medic/MG/AR/
AMG/AAR; Rush12 Olive — у РФ Medic/MG/AT/GL/AMG/AAT для медицины, лент, ПТ и ВОГ.
US LAT и GL, РФ AR/SR обходятся подсумками. В сумме 5/10 и 6/10 рюкзаков на
полный roster вместо прежних 10/10 с каждой стороны. 6Ш118/FILBE и ZipOnPanel
не входят в baseline. JPC Ronin 4, UBGL и MG несут штатные подсумки; у UBGL
удалена встроенная панель на спине. Исходные магазины основного оружия
удаляются до переноса вещей, чтобы их временный объём не требовал рюкзака.
Лицо, голос, медицина, ПТ и средства связи сохраняются. Вместимость storages
не повышается. Минимум запасных магазинов для rifle — 6, MG — 3, AR — 4,
подствольного гранатомёта — 6; совместимость проверяется по magazine well.
Нашивки флага и подразделения входят в тот же local draft, с проверкой
совместимости каждого native patch slot.

`PrepareDefaultLoadout` content profile вызывается общим `AICF_LoadoutDraft.Build`
до расчёта baseline cost и replay пользовательских операций. Начальная выдача,
серверная проверка рецепта и preview редактора используют этот же baseline.
В тесте сравниваются полные пути слотов и ResourceName каждого предмета для
всех 20 позиций двух roster, включая нашивки. Израсходованная в бою медицина
или боезапас не меняют сохранённый шаблон пополнения.
Изменения сначала собираются в local inventory draft. Authority, character,
group, faction, prefab и исходный inventory повторно проверяются перед commit.
После выдачи сверяются полный signature, оружие, боезапас и одежда;
при неудаче восстанавливается исходный snapshot. Однократный флаг на character
не позволяет повторной выдаче отменить пользовательский комплект.
FIA продолжает использовать прежний RHS PMC adapter. Virtual hook
`GetPMCPrimaryOverride` нового content profile выбирает suppressed WCS M4A1 URGI: WCS переопределяет старые
ION prefab, и прежний rifleman-комплект перестаёт проходить проверку глушителя.
MG сохраняет ПКМ с актуальным canonical spelling, Sharpshooter — прежний SVD.
Обычный RHS profile возвращает пустой override; проверки не ослабляются.
Неподдержанные роли
RHS (например, sniper/crew) не переводятся в обычного стрелка.

## Арсенал и редактор

Единый faction `ITEM` catalog сохраняет прежние RHS entries, добавляет native
WCS overrides и inventory resources установленного WCS. Новые entries получают
arsenal metadata ближайшего разрешённого prefab-предка. Когда такого предка
нет, используется первый native inventory entry той же категории: отдельно
рубашки, брюки, жилеты, рюкзаки, шлемы, перчатки, обувь, оружие и обвес.
Это явная политика стоимости интеграции: используются native цена/rank/modes
шаблона категории, а не несуществующая авторская цена WCS. Внутренние base,
звук и vehicle parts исключены; ResourceName проходят native resource и
InventoryItemComponent проверки. Контент vendor не копируется.

В редакторе три вкладки `Vanilla`, `RHS`, `WCS`; прежние фильтры типа, mode,
совместимости слота и server allowlist продолжают действовать. Источник
определяется по загруженной цепочке addons: наличие WCS имеет приоритет,
затем RHS, иначе Vanilla. Поэтому переопределённый WCS предмет виден в WCS.

## Техника

При инициализации фракций локальная конфигурация `VEHICLE` дополняется
entries из установленных WCS catalog overrides:

| Сторона | Сохранено RHS | Добавлено WCS | Активные native spawner data |
|---|---:|---:|---:|
| НАТО (`RHS_USAF`, стабильная `US`) | 20 | 72: Abrams, 12 вариантов Bradley и четыре HMMWV M134/TOW | 68 |
| РФ (`RHS_AFRF`, стабильная `USSR`) | 28 | 6: T-72A/T-72B, БМП-3 и УАЗ «Корнет» | 6 |

Исходные entry objects сохраняются в прежнем порядке; новые добавляются после
них. Каталоги одинаково строятся на server/client, native init назначает
indices. Неподходящая сторона и дубликаты исключаются. Цены, ограничения слотов
и ранги берутся из native metadata. Четыре HMMWV включены в faction catalog,
но поставляемые WCS entries не дают активного `SCR_EntityCatalogSpawnerData`:
наличие записи не означает доступность заказа в автопарке. Эта интеграция не
снимает такой запрет и не делает технику бесплатной.

Гусеничные WCS prefab с активным spawner data добавляются также в RHS
`SCR_PlaceableEntitiesRegistry`: прежние 167 entries сохраняются, 73 новых
добавляются в детерминированном порядке. Vehicle labels `US`/`USSR` переводятся
в соответствующие `RHS_USAF`/`RHS_AFRF` только при активном WCS profile.
Тяжёлый завод получает БМП-3, Т-72A/B, Bradley M2A2/M2A3 и Abrams; его native
traits, стоимость, rank, cooldown и проверка свободного места не обходятся.
Созданная через building mode техника получает свою RHS faction только на
master после штатного placement callback.

Существующий автономный транспорт RHS сохранён. AI-командир не получает новую
танковую тактику: гусеничная техника не выдаётся за `LIGHT_TRANSPORT` или
`ARMED_LIGHT`. Добавление в catalog и серверная проверка metadata не доказывают
покупку через клиентское меню, размещение в занятом автопарке или боевое
управление танком.

## Зависимости

Проверены установленные Reforger/Server/Tools `1.8.0.13`, RHS Status Quo
`0.16.5208` (все три RHS пакета) и следующие пакеты WCS:

| Пакет | GUID | Версия |
|---|---|---|
| WCS_NATO | `615806DC6C57AF02` | 8.2.0 |
| WCS_RU | `615818DA7C0343FD` | 8.2.1 |
| WCS_Weapon_Scripts | `68F006D910E7546F` | 8.2.0 |
| WCS_Attachments | `61C74A8B647617DA` | 8.2.0 |
| WCS_Scopes | `62A668F513428630` | 8.2.0 |
| WCS_Sounds | `631C3C1AEE9C90BC` | 8.2.0 |
| WCS_Armaments | `629B2BA37EFFD577` | 8.2.1 |
| WCS_SpaceCore | `5E389BB9F58B79A6` | 8.2.1 |
| WCS_M1A1 | `5D1880C4AD410C14` | 8.2.3 |
| WCS_T-72 | `5E0AB16BEB16D6A4` | 8.2.1 |
| WCS_Weapons | `65CF7AE8574E06D2` | 8.2.1 |
| WCS_RHS_Weapons | `65F929DF622BAD50` | 8.2.1 |
| WCS_Clothing | `6152CB0BD0684837` | 8.2.0 |
| WCS_Clothing_Assets | `6602C1EC7E5A4A87` | 8.2.0 |
| WCS_BMP-3 | `5B383D4CB27E0D54` | 8.2.1 |
| WCS_M2A2 | `63120AE07E6C0966` | 8.2.0 |

Пакеты пехоты и БМП в Workshop:

- [WCS_Weapons](https://reforger.armaplatform.com/workshop/65CF7AE8574E06D2-WCS_Weapons)
  — `65CF7AE8574E06D2`;
- [WCS_RHS_Weapons](https://reforger.armaplatform.com/workshop/65F929DF622BAD50-WCS_RHS_Weapons)
  — `65F929DF622BAD50`;
- [WCS_Clothing](https://reforger.armaplatform.com/workshop/6152CB0BD0684837-WCS_Clothing)
  — `6152CB0BD0684837`, включая зависимость
  [WCS_Clothing_Assets](https://reforger.armaplatform.com/workshop/6602C1EC7E5A4A87-WCS_Clothing_Assets)
  — `6602C1EC7E5A4A87`.

- [WCS_BMP-3](https://reforger.armaplatform.com/workshop/5B383D4CB27E0D54-WCS_BMP-3);
- [WCS_M2A2](https://reforger.armaplatform.com/workshop/63120AE07E6C0966).

Все перечисленные зависимости входят в launcher graph. Дополнительно
установленный `WCS_M2A2_Upgrade` не требуется и в проверенный graph не включён.
Установленные файлы не изменяются, контент не копируется в репозиторий.

## Запуск и проверки

Из корня репозитория, после terminal Workbench Validate по `docs/DEVELOPMENT.md`
с новым root `AIConflictArlandWCSRHS/addon.gproj`:

```powershell
& .\tools\Start-AICFRuntime.ps1 -Role Server -Variant ArlandWCSRHS `
  -RhsAddonsRoot 'C:\Users\retar\OneDrive\Документы\My Games\ArmaReforger\addons'
```

`RhsAddonsRoot` здесь указывает общий установленный Workshop-каталог RHS/WCS.
Launcher выбирает новый header, полный dependency graph и отдельный свежий
profile, печатает `AICF_RUNTIME_MANIFEST_JSON`. Предварительный `-DryRun`
показывает exact arguments без запуска native process. Клиент автоматически
не подключается.

```powershell
pwsh -NoProfile -File tools/Test-WCSIntegrationStatic.ps1
pwsh -NoProfile -File tools/Test-RHSIntegrationStatic.ps1
pwsh -NoProfile -File tools/Test-RuntimeLauncherStatic.ps1
```

`Test-WCSIntegrationStatic` проверяет project/catalog boundaries, authority,
inventory transaction и отсутствие отложенного overwrite. Fixture
`tools/fixtures/AICF_WCSCatalogProbe.c` копируется **только в отдельный stage**,
валидируется Workbench и запускается canonical launcher с `-aicfWCSProbe 1`.
Она проверяет исходный порядок RHS до дополнения, отсутствие дубликатов,
загрузку WCS ресурсов, native цену/слоты там, где spawner data существуют,
и закрывает сервер после `ROSTER_READY`.
Для полного теста добавляется `tools/fixtures/AICF_WCSInfantryProbe.c` с
`-aicfWCSInfantryProbe 1`: два roster по 10 ролей создаются через production
async spawn. Проверяются выданный kit, сохранённые медицинские/ПТ/радио
предметы, совместимый боезапас, идемпотентность, точный baseline редактора и наличие всех обязательных вещей в ITEM catalog; дополнительно создаются
13 ролей FIA для проверки прежнего PMC armament contract. Дополнительно проверяются
отсутствие ПНВ, очки, рюкзаки по ролям и два fresh персонажа с player preset:
полное равенство commander draft, отказ для другой faction и исключение
сохранённого arsenal loadout. Это server-side helper test; сам клиентский
spawn/respawn и JIP требуют отдельного ручного прогона. Оба fixture помещаются только
в отдельный stage; клиент не нужен. Catalog PASS требует хотя бы один IFV
с active native spawner data на каждой стороне.

`tools/fixtures/AICF_WCSAccessProbe.c` проверяет native-фильтры конфигурации
построенного арсенала, регистрацию и faction/trait labels тяжёлого завода,
а также непустые раздельные списки трёх вкладок. Fixture работает только в
stage вместе с infantry probe; не подтверждает физическую покупку через UI.

Evidence актуальных комплектов: `.codex-runtime/wcs-realism/result.md`.
Evidence предыдущих исправлений: `.codex-runtime/wcs-feedback/result.md`.
Evidence первого этапа комплектов: `.codex-runtime/wcs-complete/result.md`.
Предыдущий этап: `.codex-runtime/wcs-arland/result.md`.
Последний server probe: 20/20 kits, 2/2 player presets, 13/13 FIA; арсенал 12/12 обязательных ресурсов на каждой стороне; factory labels/registry 68/68 US и 5/5 USSR. Полный runtime log не чистый: прежние 102 E (WORLD 56, ENTITY 7, RESOURCES 39), без новых уникальных ошибок относительно `wcs-feedback`. SCRIPT E/F и ENGINE F — 0.
Client/JIP, меню сценариев, реальная покупка/размещение, visual и soak — `NOT RUN`.
