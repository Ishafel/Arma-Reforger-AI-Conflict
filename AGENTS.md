# Arma Reforger AI Conflict — правила для Codex

## Назначение

Этот репозиторий — scripts-first расширение штатного Conflict для автономной
войны `US` против `USSR`. Разработка выполняется преимущественно в Enforce
Script; единственные gameplay resources проекта — тонкие inherited
`MissionHeader` для запуска stock/RHS вариантов из меню сценариев. Они не
владеют world, bases или layouts. Общие механики должны оставаться в
`AIConflictCore`, особенности Arland — в `AIConflictArland`.

Перед существенным изменением прочитай:

1. `README.md` — актуальные границы и структура проекта.
2. `docs/ARCHITECTURE.md` — владельцы состояния и побочных эффектов.
3. `docs/DEVELOPMENT.md` — окружение и запуск.
4. `docs/TESTING.md` — применимые проверки и текущий baseline.

Объяснения и проектную документацию пиши по-русски. Имена API, классов,
методов, CLI-параметров, событий и полей логов сохраняй на английском.

## Перед изменениями

- Проверь `git status` и не затрагивай несвязанные пользовательские изменения.
- Определи затронутый домен и прочитай его соседние классы, а не только место
  предполагаемой правки.
- Запусти релевантные статические проверки до правки и сохрани baseline.
  Текущий набор не полностью зелёный; сравнивай результат до и после.
- Для неизвестной сигнатуры Enfusion сначала ищи её в закреплённом Script Diff
  `1.8.0.10` под `.cache/reforger-api/`. Если кэша нет, используй
  `tools/fetch_reforger_api_reference.sh`. Не придумывай API по аналогии с C#.

## Только терминальный workflow

- Codex не использует Computer Use, screen capture, управление мышью/клавиатурой,
  GUI automation, screenshots или video во время разработки и тестирования.
- Workbench Validate/Compile запускай только из терминала через
  `ArmaReforgerWorkbenchSteamDiag.exe` по команде из `docs/DEVELOPMENT.md`. Не
  открывай Launcher, Workbench или Script Editor через GUI.
- Dedicated server и подключаемый к нему direct-connect client запускай только через
  `tools/Start-AICFRuntime.ps1` из отдельных терминальных сессий. Не генерируй
  ad-hoc `Start-Process` команды и не используй Launcher, Host UI или GUI-кнопки.
- Перед подключением client launcher обязан подтвердить в свежем server profile
  точный `CLI Params`, живой server process и `[ROSTER_READY]`; напечатанный
  `AICF_RUNTIME_MANIFEST_JSON` сохраняй как evidence целостности аргументов.
- Evidence собирай из команды, exit code и полных Workbench/server/client logs.
  Визуальный критерий, который нельзя доказать логами, помечай `NOT RUN`, пока
  пользователь не выполнит ручную проверку; не заменяй её скриншотом или
  самостоятельным управлением GUI.

## Неприкосновенные контракты

- Не меняй идентификаторы проектов:
  - vanilla dependency: `58D0FB3206B6F859`;
  - `AIConflictCore`: `9178E5822AFE48EA`;
  - `AIConflictArland`: `B52C5F6AEDBF423E`.
- Не копируй и не изменяй vanilla world, mission или установленные файлы игры.
- Не хардкодь координаты и имена баз Arland в Core. Топология берётся из
  штатных баз и radio graph.
- Стратегия, spawn/delete, экономика, билеты, владение силами и завершение
  матча изменяются только на authoritative server/master.
- Клиент отправляет только намерение. RPC на сервере заново определяет игрока,
  фракцию, слот, лимиты и допустимость цели.
- Реплицируемое состояние меняет только authority. Вызывай
  `Replication.BumpMe()` только после фактического изменения. Для JIP-состояния
  предпочитай `RplProp`, а не одноразовый RPC.
- Vehicle и economy subsystems всегда включены. Не возвращай CLI opt-out
  `aicfVehiclesEnabled` или `aicfEconomyEnabled`.
- Не переименовывай `[AICF][STAGE...]` events и их `key=value` поля без
  синхронного изменения анализаторов логов и документации.

## Идентичность и асинхронность

- Постоянная идентичность управляемой силы — `faction + numeric slot`.
  Role-local имя (`A0`, `D0`, `R0`) может измениться.
- Замена группы не создаёт новый slot. Для защиты от устаревших callback
  сохраняй group/spawn/trip generation, assignment revision, graph revision,
  request token и immutable entity identity там, где они уже используются.
- Каждый callback/retry должен отменяться при несовпадении identity. При
  неопределённости перед разрушительным действием выбирай fail-closed.
- `SCR_AIGroup` — controller entity; его origin не является положением отряда.
  Позицию получай через живого лидера или живого участника с помощью
  `AICF_GroupRuntime`.
- Асинхронный roster не готов сразу после `RequestSpawn()`. Не обходи
  существующие readiness/generation gates.
- Каждому event `Insert` и повторному `CallLater` должен соответствовать
  явный `Remove` в lifecycle `Stop`/cleanup.

## Владельцы обязанностей

Не переносить побочные эффекты между этими границами без отдельного
архитектурного решения:

| Область | Владелец |
|---|---|
| Infantry target и waypoint | `AICF_OrderPlanner` |
| Stable group lifecycle | `AICF_FactionState` + `AICF_GroupSlot` |
| Vehicle facade/scheduling | `AICF_VehicleCoordinator` |
| Переходы transport trip и cross-domain effects | `AICF_TransportTripController` |
| Acquisition/boarding/transit/dismount | соответствующий flow; он возвращает `AICF_TripOutcome` |
| Vehicle site/reservation и создание entity | `AICF_VehicleSpawner`, вызываемый acquisition flow только в своей фазе |
| Vehicle lease, generation и faction cap | `AICF_FactionFleet` |
| Vehicle waypoint boundary | `AICF_VehicleTaskHandoff` |
| Physical clearance, release и delete | `AICF_VehicleCleanupManager` |
| Ticket + supply transaction | `AICF_EconomySystem` |

Flow-компоненты не должны напрямую менять фазу trip, вызывать соседний flow,
controller или cleanup. Cleanup может продолжаться после завершения trip.
Waypoint сначала снимается с группы, затем удаляется как replicated entity.
Vehicle удаляется только после identity-safe и непосредственно повторённой
occupancy/clearance проверки.

`AICF_MatchController` уже является крупным composition root. Новую доменную
ответственность по возможности помещай в профильный класс и оставляй в
контроллере только orchestration. Не выполняй широкий рефакторинг этого файла
без явной задачи.

## Стиль Enforce Script

- Новые глобальные проектные символы начинай с `AICF_`.
- Следуй локальному стилю: tabs, Allman braces, guard clauses и типовые
  префиксы полей `m_i`, `m_b`, `m_f`, `m_s`, `m_a`, `m_v`.
- Не используй C-style ternary `?:`.
- Один `string.Format` поддерживает не более девяти подстановок `%1..%9`;
  длинные диагностические строки собирай несколькими вызовами.
- Сохраняй детерминированный порядок и стабильный tie-break при выборе цели,
  базы или spawn site.
- Не исправляй проверку под желаемый PASS, пока не установлено, что устарел
  именно контракт, а не реализация.

## Generated и vendor-файлы

Не редактируй и не добавляй в Git:

- `.cache/`, включая закреплённый Script Diff;
- `.idea/`, `.gigaide/`;
- `*.log`, каталоги `logs/` и `profile/`;
- `AIConflictCore/resourceDatabase.rdb` и
  `AIConflictArland/resourceDatabase.rdb`;
- установленные каталоги Arma Reforger, Server и Tools;
- `.codex-runtime/active-parallel-batch.txt`, если его создал smoke-helper.

## Проверка и передача результата

- Для Enforce-изменения запусти релевантные PowerShell-аудиты из
  `docs/TESTING.md` и сравни с baseline до правки.
- Static audit, Workbench compile и runtime — разные gates. Один не заменяет
  другой. `NOT RUN` не означает `PASS`.
- Изменение Enfusion API или production `.c` требует терминального Workbench
  Validate/Compile; если он недоступен, явно укажи это как `NOT RUN`/blocker.
- Runtime-вывод оценивай по полному остановленному server log и, когда затронут
  клиент, по client log. Отфильтрованные `[AICF]` строки — только индекс.
- Не объявляй работу `ACCEPTED` самостоятельно.
- В финальном отчёте перечисли outcome, изменённые файлы, выполненные команды с
  verdict, сохранённые baseline failures и всё, что не запускалось.
