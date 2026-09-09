# Маркеры машин логистики

С `2026-09-08` оформление вынесено в общий `AICF_MapMarkerCardWidget` для
логистики и [отрядов](GROUP_MAP_MARKERS.md). Значок «Л» выровнен по центру,
у подписи появились единые отступы и полоска цвета фракции. Hover работает
на значке и фоне карточки; содержимое логистической сводки сохраняется.

Машина под управлением службы логистики отображается на карте своей фракции
маркером «Л». Рядом находятся её номер, название модели из каталога, состояние
и текущие припасы / вместимость. При наведении на значок открываются подробности:

- модель и состояние;
- фактический груз и вместимость физических containers;
- источник → получатель либо возврат груза / возврат на базу;
- расстояние до текущей цели по прямой, скорость в км/ч;
- база приписки.

Маршрут здесь — назначение рейса между базами, не линия дорожного пути.
Расстояние не является длиной дороги или обещанием времени доставки.
Если cargo pool ещё не готов или потерял identity, выводится `? / ?`, а не
неподтверждённый нулевой груз. Отменённый job или job другого поколения не
отображается как действующий рейс. Названия моделей и баз переводятся через
штатный `WidgetManager.Translate` при построении серверного снимка.

Различаются ожидание задания, подготовка, готовность к рейсу, поездка за грузом,
погрузка, доставка, разгрузка, возврат, ожидание повторной попытки, водитель у
препятствия, попытка выбраться, неисправность и завершение службы. Recovery
показывается только при активном recovery context; нулевая скорость сама по себе
не выдаётся за застревание.

Снимок обновляется каждые 2 секунды, позиция следует штатному dynamic marker
за самой машиной. Для JIP подпись и подробности хранятся в `RplProp`.
Репликация текста помечается изменённой только при изменении значений.
Штатные faction stream rules ограничивают доступ к самому marker entity.
Маркер снимается при потере custody, vehicle identity, фракции, уничтожении,
завершении cleanup или остановке службы. Replacement получает новый marker;
неизменившаяся машина сохраняет прежний. Depot без физической машины не получает
маркер машины. Потеря depot не скрывает управляемую машину с возвращаемым грузом.

## Изменённые файлы

- `AIConflictCore/Scripts/Game/AIConflict/UI/AICF_LogisticsMapMarkers.c` — read model,
  identity records, синхронизация и программный виджет.
- `AIConflictCore/Scripts/Game/AIConflict/UI/AICF_GroupMapMarkers.c` — kind `2`,
  реплицируемые подробности, hover; прежние kind `0/1` сохранены.
- `AIConflictCore/Scripts/Game/AIConflict/Economy/AICF_LogisticsService.c` —
  вызовы marker `Sync` и `Stop`.
- `tools/Test-LogisticsMapMarkersStatic.ps1` — 20 статических контрактов.
- `tools/fixtures/AICF_LogisticsMapMarkerProbe.c` — изолированная серверная fixture.
- `README.md`, `docs/ARCHITECTURE.md`, `docs/TESTING.md`, этот документ.

## Проверки 2026-09-07

Evidence находится в `.codex-runtime/logistics-markers-20260907/`, вне Git.
Baseline снят до изменений. Выполнялась команда
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-<Name>.ps1`:

| Name | Baseline | После изменения |
| --- | --- | --- |
| Stage3Static | PASS | PASS |
| Stage35Static | PASS | PASS |
| Stage3StaticContracts | PASS | PASS |
| Stage4Static | PASS | PASS |
| LogisticsStatic | PASS | PASS |
| LogisticsContracts | PASS, 110 cases | PASS, 110 cases |
| MapPointOrdersStatic | PASS | PASS |
| LogisticsMapMarkersStatic | новый audit | PASS, 20 contracts |

Baseline failures в этом наборе отсутствовали; проверки не ослаблялись.
`git diff --check` — PASS.

Терминальный Workbench запускался через
`ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent -gproj <addon.gproj>
-addonsDir <stock-addons,repository[,RHS-addons]> -addons <graph-ids>
-logsDir <evidence-directory> -wbModule=ScriptEditor -run -validate`.
Графы и команды соответствуют `DEVELOPMENT.md`:

| Граф | Exit code | Validate/Compile | SCRIPT E/F; ENGINE F |
| --- | --- | --- | --- |
| Everon | 0 | PASS | 0; 0 |
| Arland | 0 | PASS | 0; 0 |
| Arland RHS | 0 | PASS | 0; 0 |

Сохранены полные `console.log`, `script.log`, `error.log`, stdout и exit codes
в `workbench-everon/`, `workbench-arland/`, `workbench-rhs/` и соседних файлах.

Серверная команда финальной fixture:

```powershell
& tools/Start-AICFRuntime.ps1 -Role Server -Variant Everon `
  -RepositoryRoot "$PWD/.codex-runtime/logistics-markers-20260907/probe-source" `
  -ProfileRoot "$env:LOCALAPPDATA/AICF/Server-LogisticsMarkers-20260907-2328" `
  -AICommanderMode BOTH -AdditionalArguments @('-aicfLogisticsMarkerProbe','1')
```

Fixture создаёт отдельные машины из каталогов `US` и `USSR`, привязывает
настоящие fleet leases и вызывает production marker system. Она не подменяет
`VehicleIdentity` и не доказывает прохождение машины по маршруту.
**36 cases PASS**, `failures=0`: actual target и faction, текстовый снимок,
live cargo, throttle, обновление без дублей, recovery status, stale/cancelled job,
release, generation mismatch, несколько машин, исчезнувший worker, replacement,
cleanup, `Stop` и запрет создания после `Stop`.

Сервер штатно завершился через `RequestClose`, `Game destroyed`, exit `0`.
`AICF_RUNTIME_MANIFEST_JSON` сохранён в `runtime-manifest.txt/json`;
105 production `.c` совпали с изолированной копией по SHA-256
(`production-hashes.json`). Полные остановленные логи сохранены в
`stopped-server-logs/`; их полный разбор — `full-stopped-log-review.json`.

Runtime нельзя назвать полностью чистым: после завершения cases есть две
`SCRIPT (E): 'SCR_BaseResupplySupportStationComponent' needs a entity catalog
manager!` при teardown. ENGINE fatal нет. Resource/navmesh/Hierarchy diagnostics
также сохранены, не исключены из evidence. До cases SCRIPT errors не было.
Первый запуск fixture имел ошибку синтаксиса assignment в аргументе, второй —
неверный fixture RplId (`FindItemId` вместо `RplComponent.Id`). Оба исправлены;
их исходные profiles и launch logs сохранены отдельно от успешного запуска.
Оставшийся процесс первого неудачного запуска закрыт после сверки его profile;
финальная проверка процессов — `ArmaReforger* = 0`. Клиент не открывался.

## NOT RUN

Клиент не запускался. Внешний вид, hover, карта у края экрана, читаемость длинных
названий и разных масштабов, фактический JIP/faction streaming на клиентах
не проверены. Компиляция и серверные cases не заменяют эти gates.
Обычный gameplay рейс, движение по дорогам и доставка с открытой картой в этом
изменении не проверялись. Уничтожение машины и переход её фракции защищены
production gates и static audit, но отдельно не воспроизводились в runtime.

Ручная проверка: открыть карту своей стороны, найти «Л», навести на значок,
сравнить груз/состояние с рейсом; проверить обновление при погрузке и разгрузке,
движение маркера, смену фракции игрока и повторное подключение во время рейса.
Проверить отсутствие старого маркера после удаления/замены машины.
