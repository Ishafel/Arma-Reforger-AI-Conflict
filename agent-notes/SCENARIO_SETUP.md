# Пример экрана настройки Arland

Запрос владельца: показать рабочий пример одного пункта сценария с выбором
режима, стороны и сложности вместо отдельного пункта для каждой комбинации.
Это локальный прототип для vanilla Arland. Он не заменяет прежнюю матрицу
81 сценария: добавлен один новый пункт, старые GUID и запуск сохранены.

## Путь игрока

В «Сценарии» выбрать **AI Conflict — Арланд: пример настройки**. Штатные
действия Play или Host открывают отдельный экран до загрузки операции.
В Host сначала задаются обычные сетевые настройки сервера, затем параметры
операции. Экран предлагает Conflict или Co-op, США/СССР для Co-op и три
сложности гарнизонов. В Conflict сторона выбирается позднее в штатном deployment.
По умолчанию выбран Co-op СССР / Medium. Отмена возвращает в исходное меню.

После подтверждения запускается соответствующий существующий inherited header.
Подключающийся игрок использует обычный браузер/direct connect и получает
штатные Welcome, faction и group UI выбранного режима. Новый экран ему не
показывается. Сторона людей, roster, стройка и победа по-прежнему принадлежат
[COOP_SCENARIO.md](COOP_SCENARIO.md), найм — [GAMEPLAY.md](GAMEPLAY.md).

Это пример маршрутизации до запуска, **не перенос всех параметров на новый
runtime config** и не сокращение числа файлов на диске. Старые presets пока
нужны как проверенные launch targets. Следующий этап консолидации должен отдельно
решить, какие пункты скрывать и как сохранять совместимость конфигураций.

## Владельцы и native boundary

- `Config/AICF_ScenarioSetup.c`: `AICF_ScenarioSetupHeader` хранит девять ссылок
  integration; порядок — STANDARD/COOP_US/COOP_USSR × Easy/Medium/Hard.
  `ResolvePreset` проверяет границы, полноту, native metadata и тот же world;
  рекурсивный setup target запрещён. Неудачный Select сохраняет прошлый выбор.
- `UI/AICF_ScenarioSetupScreen.c`: локальные widgets, выбор и итоговая сводка.
  Использует существующие CreateRect/CreateText, штатные modal/focus API;
  Close удаляет handler/modal/widgets и допускает повторный вызов.
- `UI/AICF_ScenarioSetupEntry.c`: только собственный header subtype перехватывает
  `SCR_ScenarioUICommon.TryPlayScenario/TryHostScenario`. Для обычных headers
  вызов идёт в super с исходными аргументами. Закрытие ScenarioMenu/ServerHostingUI
  также закрывает экран.
- Подтверждение заново разрешает native `MissionWorkshopItem` и CanPlay/CanHost.
  Существующий `SCR_DSConfig` сохраняется; меняется только `game.scenarioId`
  на ID выбранного native mission. После закрытия экрана вызываются штатные
  TryPlay/TryHost, включая platform/download gates. Прямых Play/Host в обход
  этих gates, RPC, изменений текущего матча или нового replicated состояния нет.

Конкретный ресурс: `AIConflictArland/Missions/AICF_Conflict_Arland_Setup.conf`,
GUID `A1CF261019500001`. Он наследует прежний Co-op USSR Medium без собственной
world/base/layout topology. Прямой запуск ID в обход игрового меню получает
этот default; для dedicated server остаются canonical launcher и обычные presets.
Прототип не добавляет настройки в JSON `missionHeader` и не требует внешнего launcher.

## Проверки и ограничения

`Test-ScenarioSetupStatic.ps1`: соответствие девяти targets общему каталогу,
passthrough, opt-in, native permission/launch boundary, atomic choice и cleanup;
16 отрицательных mutations. Входит в общий offline набор.

`AICF_ScenarioSetupProbe.c` добавляется только в isolated Core/UI stage как
`AICF_ZScenarioSetupProbe.c` после основного Co-op probe. CLI `-aicfSetupProbe 1`.
Server проверяет 18 cases: native чтение subtype, defaults, девять leaf headers,
сопоставление реально запущенного header, bounds/null/incomplete/metadata/recursion
и сохранение прошлого выбора. Client проверяет 15 cases: создание настоящих
widgets, ненулевые bounds, native modal, девять комбинаций, локализованную сводку,
disabled стороны в STANDARD, неверный выбор и полную идемпотентную очистку.
UI input, нажатия клавиш, Play и Host fixture не эмулирует.

Строгий `Test-ScenarioSetupLog.ps1 [-Client]` сначала проверяет весь stopped log
через Co-op audit, затем exact CLI, cases и completion. Для client обязательны
настоящий player, Welcome и JIP readbacks. SCRIPT/VM/fatal не исключаются.

**NOT RUN / ограничение прототипа:** ручной путь из frontend через Play/Host,
mouse/keyboard/controller input, внешний вид на разных разрешениях. Игроку нужно
открыть новый пункт, проверить выбор/отмену и запуск нужного режима через оба
штатных действия. Terminal-only workflow запрещает Host UI/GUI automation;
canonical server/client запуск и native widget readbacks не доказывают этот
frontend переход. До такой проверки полноценная готовность UI не заявляется.
Packaged/Workshop build, другие карты/моды и длительный soak также не запускались.
Свежие команды и verdict — [TESTING.md](TESTING.md).
