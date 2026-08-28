# Проверки и evidence

## Семантика gates

Каждый уровень отвечает на отдельный вопрос:

| Gate | Что доказывает | Чего не доказывает |
|---|---|---|
| Static audit | Структурные и текстовые project contracts | Компиляцию Enforce и игровое поведение |
| Workbench Validate/Compile | Совместимость исходников с текущим Enfusion API | Server/client lifecycle и долгий runtime |
| Server runtime | Authoritative gameplay и server diagnostics | Клиентский UI, replication/JIP и визуальное поведение |
| Client runtime/logs | Запуск клиента, log-side ошибки и наблюдаемую в логах репликацию | Визуальный UI и controls без ручной проверки пользователя |
| Soak | Устойчивость на заданном времени и профиле | Поведение в неиспытанных конфигурациях |

`NOT RUN` не равен `PASS`. Запущенный процесс или отсутствие `[AICF][ERROR]` в
коротком фрагменте не образуют runtime PASS. Агент не присваивает результату
статус `ACCEPTED` без решения владельца.

## Terminal-only evidence

Codex не использует Computer Use, screen capture, GUI automation,
screenshots/video или управление мышью и клавиатурой для разработки и
тестирования этого проекта. Workbench, server и client запускаются только из
терминала командами из `docs/DEVELOPMENT.md`.

Evidence агента состоит из команд, exit codes и полных логов. Если критерий
можно проверить только визуально или через интерактивный UI, Codex фиксирует
его как `NOT RUN`. Такой критерий становится проверенным только после отдельной
ручной проверки пользователя; наличие запущенного окна этого не доказывает.

## Статические аудиторы

Запускай из корня репозитория:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage3Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage35Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage35RecoveryPolicy.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage4Static.ps1
```

Назначение:

| Script | Основной охват |
|---|---|
| `Test-Stage3Static.ps1` | vehicle architecture, acquisition, trip, cleanup и diagnostics contracts |
| `Test-Stage35Static.ps1` | force structure, vehicle/infantry integration и Enforce language audit |
| `Test-Stage35RecoveryPolicy.ps1` | точные recovery, timing, ownership и fail-closed policies |
| `Test-Stage4Static.ps1` | economy transaction, supply balance, replication, strategic UI и RPC authority |

Аудиторы проверяют часть архитектуры регулярными выражениями. Красный rule ID
может означать реальный regression или drift тестового контракта. Сначала
сопоставь правило с поведением и историей; не меняй production-код либо regex
только ради зелёного вывода.

## Зафиксированный baseline

Baseline получен `2026-08-28` на `main`, commit
`6ff6c550dc7c5a40d5876e3f96ddf57bfc64f107`, до создания этой документации.
После изменения HEAD его нужно запускать заново.

| Проверка | Exit | Результат |
|---|---:|---|
| `Test-Stage3Static.ps1` | 1 | `FAIL`, 5 issues |
| `Test-Stage35Static.ps1` | 1 | `FAIL`, 4 issues |
| `Test-Stage35RecoveryPolicy.ps1` | 0 | `PASS` |
| `Test-Stage4Static.ps1` | 0 | `PASS` |

Stage 3 baseline failures:

- `STAGE3_PROGRESS_EVIDENCE`: auditor ожидает five-minute objective timeout,
  код задаёт `120000` ms;
- два `STAGE3_RUNNING_CARGO_STALL` contracts;
- `STAGE3_BOUNDED_PROTECTED_CLEARANCE` reason signature;
- `STAGE3_MARKER_STATE`: auditor ожидает marker fragment `VEH `.

Stage 3.5 baseline failures:

- `STAGE35_FORCE_STRUCTURE`: auditor всё ещё ожидает floor `80`, текущий код
  использует более высокий `MIN_MANAGED_AGENTS = 128`;
- `STAGE35_MEANINGFUL_TASK_PROOF` queue evidence;
- `STAGE35_EXACT_CARGO_STALL`;
- `STAGE35_BOUNDED_PROTECTED_CLEARANCE` reason signature.

При работе в этих областях отчёт должен показывать pre-change и post-change
наборы failures. Новых failures быть не должно; исчезнувший failure объясняется
изменением реализации или осознанным обновлением контракта.

## Выбор проверок по области

| Изменение | Минимум |
|---|---|
| Только Markdown | проверить ссылки/команды, `git diff --check` |
| Config, bootstrap, faction/group state, forces, objectives, orders | релевантные static audits + Workbench Validate; runtime при изменении поведения |
| `Vehicles/` или `State/Vehicles/` | Stage 3, Stage 3.5 и RecoveryPolicy + Workbench + targeted runtime |
| `Economy/` | Stage 4 static + Workbench + server runtime/log audit |
| `UI/`, RPC или campaign replicated state | Stage 4 static + Workbench + server/client runtime; JIP при изменении snapshot |
| Arland `modded` integration | Workbench + canonical Arland server runtime; клиент, если меняется UI/replication |
| PowerShell analyzer | позитивный и негативный representative input; не ослаблять rule молча |
| Enfusion version/API | pinned reference diff + полный Workbench Validate по целевой версии |

Static audit после Enforce-изменения обязателен, но не заменяет Workbench.
Runtime, выполненный до последнего product-code изменения, не доказывает новый
commit.

## Workbench gate

Используй только терминальную Diag-команду из `docs/DEVELOPMENT.md`. Сохраняй:

- exact game/tools version;
- commit и dirty status;
- команду или последовательность действий;
- полный `console.log`, `script.log`, `error.log`;
- exit code;
- факт `Game successfully created`;
- количество `SCRIPT (E/F)`, `ENGINE (F)`, VM/null exceptions.

Platform/backend или shutdown resource messages нельзя автоматически скрывать.
Их нужно классифицировать отдельно от AICF compile error.

## Анализаторы runtime-логов

### Stage 2

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Test-Stage2Log.ps1 `
  -LogPath 'C:\absolute\path\console.log'
```

Опционально: `-MaxRepeatedOrderRecoveries 3`.

Известное ограничение: binding regex принимает только slots `[0-3]` и требует
минимум восемь bindings, тогда как текущая модель имеет slots `0-9` на каждую
фракцию. Поэтому `PASS` этого анализатора может быть false green для полного
20-slot roster и не заменяет ручную проверку всех `SPAWN_BOUND` identities.

### Stage 4

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\Test-Stage4Log.ps1 `
  -LogPath 'C:\absolute\path\console.log'
```

`Test-Stage4Log.ps1` требует постоянный always-on invariant
`[AICF][STAGE4][INFO][CONFIG] ... enabled=1` и `SUPPLY_PROBE`.
`-AllowActiveAtEnd` разрешает только незавершённые reservations/shipments на
момент остановки; errors он не игнорирует.

Сейчас отсутствуют:

- автоматический Stage 0/1 log verdict;
- специальный Stage 3/3.5 runtime log analyzer;
- client-log/visual validator;
- единая команда `test all` и CI.

Это пробелы покрытия, а не неявный PASS.

## Runtime contract

Запускай canonical Arland server/client из терминала по `docs/DEVELOPMENT.md`:

- новый server profile на каждый run;
- новый client profile, если нужен клиент;
- `-backendFreshSession`;
- raw Arland world вместе с `-MissionHeader` и `-worldSystemsConfig`;
- оба addon GUID;
- записанные `aicf*` flags;
- одинаковые версии Game, Server и Tools.

После завершения изучи полный остановленный log. Минимальный поиск:

```powershell
Select-String -LiteralPath $log.FullName -Pattern `
  '\[AICF\]|SCRIPT\s+\((E|F)\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer'
```

Префиксы приложения:

```text
[AICF][STAGE0]
[AICF][STAGE1]
[AICF][STAGE2]
[AICF][STAGE3]
[AICF][STAGE3.5]
[AICF][STAGE4]
```

Stage 2–4 используют Stage 1 `run` и `t_ms`, поэтому события одной сессии можно
сопоставлять без приблизительных wall-clock timestamps.

## Evidence checklist

Для каждого проверочного прогона запиши:

- цель и конфигурацию сценария;
- branch, full commit SHA и dirty status;
- Game/Server/Tools versions;
- server/client роли, world, mission header и systems config;
- полный CLI с `aicf*` flags;
- время начала/завершения и способ остановки;
- команды, exit codes и rule IDs;
- абсолютные пути к полным Workbench/server/client logs;
- отдельные verdict: static, compile, server, client, JIP, soak;
- ручной verdict пользователя для визуальных критериев либо `NOT RUN`; Codex
  сам screenshots/video не создаёт;
- все `FAIL`, `BLOCKED` и `NOT RUN` без повышения статуса.

Финальная передача должна отделять новый regression от сохранённого baseline и
явно перечислять непроверенные свойства.
