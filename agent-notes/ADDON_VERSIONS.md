# Версии установленных Workshop-аддонов — 01.11.2026

Проверка каталога, который canonical launcher использует как внешний addons root:
`C:/Users/retar/OneDrive/Документы/My Games/ArmaReforger/addons`.
Evidence: `.codex-runtime/addon-version-audit-20261101/`.

**42из42 установленных пакетов совпадают с актуальными опубликованными
версиями официального Workshop.** HTTP errors/unverified0; все выбранные
current versions имеют approved/published=true. Это dated inventory,
а не гарантия отсутствия обновлений после проверки.

Локальные `ServerData.json` и выбранная revision в `meta` согласованы42/42;
corrupted flags0. Для всех42пакетов присутствуют непустые `addon.gproj`,
`data.pak`, `resourceDatabase.rdb`. Полный content hash больших PAK не проверялся.
Остальные20каталогов с GUID —10пустых и10с remnants/manifests без payload;
ещё один каталог `saves` служебный. Они не засчитаны установленными аддонами.

Все6source graphs из `tools/config/AICF_Scenarios.json` проверены:
7local AI Conflict IDs +21external dependency IDs =28unique addons.
Недостающих graph addons и транзитивных зависимостей активного набора0.
Game, dedicated server и terminal Workbench имеют одинаковую установленную
версию1.8.0.13; её актуальность в Steam отдельно не проверялась.

Семь опубликованных AI Conflict пакетов имеют0.2.3, включая Core.
Workshop Core0.2.3 датирован24.10.2026; последующие изменения рабочего
репозитория и native проверки01.11 не следует выдавать за опубликованный0.2.3.
Source `.gproj` содержит GUID/dependencies, а не Workshop release version.
Проверка не готовит релиз, не меняет идентификаторы и не публикует исходники.

Разные номера WCS не означают устаревание: RU/Sounds8.3.0, большинство пакетов
8.4.0, Attachments/Core8.4.1 и ChernarusMinus8.1.0 — текущие версии своих
отдельных аддонов. Аналогично RHS Content Packs0.16.5236 и StatusQuo0.16.5237.

## Посторонние неполные наборы

У двух установленных пакетов вне AI Conflict graph обнаружены12missing
dependency edges /11unique missing IDs:

- `NOVA Everon PVE`: WCS_BMP-1, M113 APC, CIE Tracked Core,
  Vanilla Expanded - BTR80A, Realistic Combat Drones, WCS_Stryker, WCS_JLTV,
  CIE_Thermal, ATGM, Gaz Tigr, CIE BMP-2.
- `Realistic Combat Drones - RHS`: Realistic Combat Drones, общий с предыдущим.

Это недостающие пакеты, а не найденные старые версии установленных42.
Ещё6dependency edges декларируют более ранние версии уже установленных
актуальных пакетов; они сохранены в `dependencies.json`, не объявлены
доказанным нарушением runtime совместимости. Пакеты не устанавливались,
не обновлялись и не удалялись.

## Метод и gates

1. Прочитать `ServerData.json`, selected revision `meta` и наличие payload.
2. Получить каждую официальную страницу `/workshop/<id>/changelog` через
   `Invoke-WebRequest`; сохранить полный HTML. Из наблюдаемого structured
   `__NEXT_DATA__.props.pageProps.asset` сверить id, `currentVersionNumber`
   и свойства соответствующей revision. Это read-only запрос сайта.
3. Сопоставить GUID source graphs, проверить транзитивную metadata closure,
   отдельно перечислить missing dependencies посторонних модов.
4. Прочитать `VersionInfo` трёх native executable; `git diff --check`.

Итоговые collectors завершились успешно. Первичная inline closure попытка
завершилась exit1 без результата; явный повтор `Closure.ps1` дал exit0,
28reachable/missing0. Version gate42/42PASS; AI Conflict dependency
presencePASS; глобальная полнота всех посторонних модовFAIL.
Compile/runtime/визуальная совместимость всех42пакетов **NOT RUN** в этой
задаче: это сверка версий и metadata. Она не снимает FPS/runtime/resource
FAIL из [WCS_HIGHLOAD_SOAK.md](WCS_HIGHLOAD_SOAK.md).

JSON evidence: `installed.json`, `online-comparison.json`, `verdict.json`,
`local-metadata-check.json`, `dependencies.json`, `graphs.json`,
`active-closure.json`, `native-versions.json`, `empty-directories.json`.
`<GUID>.html` — полные официальные ответы, `REPORT.md` — отчёт.

## Полная сверка

В каждой строке установленная версия равна `currentVersionNumber` официальной
страницы по ссылке; все42строки получили CURRENT.

| Аддон / официальный changelog | Установлено | Текущая версия | Game Version текущей revision |
|---|---|---|---|
| [AI Conflict Arland](https://reforger.armaplatform.com/workshop/B52C5F6AEDBF423E/changelog) | 0.2.3 | 0.2.3 | 1.8.0.13 |
| [AI Conflict Arland RHS](https://reforger.armaplatform.com/workshop/9F88011DA22B471C/changelog) | 0.2.3 | 0.2.3 | 1.8.0.13 |
| [AI Conflict Arland WCS RHS](https://reforger.armaplatform.com/workshop/A1CF260928100001/changelog) | 0.2.3 | 0.2.3 | 1.8.0.13 |
| [AI Conflict Core](https://reforger.armaplatform.com/workshop/9178E5822AFE48EA/changelog) | 0.2.3 | 0.2.3 | 1.8.0.13 |
| [AI Conflict Everon](https://reforger.armaplatform.com/workshop/A4B2E62595F645A4/changelog) | 0.2.3 | 0.2.3 | 1.8.0.13 |
| [AI Conflict Everon RHS](https://reforger.armaplatform.com/workshop/FA9FDCCA428A43BA/changelog) | 0.2.3 | 0.2.3 | 1.8.0.13 |
| [AI Conflict Everon WCS RHS](https://reforger.armaplatform.com/workshop/A1CF261006100001/changelog) | 0.2.3 | 0.2.3 | 1.8.0.13 |
| [Chernarus Minus](https://reforger.armaplatform.com/workshop/665D1AA55B5D8076/changelog) | 2.1.6 | 2.1.6 | 1.8.0.13 |
| [ConflictPVERemixedVanilla2.0](https://reforger.armaplatform.com/workshop/61B514B96692C049/changelog) | 2.6.20 | 2.6.20 | 1.8.0.13 |
| [LinearConflictPVE](https://reforger.armaplatform.com/workshop/628729E87E79DA7F/changelog) | 1.5.7 | 1.5.7 | 1.8.0.13 |
| [Minus Building Pack](https://reforger.armaplatform.com/workshop/672166DE5926F11F/changelog) | 1.0.68 | 1.0.68 | 1.8.0.13 |
| [NOVA Chernarus PVE Vanilla](https://reforger.armaplatform.com/workshop/697E531B23C7F597/changelog) | 1.0.14 | 1.0.14 | 1.7.0.54 |
| [NOVA Everon PVE](https://reforger.armaplatform.com/workshop/66C65AD1056A8D76/changelog) | 1.0.0 | 1.0.0 | 1.6.0.54 |
| [Outside Terrain Core](https://reforger.armaplatform.com/workshop/61F0D506562D9BC9/changelog) | 1.1.0 | 1.1.0 | 1.2.1.202 |
| [Rail Enfusion Redux](https://reforger.armaplatform.com/workshop/5DE612963E4B005A/changelog) | 1.0.12 | 1.0.12 | 1.7.0.49 |
| [Railway Generator](https://reforger.armaplatform.com/workshop/5F0D245931200FD1/changelog) | 1.0.5 | 1.0.5 | 1.3.0.130 |
| [RB_AFRF2USSR](https://reforger.armaplatform.com/workshop/6A64552B08B948A0/changelog) | 1.0.3 | 1.0.3 | 1.8.0.13 |
| [Realistic Combat Drones - RHS](https://reforger.armaplatform.com/workshop/65D08E0825A12E2C/changelog) | 0.0.9 | 0.0.9 | 1.7.0.54 |
| [RHS - Content Pack 01](https://reforger.armaplatform.com/workshop/1337C0DE5DABBEEF/changelog) | 0.16.5236 | 0.16.5236 | 1.8.0.13 |
| [RHS - Content Pack 02](https://reforger.armaplatform.com/workshop/BADC0DEDABBEDA5E/changelog) | 0.16.5236 | 0.16.5236 | 1.8.0.13 |
| [RHS - Status Quo](https://reforger.armaplatform.com/workshop/595F2BF2F44836FB/changelog) | 0.16.5237 | 0.16.5237 | 1.8.0.13 |
| [WCS_Armaments](https://reforger.armaplatform.com/workshop/629B2BA37EFFD577/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_Attachments](https://reforger.armaplatform.com/workshop/61C74A8B647617DA/changelog) | 8.4.1 | 8.4.1 | 1.8.0.13 |
| [WCS_BMP-3](https://reforger.armaplatform.com/workshop/5B383D4CB27E0D54/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_ChernarusMinus](https://reforger.armaplatform.com/workshop/68CCF2D43167657E/changelog) | 8.1.0 | 8.1.0 | 1.8.0.13 |
| [WCS_Clothing](https://reforger.armaplatform.com/workshop/6152CB0BD0684837/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_Clothing_Assets](https://reforger.armaplatform.com/workshop/6602C1EC7E5A4A87/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_Core](https://reforger.armaplatform.com/workshop/64610AFB74AA9842/changelog) | 8.4.1 | 8.4.1 | 1.8.0.13 |
| [WCS_M1A1](https://reforger.armaplatform.com/workshop/5D1880C4AD410C14/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_M2A2](https://reforger.armaplatform.com/workshop/63120AE07E6C0966/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_M2A2_Upgrade](https://reforger.armaplatform.com/workshop/6A3873E87AD57606/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_NATO](https://reforger.armaplatform.com/workshop/615806DC6C57AF02/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_RHS_Attachments](https://reforger.armaplatform.com/workshop/6A717EEA63A35E67/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_RHS_Scopes](https://reforger.armaplatform.com/workshop/3B8081D9CAD979D2/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_RHS_Weapons](https://reforger.armaplatform.com/workshop/65F929DF622BAD50/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_RU](https://reforger.armaplatform.com/workshop/615818DA7C0343FD/changelog) | 8.3.0 | 8.3.0 | 1.8.0.13 |
| [WCS_Scopes](https://reforger.armaplatform.com/workshop/62A668F513428630/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_Sounds](https://reforger.armaplatform.com/workshop/631C3C1AEE9C90BC/changelog) | 8.3.0 | 8.3.0 | 1.8.0.13 |
| [WCS_SpaceCore](https://reforger.armaplatform.com/workshop/5E389BB9F58B79A6/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_T-72](https://reforger.armaplatform.com/workshop/5E0AB16BEB16D6A4/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_Weapon_Scripts](https://reforger.armaplatform.com/workshop/68F006D910E7546F/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
| [WCS_Weapons](https://reforger.armaplatform.com/workshop/65CF7AE8574E06D2/changelog) | 8.4.0 | 8.4.0 | 1.8.0.13 |
