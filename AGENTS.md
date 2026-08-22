# Arma Reforger AI Conflict — инструкции для агентов

## Назначение

Этот файл задаёт постоянный рабочий контракт для Codex и других агентов в
репозитории. Он не заменяет продуктовые требования, карточку текущей работы,
нормативные тестовые документы или явные решения владельца продукта.

Цель процесса — доводить ограниченную работу до проверяемого результата и
передавать её владельцу в состоянии `READY_FOR_ACCEPTANCE`. Агент не принимает
собственную работу и не расширяет продуктовый scope самостоятельно.

## Язык и термины

- Объяснения, продуктовые требования и отчёты пиши по-русски.
- Имена файлов, классов, методов, CLI options, команды и строки логов сохраняй
  в исходном английском виде.
- Не переводи нормативные статусы: `DRAFT`, `READY`, `IN_PROGRESS`,
  `VERIFYING`, `READY_FOR_ACCEPTANCE`, `ACCEPTED`, `REJECTED`,
  `PASS`, `FAIL`, `BLOCKED`, `NOT RUN`.
- Не переименовывай существующие `[AICF]` diagnostic events ради стиля.

## Иерархия источников истины

При конфликте используй следующий порядок:

1. Явная инструкция владельца продукта в текущей задаче.
2. Активная карточка в `product/work-items/`.
3. Зафиксированное решение владельца или acceptance record.
4. Короткое текущее состояние в `product/CURRENT.md`.
5. Продуктовые границы и архитектурные принципы в `PROJECT_VISION.md`.
6. Нормативный Stage-контракт в соответствующем `docs/STAGE_*_TESTING.md`.
7. Проверенные API и version caveats в `docs/API_REFERENCE.md`.
8. `README.md` как обзор и историческая сводка.
9. Исторические repeat-, defect- и runtime-отчёты.

Исторический отчёт не отменяет более новое явное owner decision, но его
`PASS`, `FAIL`, `BLOCKED` или `NOT RUN` нельзя переписывать задним числом.
Если конфликт меняет scope, критерии приёмки или продуктовый результат,
остановись и запроси решение владельца.

## Контекст и экономия токенов

- Сначала открой активную work item и `product/CURRENT.md`.
- Затем используй `docs/CONTEXT_INDEX.md` и читай только указанные разделы.
- Не загружай все Stage-документы, весь `README.md` или полные runtime logs без
  доказанной необходимости.
- Для поиска используй `rg` и точечное чтение файлов.
- Передавай модели вывод анализаторов и минимальный релевантный фрагмент лога,
  а не полный `console.log`.
- Сохраняй исходные логи как evidence; в Git фиксируй компактный manifest,
  verdict, путь и при необходимости hash.
- Не повторяй в новых документах большие исторические описания: ставь ссылку на
  канонический источник.

## Когда нужна work item

- Диагностика, объяснение и read-only review могут выполняться без новой
  карточки, если владелец не просит изменить состояние проекта.
- Перед изменением product code, нормативных тестов или статуса продукта должна
  существовать активная work item.
- Явная команда владельца «реализуй», «исправь» или «проверь» считается
  разрешением оформить указанную работу как `READY`, но scope всё равно нужно
  записать до существенных изменений.
- Не начинай задачу со статусом `DRAFT`, `BLOCKED`, `REJECTED` или
  `ACCEPTED`.
- Одновременно у одного writer должна быть только одна активная work item.

## Жизненный цикл work item

`DRAFT -> READY -> IN_PROGRESS -> VERIFYING -> READY_FOR_ACCEPTANCE`

Нормальные переходы и actor:

| From | To | Actor | Условие |
|---|---|---|---|
| `DRAFT` | `READY` | владелец продукта или агент по его явной команде | scope и acceptance записаны |
| `READY` | `IN_PROGRESS` | назначенный writer | baseline branch/commit записаны |
| `IN_PROGRESS` | `VERIFYING` | writer | изменения завершены, начата проверка |
| `VERIFYING` | `READY_FOR_ACCEPTANCE` | writer | пакет evidence подготовлен |
| `READY_FOR_ACCEPTANCE` | `ACCEPTED` | только владелец продукта | результат и deviations приняты |
| `READY_FOR_ACCEPTANCE` | `REJECTED` | только владелец продукта | записаны причина и следующее действие |
| `REJECTED` | `READY` | только владелец продукта | разрешён новый ограниченный цикл работы |

`BLOCKED` — боковое состояние исполнения, а не сокращённая замена
`READY_FOR_ACCEPTANCE`:

- writer может перевести `READY`, `IN_PROGRESS` или `VERIFYING` в `BLOCKED`,
  когда проверяемый результат невозможно получить из-за внешней среды,
  отсутствующего решения, несовместимой версии, сборки или неустранённого
  предусловия;
- при входе обязательны исходный статус, blocker, evidence, последний
  завершённый gate, точное условие возобновления и actor, который может его
  подтвердить;
- если условие возобновления объективно проверяемо и не требует нового scope
  или authority, writer может после его подтверждения выполнить только
  `BLOCKED -> READY`; если требовалось продуктовое решение или новое
  разрешение, этот переход выполняет владелец;
- прямые `BLOCKED -> IN_PROGRESS`, `BLOCKED -> VERIFYING` и
  `BLOCKED -> READY_FOR_ACCEPTANCE` запрещены;
- после `BLOCKED -> READY` новый writer снова записывает baseline и начинает
  обычную цепочку с `IN_PROGRESS`.

Далее только владелец продукта устанавливает итоговый статус:

- `ACCEPTED` — результат принят, включая явно перечисленные deviations;
- `REJECTED` — результат возвращён с причиной и следующим действием.

Трудность или длительность сами по себе не являются `BLOCKED`. Work-item status
`BLOCKED` не меняет verdict уже выполненных gates: технические `PASS`, `FAIL`,
`BLOCKED` и `NOT RUN` фиксируются отдельно.

## Технические verdict

- `PASS`: конкретный gate выполнен на записанном commit и подтверждён
  требуемым evidence.
- `FAIL`: проверка реально стартовала и обнаружила нарушение контракта.
- `BLOCKED`: проверка не смогла дойти до проверяемого поведения.
- `NOT RUN`: проверка не выполнялась.

Обязательные инварианты:

- `NOT RUN` никогда не равен `PASS`.
- Static audit, Workbench validation и runtime проверяют разные свойства.
- Static или Workbench `PASS` не повышает runtime gate до `PASS`.
- Успешное существование процесса не доказывает soak `PASS`.
- Runtime verdict присваивается после анализа полного остановленного server/client
  log, если Stage-контракт не требует иного.
- Несколько прогонов можно объединять в acceptance только когда контракт это
  разрешает и каждый run привязан к точному commit и environment metadata.

## Scope и изменения

- Изменяй только пути из `Allowed paths` активной work item.
- Не трогай `Forbidden paths` и `Non-goals`.
- Делай минимальное изменение, достаточное для критериев приёмки.
- Не исправляй соседние дефекты без отдельной work item или явного разрешения.
- Сохраняй существующие пользовательские изменения и не включай их в свой diff.
- Не меняй project IDs:
  - `AIConflictCore`: `9178E5822AFE48EA`;
  - `AIConflictArland`: `B52C5F6AEDBF423E`;
  - vanilla dependency: `58D0FB3206B6F859`.
- Не копируй и не изменяй vanilla world без отдельного архитектурного решения.
- Сохраняй server-authoritative модель стратегических решений, экономики,
  владения силами и завершения матча.

## Git и параллельная работа

- Одна work item соответствует одной ветке или одному worktree и одному
  проверяемому outcome.
- Перед работой запиши baseline branch и commit.
- Не используй destructive Git commands и не удаляй пользовательские изменения.
- Не смешивай независимые задачи в одном commit.
- Два агента не должны одновременно изменять одну рабочую копию или одни файлы.
- Для двух независимых writer используй отдельные Git worktrees.

## Делегирование

- По умолчанию работай одним агентом.
- Используй subagents только если владелец или активная work item явно разрешает
  параллельную работу.
- Одновременно запускай не более двух вспомогательных subagents.
- Вспомогательные агенты по умолчанию read-only: exploration, API research,
  review, log triage или summarization.
- Только один агент является writer для work item.
- Запрещено рекурсивное делегирование без явного разрешения владельца.
- Результат subagent должен быть компактным: факты, ссылки на файлы/строки,
  verdict, риски и blocker; без больших raw dumps.

## Проверка изменений

Выбирай команды по `docs/CONTEXT_INDEX.md` и активной work item.

Основные анализаторы:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage3Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage35Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage35RecoveryPolicy.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage4Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage2Log.ps1 -LogPath <absolute-console.log>
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage4Log.ps1 -LogPath <absolute-console.log>
```

Правила:

- После изменения Enforce Script выполни релевантный static audit.
- Workbench `Validate Scripts` и `Compile and Reload Scripts` обязательны,
  когда их требует work item или Stage-контракт.
- Если Workbench/ServerDiag/client недоступен, укажи `NOT RUN` или
  `BLOCKED` по точному определению; не симулируй результат.
- Runtime run должен использовать зафиксированные версии, commit, чистый profile
  и параметры запуска из нормативного Stage-документа.
- Любое изменение product code после runtime run инвалидирует runtime evidence
  для нового commit и требует повторной проверки.
- Не запускай M30 или S120 до прохождения их предусловий.

## Evidence

Для каждого проверочного run фиксируй:

- work item ID;
- commit SHA и наличие/отсутствие dirty changes;
- версии Arma Reforger, Tools и Server;
- профиль, world, CLI options, server/client roles;
- время начала и завершения;
- команды и exit codes;
- пути к полным server/client/Workbench logs;
- hash или другой устойчивый идентификатор значимого артефакта;
- verdict каждого gate и объяснение `FAIL`, `BLOCKED` или `NOT RUN`;
- screenshots/video, если критерий визуальный.

Не добавляй большие runtime logs в Git, если work item явно этого не требует.

## Документация

- `product/CURRENT.md` содержит только короткое актуальное состояние.
- `product/work-items/` содержит scope и критерии отдельных работ.
- `product/acceptance/` содержит компактные пакеты для решения владельца.
- `docs/CONTEXT_INDEX.md` маршрутизирует агента к нужным источникам.
- Нормативный Stage-документ меняй только при изменении контракта, а не ради
  записи каждого запуска.
- Исторический evidence не редактируй для создания более благоприятного статуса.
- Раздел `Recognized baseline` в `CURRENT.md` обновляй только после явного
  решения владельца.

## Передача владельцу

Финальный отчёт должен быть коротким и самодостаточным:

1. Outcome.
2. Изменённые файлы.
3. Таблица verification: gate, verdict, evidence.
4. Известные риски и deviations.
5. Что осталось `NOT RUN` или `BLOCKED`.
6. Какое решение требуется от владельца.

Не объявляй работу `ACCEPTED` и не выполняй merge только на основании
собственного review.
