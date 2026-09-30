# AI Conflict Arland WCS + RHS — Workshop metadata

Подготовка к релизу [0.1.23](../../releases/0.1.23.md). Поля переносятся в
Publish Project владельцем; наличие этого каталога не означает Workshop upload.
Исходники мода: [AIConflictArlandWCSRHS](../../AIConflictArlandWCSRHS/README.md).

## Поля Publish Project

- Project Name: `AI Conflict Arland WCS + RHS`
- Project GUID: `A1CF260928100001` — сохранить при публикации.
- Version: `0.1.23`
- Visibility первой публикации до проверки packaged build: `Test`
- Category: `Systems & Features`
- Tags: `AI CONFLICT ARLAND WCS RHS NATO RUSSIA`
- License: `Custom`, Apache License 2.0
- License File: `AIConflictArlandWCSRHS/license.txt`
- Preview Image: `workshop/AIConflictArlandWCSRHS/preview.jpg`

Preview повторно использует существующую обложку Arland RHS без изменений:
общая иллюстрация кампании на острове, не скриншот WCS. Сторонние ресурсы
WCS/RHS не входят в addon и сохраняют лицензии своих авторов.

## Прямые зависимости

Список соответствует [addon.gproj](../../AIConflictArlandWCSRHS/addon.gproj).
Публиковать AICF-зависимости раньше этого мода: Core → Arland → Arland RHS.

| Проект | GUID |
|---|---|
| Arma Reforger | `58D0FB3206B6F859` |
| AI Conflict Core | `9178E5822AFE48EA` |
| AI Conflict Arland | `B52C5F6AEDBF423E` |
| AI Conflict Arland RHS | `9F88011DA22B471C` |
| WCS_NATO | `615806DC6C57AF02` |
| WCS_RU | `615818DA7C0343FD` |
| WCS_Armaments | `629B2BA37EFFD577` |
| WCS_SpaceCore | `5E389BB9F58B79A6` |
| WCS_M1A1 | `5D1880C4AD410C14` |
| WCS_T-72 | `5E0AB16BEB16D6A4` |
| WCS_Weapons | `65CF7AE8574E06D2` |
| WCS_RHS_Weapons | `65F929DF622BAD50` |
| WCS_Clothing_Assets | `6602C1EC7E5A4A87` |
| WCS_Clothing | `6152CB0BD0684837` |
| WCS_BMP-3 | `5B383D4CB27E0D54` |
| WCS_M2A2 | `63120AE07E6C0966` |

## Дополнительные зависимости полного графа

Эти пакеты требуются зависимостями выше и входят в проверенный launcher graph:

| Пакет | GUID |
|---|---|
| RHS Content Pack 01 | `1337C0DE5DABBEEF` |
| RHS Content Pack 02 | `BADC0DEDABBEDA5E` |
| RHS - Status Quo | `595F2BF2F44836FB` |
| WCS_Weapon_Scripts | `68F006D910E7546F` |
| WCS_Attachments | `61C74A8B647617DA` |
| WCS_Scopes | `62A668F513428630` |
| WCS_Sounds | `631C3C1AEE9C90BC` |

Everon и Everon RHS для этого сценария не требуются. `WCS_M2A2_Upgrade` не
входит в проверенный набор. Версии, использованные в проверках исходников,
указаны в README мода; совместимость с последующими обновлениями требует проверки.

## Summary

Autonomous NATO-versus-Russia campaign on Arland with WCS infantry equipment
and vehicles on the RHS Conflict foundation. Requires AI Conflict, RHS and WCS dependencies.

## Description

AI Conflict Arland WCS + RHS adds the scenario `AI Conflict WCS + RHS - Arland`.
Both sides use AI commanders to select objectives, reinforce squads, capture
bases and manage ground transport, tickets and supplies on the RHS Arland map.

Infantry and default player equipment share faction-specific WCS kits, with
role-based weapons, armour and backpacks. The equipment editor offers
Vanilla, RHS and WCS categories. You can create a personal preset before
spawning and switch between it and Default loadout in the deployment gallery.

WCS items extend the arsenal. Abrams, T-72, BMP-3 and Bradley vehicles are added
to compatible heavy factories while retaining native prices, ranks and placement
requirements. This does not add autonomous tank tactics for AI commanders.

Install and enable this addon with all AI Conflict, RHS and WCS dependencies.
Open Scenarios and choose `AI Conflict WCS + RHS - Arland`, or join a server
running this scenario and the same dependency set. The Arland and Arland RHS
dependencies may also show their own scenario entries. Both factions use AI
commanders by default. Each campaign starts fresh; campaign session saves are disabled.

No WCS or RHS assets are redistributed. Apache License 2.0 applies to this
integration code; third-party packages retain their own licenses.

Русский:

AI Conflict Arland WCS + RHS добавляет сценарий **«AI Conflict WCS + RHS — Арланд»**:
НАТО против РФ на штатной RHS-карте. ИИ-командиры выбирают цели, пополняют
отряды, захватывают базы и управляют наземным транспортом, билетами и снабжением.

Пехота и стандартный комплект игрока используют общую экипировку WCS своей
стороны. Оружие, броня и рюкзаки распределены по ролям. В редакторе доступны
вкладки Vanilla/RHS/WCS, создание личного пресета перед появлением и выбор
между ним и Default loadout в штатной галерее возрождения.

Арсенал дополнен предметами WCS; тяжёлые заводы получают Abrams, Т-72, БМП-3
и Bradley со штатными ценами, рангами и ограничениями размещения. Автономная
танковая тактика ИИ-командиров не добавляется.

Включите этот мод и все перечисленные зависимости, затем в меню «Сценарии»
выберите **«AI Conflict WCS + RHS — Арланд»** либо подключитесь к серверу с
тем же сценарием и набором модов. По умолчанию обеими сторонами командует ИИ.
Каждый запуск начинает новую кампанию. Ресурсы WCS и RHS загружаются отдельно.

## Change Notes

0.1.23: подготовлена первая версия Workshop metadata для сценария Arland WCS + RHS.
Добавлены комплекты пехоты и игрока WCS, вкладки редактора, предметы арсенала
и техника тяжёлых заводов. Общие изменения личных пресетов, позывных,
возрождения и восстановления движения перечислены в [патчноутах](../../releases/0.1.23.md).

## Перед upload

Открыть проект `AIConflictArlandWCSRHS/addon.gproj` с полным графом зависимостей,
использовать license и preview из полей выше. После упаковки проверить
автоматическую загрузку зависимостей, плитку сценария и запуск в чистом profile.
Подготовка metadata не подтверждает packaged build, JIP, длительную игру
или покупку техники через меню; соответствующие проверки остаются отдельными gates.
