# Пополнение пехоты

`INFANTRY` появляется с одним бойцом, сохраняя выбранный размер отряда
`1..10` как цель набора. Это относится и к новой группе после полного
уничтожения. Другие unit types создаются с заданной численностью. Полная
замена группы сохраняет прежние tickets и supply pricing, рассчитанные
по фактическому deployment size: default infantry replacement стоит
1 ticket и 50 supplies. Начальный roster бесплатный.

Живой пехотный отряд пополняется без нового slot или group generation.
Набор использует существующую последовательность ролей content profile.
Если погиб медик, освобождается именно его позиция. Учитываются конкретные
живые entities в своей группе: stock campaign randomizer может создать
concrete prefab с другим именем. Разрешённый stock source-roster fallback
сохраняется; RHS fallback к stock запрещён. Цена fallback определяется по
фактической роли исходного roster, обычный campaign randomizer стоит 10.

| Роль | Supplies | CLI override, диапазон 1..1000 |
|---|---:|---|
| Обычный боец, командир, помощник | 10 | `aicfRecruitRiflemanCost` |
| Медик | 15 | `aicfRecruitMedicCost` |
| Гранатомётчик, AT | 20 | `aicfRecruitGrenadierCost` |
| Пулемётчик, автоматчик | 20 | `aicfRecruitSpecialistCost` |

За этих бойцов tickets не снимаются. Цена относится к одному бойцу,
поэтому необязательно иметь supplies на весь недостающий состав.

## Выбор казармы и возврат к приказу

Решение принимается после `ROSTER_READY`, только для автономно управляемой
пехоты с боевым назначением. Явный player intent и ожидание приказа игрока
имеют приоритет; такой отряд самостоятельно в казарму не уходит.
Motorized группы, включая их временный infantry fallback, не участвуют.

Текущая база определяется по ближайшему узлу штатного graph. Допускается она
или сосед с одним radio edge в любую сторону. База должна принадлежать своей
фракции, не захватываться, иметь зарегистрированную `ONLINE BARRACKS` своей
фракции и supplies на следующего бойца. Расстояние считается от живого
лидера/участника до самой службы, максимум 500 м. При равенстве расстояний
сохраняется первый кандидат в порядке graph nodes и зарегистрированных services.

Если отряд уже в пределах 35 м от казармы, он может заказать бойца сразу.
Для перехода на соседнюю базу казарма должна быть строго ближе текущей боевой цели. При отсутствии
подходящей казармы приказ остаётся в силе. Дальнего заказа или доставки
бойцов к отряду нет: новый солдат появляется у казармы после подхода группы.

Визит сохраняет стратегическое намерение; временным waypoint владеет
`AICF_OrderPlanner`. Отряд возвращается к сохранённому приказу после набора,
исчерпания supplies, потери службы, изменения graph или timeout. Подход
ограничен 180 секундами, весь визит — 300, ожидание одного spawn — 30.
Повторный поиск откладывается на 60 секунд; между заказами — минимум 3.
Одновременно материализуются максимум два бойца, их pending capacity входит
в общий managed-agent budget. После достижения цели набора новых заказов нет.

## Владение и транзакция

- `AICF_GroupSlot` хранит желаемую численность, deployment size и identities
  позиций roster; lifecycle группы остаётся прежним.
- `AICF_InfantryRecruitmentService` обслуживает визиты из server/master
  `Update()`, без собственных callbacks. Проверяет faction, slot, group/entity
  identity, generation, assignment/intent/graph revisions и request token.
- `AICF_OrderPlanner` создаёт и удаляет временный waypoint, затем
  восстанавливает durable intent. Новый приказ игрока отменяет старый визит.
- `AICF_InfantryRecruitSpawner` создаёт отдельный одиночный donor, используя
  `RequestSpawn(1)`. Перед передачей требуется точный живой faction roster,
  физическая близость казармы, AI ownership, replication authority и `VETERAN`.
- `AICF_EconomySystem` повторно проверяет supplies и близость, снимает точную
  цену перед синхронным transfer и возвращает фактически снятое при отказе.
  Пока очередь spawn не завершена, деньги не снимаются.

`Stop()` отменяет визиты до остановки экономики. Cleanup очищает штатную
spawn queue, проверяет identity и отсутствие player-controlled donor member;
удаляет только принадлежащий запросу donor. Waypoint сначала снимается
с группы, затем удаляется. Переведённый боец остаётся в исходной managed group.

## Проверки

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-InfantryRecruitmentStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-InfantryRecruitmentLog.ps1 -LogPath '<полный остановленный console.log>' -RequireFullRosters
```

Для focused runtime временно скопируй
`tools/fixtures/AICF_InfantryRecruitmentRuntimeProbe.c` в Core `Forces`.
Canonical launcher: `Start-AICFRuntime.ps1 -Role Server -Variant Stock`
или `-Variant RHS` с `-AdditionalArguments @('-aicfRecruitProbe','1',
'-aicfRequirePlayerForResult','0')`. Fixture бесплатно готовит настоящие
казармы и supplies; только slot 0 каждой стороны сохраняет цель 10,
остальные получают цель 1. Все подходы, набор и платежи идут через production.
Проверяются два полных roster и неизменные group identities. Fixture
завершает процесс сама. После удаления временной копии повтори static audits
и отдельные Workbench Validate для stock/RHS.

`INFANTRY_RECRUITMENT_STARTED/ARRIVED/FINISHED` описывают визит;
`INFANTRY_RECRUIT_SPAWN_REQUESTED/DEBITED/JOINED/REFUNDED` — покупку.
Общий ключ: `faction + slot + generation + token`; в одном визите может быть
несколько последовательных покупок. Полный log проверяется отдельно от
event trace, включая startup и shutdown errors.

Дополнительная runtime matrix: потеря медика, недостаток supplies на
следующего бойца, уничтожение/смена владельца казармы, смена unit type,
новый player order во время очереди, deadline/недостижимая казарма, смерть
отряда и controller Stop. Для client/JIP и визуального поведения требуется
отдельный прогон; без ручной проверки визуальный gate остаётся `NOT RUN`.
