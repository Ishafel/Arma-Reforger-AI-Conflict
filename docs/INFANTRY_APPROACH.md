# Разные маршруты подхода пехотных отрядов

Дальний `ATTACK BASE` у пехоты получает последовательность промежуточных
`Move`, чтобы несколько отрядов с общей целью не шли к одному waypoint.
Выбор базы по radio graph, состав отряда и штатный строй не меняются.

`AICF_InfantryApproachRoute` сохраняет начало подхода и identity базы.
Numeric slot задаёт постоянную боковую полосу: 0/1 — −40/+40 м,
2/3 — −80/+80 м, 4/5 — −120/+120 м. Следующий участок продвигается
примерно на 220 м вдоль оси от начала подхода к базе. Боковой вынос
ограничен 200 м и 20% длины исходного направления. Поэтому повторная
выдача не добавляет боковое смещение заново и не закручивает маршрут.
Разные стартовые позиции отрядов дают немного разные оси.

Каждая точка проецируется на загруженный navmesh с допустимым горизонтальным
сдвигом не более 8 м и вертикальным — 3 м. Вода отклоняется; обычный участок
должен сокращать расстояние до базы минимум на 30 м после проекции.
Во время загрузки следующего tile выдаётся временный `Move` у проверенной
позиции живого leader. Ожидание загрузки ограничено 30 секундами.
Если пригодной точки нет, дальнейшее распределение этого подхода отключается
и используется прежний маршрут к базе. Запрос tile не дублируется.

Waypoint создаёт и удаляет только `AICF_OrderPlanner`. Slot хранит геометрию
до смены базы, сброса intent или смены group generation. Каждый промежуточный
waypoint использует существующий boundary `MarkStuckRouteWaypoint`:
group/generation/assignment/waypoint identity, `ALL`, физическая проверка
leader и ограниченное ожидание stock completion. Близость leader сама по
себе не подтверждает завершение стратегического приказа. У waypoint есть
дополнительный признак `IsApproachRouteWaypoint`, а переходы записываются с
причинами `INFANTRY_APPROACH_LEG_ARRIVED` и
`INFANTRY_APPROACH_COMPLETION_TIMEOUT`. Старые stuck events сохраняются;
для нового подхода у ожидания добавляется `route_kind=DISTRIBUTED_APPROACH`.
`INFANTRY_APPROACH_ASSIGNED` содержит `tile_wait`: временное ожидание загрузки
не следует считать движением по отдельной полосе. Быстрый task audit вызывает
planner перед проверкой meaningful task, чтобы нормальное завершение участка
не стало recovery. Активные vehicle ownership/restore и pending order recovery
сохраняют приоритет; accounting незавершённой проверки не обходится.

Внутри pending recovery завершение промежуточного участка проверяется через
`HasCompletedApproachLeg`: текущие group/generation/assignment, callback именно
этого waypoint и живой leader внутри его completion radius. Это относится и к
короткому Move ожидания tile: отсутствие перемещения здесь допустимо и не
означает ложного достижения базы. Старый repair attempt закрывается как
`SUPERSEDED` до замены waypoint; счётчик false completion не увеличивается.
Удаление waypoint, callback вдали от endpoint и устаревший context эту ветку
не проходят.

При настоящем false completion recovery исключает окрестность последнего
отвергнутого endpoint в пределах completion radius. `GetReachablePoint` сам по
себе не заменяет проверку воды; она применяется отдельно на 0,5 м ниже
navmesh endpoint. API проверяет погружение, а точка navmesh может быть чуть
выше водной поверхности; проверка ровно в endpoint давала false negative.
Такая проверка сохраняет возможность прохода по высоким мостам над водой.
Проекция query origin
ограничена 8 м по горизонтали и 3 м по высоте, чтобы не начать поиск маршрута
на удалённой поверхности. При отсутствии безопасной точки сохраняется штатный
bounded fallback, без принудительного перемещения бойцов.

В пределах последних 250 м по оси или фактическому расстоянию planner
возвращается к прежнему endpoint базы. Захват и локальный SearchAndDestroy
работают по прежним правилам. `POSITION`, relay smart action, DEFEND,
SYSTEM_HOLD, recruitment, lone-survivor retreat и vehicle waypoints не
получают такие полосы. Stuck/false-completion recovery отключает полосы
текущего подхода, чтобы не возвращать отряд к неудачной промежуточной точке.

Проверка endpoint на navmesh не доказывает связность всего пути. Штатная
навигация выбирает путь между точками; мосты, ограды и узкие проходы могут
сводить отряды вместе. При недостижимости действует существующий bounded
recovery. Это распределение маршрутов подхода, без гарантии полностью
непересекающихся путей и без изменения строя бойцов внутри отряда.

## Проверки

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-InfantryApproachContracts.ps1
```

Контрактный audit проверяет scope, tile lifecycle, ограничение проекции,
water guard, identity, cleanup и отсутствие побочных эффектов у geometry
helper. Дополнительно проверяется порядок продолжения маршрута относительно
task audit, safety recovery и physical/identity guards завершения участка.
Двенадцать отрицательных мутаций должны быть отклонены.

`tools/fixtures/AICF_InfantryApproachProbe.c` включается только в отдельную
копию Core. Она проверяет геометрию десяти полос, поворот оси, продвижение,
финальный участок, отключение после recovery, затем выдаёт двум настоящим
отрядам одну базу и наблюдает разные endpoints и движение. Состав в fixture
ограничен одним бойцом, чтобы пополнение не подменяло изучаемый приказ.
Это не доказательство поведения полных десятиместных отрядов.
Fixture дополнительно проверяет completion через синхронные fault injections:
callback рядом/далеко, removal, смена generation и assignment revision.

`AICF_ApproachRecoveryProbe.c` наблюдает штатные отряды 420 секунд после
`ROSTER_READY`, записывает позиции US/A4, выбранные recovery endpoints и
штатно завершает сервер. Координаты из исходного сбоя находятся только в
fixture. Проверка API непосредственно в трёх подозрительных endpoint из исходного
лога вернула false; это не доказывает отсутствие воды под точкой. Fixture
дополнительно записывает `foot_water` на 0,5 м ниже и высоту terrain.
Evidence исправления: `.codex-runtime/false-completion-fix-20260927/`.
Финальный семиминутный stock North run: native exit 0, false completion 0,
SCRIPT E/F, ENGINE F и VM 0. Прибрежный A4 (в этом запуске USSR) перешёл
к `ATTACK_OBJECTIVE_ACTION` в 93,9 м от базы. Regression completion guards:
24/24; финальные static audits и Workbench Arland/EveronRHS — PASS.
Resource/world/pathfinding diagnostics сохранены в полном evidence;
client/JIP, RHS runtime и длительный бой после этого исправления — NOT RUN.

Команды, baseline, полные Workbench/server logs и отдельные verdict текущей
проверки: `.codex-runtime/infantry-distribution-20260927/result.md`.
На 2026-09-27: восемь существующих static audits до/после — PASS, baseline
failures в выбранном наборе отсутствуют; новый audit и шесть отрицательных
мутаций — PASS. Workbench Validate stock Arland и полного EveronRHS — PASS.
Финальный stock EveronNorth server: 23/23 checks, native exit 0, SCRIPT E/F,
ENGINE F и VM/null — 0; resource/world/entity/material ошибки сохранены.
Визуальная оценка разнесения полных отрядов, client/JIP и длительный бой
требуют отдельных проверок; static/compile их не заменяют.
