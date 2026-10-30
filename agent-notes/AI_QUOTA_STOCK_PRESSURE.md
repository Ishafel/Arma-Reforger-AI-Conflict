# Контролируемая проверка stock-квоты и набора AICF

Это узкий серверный тест `Stock` / `BOTH`, а не обычный боевой soak,
проверка маршрута или подтверждение всех производителей AI. Он использует
штатное создание бойцов и настоящую покупку пополнения.

Тест сохраняет двадцать управляемых слотов и начальный состав из одного бойца.
Через обычные commander API он задаёт `US / slot 0` тип `INFANTRY` и удержание
исходной позиции. Дополнительные неуправляемые группы той же фракции заполняют
`STOCK`. Фракция сама по себе не является признаком владельца квоты.

Положительный функциональный результат требует одновременно:

- фактически материализованный `STOCK == stock_limit`, `reserved == 0`;
- отказ дополнительной stock-группе без бойца и изменения native intended;
- непрерывное сохранение этого давления минимум 35 секунд;
- нормальное принятие заявки пополнения исходного управляемого слота;
- четыре реальные `INFANTRY_RECRUIT_JOINED` и четыре соответствующих списания
  supplies, пять разных actor ID / Rpl ID и сохранение исходного бойца;
- неизменную группу и spawn generation.

Потеря непрерывного давления после начала окна завершает тест с FAIL. Native
pending, резерв квоты и физически созданный боец учитываются раздельно. Тест не
меняет здоровье, LOD, activation/count flags, общий лимит, положение бойцов или
readiness. Группы теста не удаляются вручную; они живут до штатного завершения
собственного мира. Максимум запуска — 600 секунд, успешное доказательство может
завершить его раньше. Это не 30-минутный gate.

## Воспроизведение

Поздние чистые controls на прежнем383 завершились functionalFAIL: спустя34–48s
после первоначального veto extra-группа получила counted/activated character с
прежним group/Rpl/parent identity. Это не доказывает новый birth или overshoot:
нужно отличить transfer и освобождённый unit. Новая fixture пишет scoped admission/
reservation/group birth/external factory receipts с actor ID, return status и
контекстом extra-группы. Обёртки вызывают real super один раз, не делают Refresh,
не меняют результат; limit2048/overflow и потеря scope явно видны. BudgetDetails —
cached ledger и отдельные after-decision native counters, не свежий полный census.

Trace-only supplement: actual19controls/5unsafe и независимый peer PASS;
полное удаление named trace blocks возвращает тот же policy token hash33EC2C8A…
исходной fixture70020F6D… (stage pressure-agent-kind-v2). Срок35s, full/veto/FAIL и
timeout не ослаблены. Prepare385 StockWB0/sourceSHA0; новый trace Native NOT RUN.

Из корня checkout, в PowerShell 7, с доступными установленными Server и Tools:

```powershell
pwsh.exe -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-AICFQuotaStockPressure.ps1 `
  -Phase Prepare -EvidenceRoot .codex-runtime/quota-stock-pressure-current

pwsh.exe -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-AICFQuotaStockPressure.ps1 `
  -Phase Run -EvidenceRoot .codex-runtime/quota-stock-pressure-current -ServerPort 2660
```

`-Phase Full` объединяет оба шага. `-SourceRoot` позволяет явно выбрать другой
fixture-free frozen checkout. Каталог evidence должен быть новым. `Prepare`
копирует все семь production addons, добавляет только две назначенные fixture,
замораживает log auditor и выполняет терминальный Workbench Validate/Compile.
`Run` требует тот же manifest, каждый source SHA, frozen auditor и успешные
полные compiler logs; обхода compile нет. Повторный запуск того же профиля
запрещён. Используются только `tools/Start-AICFRuntime.ps1`, `Stock`, `BOTH`.

Перед запуском нужно согласовать занятые native слоты: runner допускает максимум
два игровых процесса и требует более 8 GiB свободной памяти. Запуск client этому
тесту не нужен. Подготовка и компиляция не запускают игровой мир.

Полный verdict разделяет функциональный auditor, native exit, точный CLI,
завершение мира, отсутствие собственного процесса, source drift, SCRIPT/engine
fatal, project failures, ошибки ресурсов и FPS. Ошибки ресурсов и хотя бы один
sample FPS ниже 30 дают общий exit 1 даже при функциональном PASS. Просматриваются
все остановленные sibling logs целиком, включая строки после `DONE`.

## Наблюдатели и контракты

`AICF_AIQuotaStockPressureProbe.c` содержит сам тест и необязательный измеритель
существующих вызовов `RefreshCensus`: один вызов `super`, неизменный bool-результат,
агрегат раз в 15 секунд. Он не добавляет вызовов Refresh и не меняет cadence.
`AICF_AIQuotaBootReadbackProbe.c` раз в 15 секунд читает bounded native roster,
raw counters, отдельный диагностический census и текущую политику. Его snapshot
не используется для допуска фабрики или обновления ledger.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/contracts/Test-AIQuotaStockPressureProbeContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/contracts/Test-AIQuotaStockPressureLogContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/contracts/Test-AIQuotaStockPressureRunnerContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/contracts/Test-AIQuotaVariantContracts.ps1
```

Log auditor отдельно проверяет только указанный функциональный сценарий;
полный остановленный runtime gate принадлежит runner. Старый PASS нельзя
переносить на новый source manifest.

## Сохранённые результаты 2026-10-30

Evidence находится под `.codex-runtime/issue78-finalization-20261029/quota/`.

Первый `stock-pressure-native-v1` остановлен адресно после доказанного дефекта:
штатный `SpawnEntityPrefabEx(..., true)` возвратил разрешённый случайный вариант
`Campaign_US_AI`, а прежнее точное сравнение с исходным prefab поместило его в
quarantine. Результат остаётся ABORT / native -1 / functional FAIL / fatal 3 /
resources 24, а удержание тела и незакрытого допуска не отменялось.

Исправление относится только к групповой фабрике: до native constructor
фиксируется закрытый одноуровневый набор вариантов по pinned 1.8.0.13 metadata.
RNG и native `SpawnEntityPrefabEx(..., true)` сохранены. Допуск default повторяет
штатные fallback/weight условия; неизвестные, отрицательные или неподдерживаемые
метаданные не превращаются в разрешение. Реально полученный prefab фиксируется
для последующей identity/rollback проверки. Plain external factories, заголовки
групп и техника сохраняют прежнее точное сравнение prefab.

`stock-pressure-native-v2` на frozen 383 production + двух fixture завершился
штатно: native 0, frozen functional auditor 0, fatal 0, project failures 0,
own CIM 0, source drift 0. При `STOCK 128 / reserved 0` дополнительная группа
оставалась пустой 156024 ms, исходный `group 0132 / generation 1` сохранил бойца
`017A` и получил четыре оплаченных пополнения до пяти. Само доказательство
завершилось на 245381 ms; wall time 263.132 s. Зафиксированы 15 допусков реальных
штатных вариантов, включая исходный ранее падавший вариант.

Общий результат этого запуска остаётся FAIL: errors ресурсов 28, min FPS 22.2,
17 из 24 FPS samples ниже 30. Профиль `RefreshCensus` при 148 active достигал
304 вызовов / 9752 ms суммарно за 15-секундное окно, max 65 ms. Это измерение
полного вызова по wall clock, а не доказательство стоимости отдельного метода.
Соседний игровой мир с этим запуском не пересекался; часть времени параллельно
работали offline проверки и Workbench. Оптимизация не разрешает ослаблять
проверки UNKNOWN или повторные identity/counter fences.

Native ветка первоначально существовавшего ALIVE/INCAP uncounted бойца в этом
запуске не наблюдалась (`alive_inactive_uncounted=0`). Её extracted контракты
остаются отдельным gate. Universal quota PASS и original route PASS не заявлены.
Новый tracked runner перенесён после этого запуска; его собственные prepare,
контракты и будущие native результаты учитываются отдельно от исходного wrapper.
