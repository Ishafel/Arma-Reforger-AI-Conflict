# Публикация в Arma Reforger Workshop

## Статус и границы

Проект состоит из двух stock-compatible addon и отдельного опционального RHS
root-addon. Порядок публикации:

| Addon | GUID / Workshop ID | Назначение |
|---|---|---|
| `AIConflictCore` | `9178E5822AFE48EA` | Общая server-authoritative логика и UI |
| `AIConflictArland` | `B52C5F6AEDBF423E` | Интеграция со stock Conflict Arland; зависит от Core |
| `AIConflictArlandRHS` | `9F88011DA22B471C` | RHS USMC/MSV integration; зависит от Core, Arland и RHS packages |

GUID менять нельзя. Первый upload выполняется владельцем вручную через
стабильную Arma Reforger Tools: CLI-параметр `-publishAddon` предназначен для
обновления уже опубликованного проекта и использует metadata предыдущей
Workbench-сессии. Experimental Tools публикуют в отдельную Experimental
Workshop и для обычного релиза не используются.

Официальные справочные страницы:

- [Mod Publishing Process](https://community.bohemia.net/wiki/Arma_Reforger%3APublishing_Process);
- [Workbench Startup Parameters](https://community.bohemia.net/wiki/Arma_Reforger%3AStartup_Parameters);
- [Mod Project Setup](https://community.bohemia.net/wiki/Arma_Reforger%3AMod_Project_Setup).

Codex не выполняет вход в Bohemia Account и не управляет Workbench GUI. Все
остальные подготовительные файлы находятся в репозитории.

## Источники release metadata

| Данные | Файл |
|---|---|
| Core metadata | `workshop/AIConflictCore/metadata.md` |
| Core preview | `workshop/AIConflictCore/preview.jpg` |
| Arland metadata | `workshop/AIConflictArland/metadata.md` |
| Arland preview | `workshop/AIConflictArland/preview.jpg` |
| RHS metadata | `workshop/AIConflictArlandRHS/metadata.md` |
| RHS preview | `workshop/AIConflictArlandRHS/preview.jpg` |
| Custom license Core | `AIConflictCore/license.txt` |
| Custom license Arland | `AIConflictArland/license.txt` |
| Custom license RHS | `AIConflictArlandRHS/license.txt` |

`manifest.json` и packaged files не создаются вручную и не коммитятся:
Resource Publisher генерирует их в выбранном Working Dir.

## Preflight

1. Использовать одинаковую stable-версию Game, Server и Tools.
2. Проверить чистоту release commit и записать полный SHA.
3. Выполнить актуальные gates из `docs/TESTING.md`. Для изменения только
   metadata/preview: проверить ссылки, `git diff --check`, идентичность всех
   `license.txt` корневому `LICENSE` и размер каждого preview не более 2 MiB.
4. Для релиза production-кода дополнительно получить свежие Workbench
   Validate/Compile и применимый server/client runtime evidence.
5. Создать резервную копию source checkout. Packaged addon нельзя восстановить
   обратно в редактируемые исходники.

## Первая публикация Core

1. В stable Arma Reforger Tools добавить существующий проект
   `AIConflictCore/addon.gproj` и открыть его в Resource Manager.
2. Выполнить `Workbench -> Login`/`Link` и войти в Bohemia Account владельца.
3. Выбрать `Workbench -> Publish Project`.
4. Перенести поля из `workshop/AIConflictCore/metadata.md`.
5. Для Preview Image выбрать `workshop/AIConflictCore/preview.jpg`.
6. Оставить Working Dir в стандартном publish-каталоге либо выбрать другой
   каталог вне репозитория и вне `AIConflictCore`.
7. Выбрать `Custom license`; полный Apache-2.0 текст находится в
   `AIConflictCore/license.txt`.
8. Выполнить первую публикацию с visibility `Test`.
9. На созданной Workshop-странице проверить ID `9178E5822AFE48EA`, version,
   preview, summary, description и license.

## Первая публикация Arland

1. В том же stable Tools открыть `AIConflictArland/addon.gproj`. Workbench
   должен видеть vanilla project и локальный `AIConflictCore`.
2. В `Workbench -> Options` проверить зависимости:
   `58D0FB3206B6F859` и `9178E5822AFE48EA`.
3. Выбрать `Workbench -> Publish Project` и перенести поля из
   `workshop/AIConflictArland/metadata.md`.
4. Для Preview Image выбрать `workshop/AIConflictArland/preview.jpg`, Working
   Dir держать вне исходников, license выбрать `Custom`.
5. Выполнить публикацию с visibility `Test`.
6. На Workshop-странице проверить ID `B52C5F6AEDBF423E` и наличие
   `AI Conflict Core` в списке Dependencies.

Если Core не появился как dependency, Arland нельзя переводить в Public:
сначала исправляется обнаружение dependency и повторяется upload.

## Первая публикация RHS root-addon

1. Сначала опубликовать и проверить Core и Arland.
2. В stable Tools открыть `AIConflictArlandRHS/addon.gproj` с установленными RHS
   `0.16.5150` packages.
3. Проверить GUID `9F88011DA22B471C` и dependencies:
   `58D0FB3206B6F859`, `9178E5822AFE48EA`, `B52C5F6AEDBF423E`,
   `1337C0DE5DABBEEF`, `BADC0DEDABBEDA5E`, `595F2BF2F44836FB`.
4. Выбрать `Workbench -> Publish Project` и перенести поля из
   `workshop/AIConflictArlandRHS/metadata.md`.
5. Для Preview Image выбрать `workshop/AIConflictArlandRHS/preview.jpg`, Working
   Dir держать вне исходников, license выбрать `Custom`; полный текст находится
   в `AIConflictArlandRHS/license.txt`.
6. Первый upload выполнить вручную с visibility `Test`; не переиспользовать
   stock entry так, чтобы скрыть RHS dependency.
7. На Workshop-странице проверить ID `9F88011DA22B471C`, version, metadata и
   наличие Core, Arland и трёх RHS packages в Dependencies.
8. Проверить автоматическую загрузку всех dependencies на чистом profile.

## Проверка packaged build

Проверка из локальных исходников не доказывает исправность Workshop-пакета.
После публикации применимых Test addons:

1. Использовать отдельный свежий game profile и Workshop addons directory,
   которые не видят checkout репозитория.
2. Скачать/включить `AI Conflict Arland` и доказать, что Core скачался как
   dependency.
   Для RHS отдельно скачать/включить RHS root-addon и доказать загрузку Arland,
   Core и трёх RHS packages.
3. Для stock entry запустить stock Conflict Arland. Для RHS entry запустить
   штатную RHS mission `{7577640CD42A00BD}Missions/RHS_Conflict_Arland.conf`.
   Собственной scenario tile AICF addon не добавляют.
4. На dedicated server загрузить применимый Workshop dependency set и сохранить
   полный остановленный server log; для UI/replication также сохранить client
   log.
5. Проверить отсутствие `ADDON_LOAD_ERROR`, `SCRIPT (E/F)`, `ENGINE (F)`,
   `Virtual Machine Exception` и `NULL pointer`, затем применить runtime
   анализаторы из `docs/TESTING.md`.
6. Визуальный UI остаётся `NOT RUN`, пока пользователь не выполнит ручную
   проверку.
7. Только после этого перевести требуемые Workshop entries из `Test` в
   `Public` и записать опубликованные version/date в release notes.

## Обновления

Каждый функциональный release затронутого addon увеличивает version в формате
`major.minor.patch`. Если меняются несколько addon, зависимости публикуются и
проверяются раньше потребителей: Core -> Arland -> RHS. Arland-only или RHS-only
update не требует фиктивного обновления неизменных dependencies. В Change Notes
перечисляются только изменения данной версии. Изменение visibility, preview или
description также создаёт новую Workshop version.

После успешной первой публикации можно отдельной задачей добавить
воспроизводимый terminal update workflow с `-packAddon`/`-publishAddon`. Он не
должен подменять ручной initial upload или хранить account credentials в
репозитории.
