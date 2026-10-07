# AI Conflict Everon WCS + RHS

Отдельный source addon `A1CF261006100001` с двумя сценариями:

| Launcher Variant | Сценарий | Resource GUID |
|---|---|---|
| `EveronWCSRHS` | Эверон (полный) | `A1CF261006100002` |
| `EveronNorthWCSRHS` | Север Эверона | `A1CF261006100003` |

Проект объединяет `AIConflictEveronRHS` и `AIConflictArlandWCSRHS` с их
зависимостями. Установленные RHS/WCS пакеты нужны те же, что для
[WCS + RHS Арланда](../AIConflictArlandWCSRHS/README.md).
Экипировка, редактор, арсенал и техника используют существующий WCS profile.
Его исторический ключ `WCS_RHS_ARLAND` сохраняется: рецепты относятся к тому
же набору контента, новая идентичность профиля не создаётся.

Full наследует `{57FA3D0337BE47E5}Missions/AICF_RHS_Conflict_Everon.conf`.
North наследует `{A1CF190919100000}Missions/AICF_RHS_Conflict_Everon_North.conf`:
два HQ и шесть точек захвата, включая Грабовую долину, радиосвязь 2000 м,
без основания новых баз. Весь ландшафт Эверона остаётся доступным.
World, whitelist, radio policy, callsign adapter и campaign lifecycle
остаются у зависимостей; новых production scripts в аддоне нет.
Каждый запуск начинает новую кампанию, сохранение сессии отключено.
Названия и описания имеют RU/EN локализацию.

## Запуск

Из корня репозитория, после терминального Workbench Validate нового
`AIConflictEveronWCSRHS/addon.gproj` по `agent-notes/DEVELOPMENT.md`:

```powershell
./tools/Start-AICFRuntime.ps1 -Role Server -Variant EveronWCSRHS
./tools/Start-AICFRuntime.ps1 -Role Server -Variant EveronNorthWCSRHS
```

Это альтернативные запуски; для одновременных серверов нужны разные порты.
Если Workshop-каталог не найден автоматически, передайте `-RhsAddonsRoot`.
`-DryRun` показывает graph и `AICF_RUNTIME_MANIFEST_JSON` без запуска.
Клиент подключается отдельно через тот же Variant и `-ServerProfileRoot`:
launcher проверяет живой сервер, точный CLI и `ROSTER_READY`.

## Проверки — 06.10.2026

Команда `pwsh -NoProfile -File tools/Test-<Audit>Static.ps1`:
ScenarioHeaders, EveronNorth, WCSIntegration, RHSIntegration,
RuntimeLauncher и Localization — PASS до и после, exit 0.
ScenarioHeaders проверяет наследование обоих headers и отсутствие новых
scripts/worlds; RuntimeLauncher проверяет server/client manifests обоих вариантов.
`Build-AICFLocalization.ps1` — PASS, 533 записи.
DryRun с установленными пакетами — PASS для обеих ролей обоих вариантов;
в manifests присутствуют семь непустых source resource databases.

Терминальный Workbench `-wbsilent -wbModule=ScriptEditor -run -validate`:
PASS, exit 0, `Script validation successful`, без SCRIPT E/F и ENGINE F.
Полный log содержит resource errors установленного контента; compile PASS
не означает отсутствие таких ошибок или успешную загрузку мира.
Первый запуск с server resources завершился native crash; повтор с game
resources внутри sandbox — SteamAPI_Init failed. Успешный запуск использовал
game resources и доступ к пользовательскому Steam вне sandbox.
Команды, manifests и полные логи сохранены в `.codex-runtime/everon-wcs/`.

Server runtime, client runtime, JIP, soak, меню сценариев и визуальная
проверка — NOT RUN. Аддон не опубликован в Workshop.
