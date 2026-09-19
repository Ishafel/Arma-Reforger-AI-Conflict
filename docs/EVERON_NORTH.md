# Север Эверона — стандартный и RHS

Два отдельных сценария используют одинаковую игровую область:

| Вариант launcher | Плитка | Фракции и зависимости |
|---|---|---|
| `EveronNorth` | AI Conflict — Север Эверона | Стандартные США/СССР; Core, Arland, Everon, без RHS |
| `EveronNorthRHS` | AI Conflict RHS — Север Эверона | RHS USMC/MSV и установленный RHS |

Стандартная версия использует `AIConflictEveron/addon.gproj` и ресурс
`{A1CF190919300000}Missions/AICF_Conflict_Everon_North.conf`.
RHS версия использует `AIConflictEveronRHS/addon.gproj` и ресурс
`{A1CF190919100000}Missions/AICF_RHS_Conflict_Everon_North.conf`.
Название и описание выбираются по языку клиента, RU/EN.

## Игровая область

Две стартовые базы: северная авиабаза и военный госпиталь на южном краю
игровой области. Штатный Conflict распределяет их между сторонами при старте;
постоянная привязка конкретной фракции к северу или югу не задана.
Помимо HQ доступны **шесть точек захвата**, всего восемь активных баз:

| Роль | Место | Entity name штатной базы |
|---|---|---|
| HQ | Северная авиабаза | `MilitaryBaseAirfield` |
| HQ | Военный госпиталь | `MilitaryHospital` |
| Захват | Saint-Philippe | `MainBaseNorth` |
| Захват | Maiden's Bay | `SmallBaseMaidensBay` |
| Захват | Meaux | `TownBaseMeaux` |
| Захват | Tyrone | `TownBaseTyrone` |
| Захват | Kermovan | `TownBaseKermovan` |
| Захват | Hornbeam Valley | `SmallBaseHornbeamValley` |

Победа определяется общим `AICF_VictorySystem`: у противника недостаточно
билетов для восстановления и не осталось управляемых боевых групп.
Штатная территориальная победа отключена; захват всех шести точек сам по себе
матч не завершает. HQ не захватываются.
Радиус связи каждой из восьми баз — 2000 м: северная сеть связана без relay
южного острова. Остальные базы не инициализируются; создание дополнительных
баз отключено через `m_bEstablishingBasesEnabled 0`.

Ландшафт всего Эверона остаётся загруженным и доступным для перемещения.
Это ограничение кампании, без невидимой стены, обрезки terrain и удаления
южных объектов. Поэтому сокращение числа активных баз не означает пропорционального
снижения потребления памяти. Полные `Everon` и `EveronRHS` остаются отдельными сценариями.

## Запуск

Из корня репозитория, сервер в отдельной терминальной сессии:

```powershell
./tools/Start-AICFRuntime.ps1 -Role Server -Variant EveronNorth
```

После вывода `AICF_RUNTIME_PROFILE` подключить клиент из другой сессии:

```powershell
./tools/Start-AICFRuntime.ps1 -Role Client -Variant EveronNorth `
  -ServerProfileRoot '<AICF_RUNTIME_PROFILE сервера>' `
  -AdditionalArguments @('-language', 'ru_ru')
```

Для RHS в обеих командах заменить `EveronNorth` на `EveronNorthRHS`.
Launcher проверяет точный server CLI, живой процесс и `ROSTER_READY`.
В server CLI `-server` получает GUID ресурса **header**. Запуск raw `.ent`
с отдельным `-MissionHeader` в Diag 1.8.0.13 оставляет `GetMissionHeader()`
пустым, и stock campaign не применяет whitelist. Не заменять launch target
путём world. Полный addon graph и установка RHS: [RHS_EVERON.md](RHS_EVERON.md).

## Проверки

`tools/Test-EveronNorthStatic.ps1` проверяет в обеих версиях восемь баз, два HQ, шесть
control points, радиус связи, запрет новых баз, наследование world и GUID.
`Test-RuntimeLauncherStatic.ps1` проверяет server/client manifests нового
вариантов, включая отсутствие RHS в стандартном graph и запуск без установленного
каталога RHS; `Test-LocalizationStatic.ps1` включает все шесть плиток.

`tools/fixtures/AICF_EveronNorthProbe.c` копируется только в отдельный stage
Core для terminal runtime проверки с `-aicfNorthProbe 1`. Она проверяет
активные базы и HQ на сервере и клиенте, готовность командиров и связность
радиографа на сервере. После результата клиент закрывается; сервер закрывается
через 150 секунд, оставляя время для direct-connect/JIP. Fixture не входит
в production addons. Результаты и ограничения: [TESTING.md](TESTING.md).
