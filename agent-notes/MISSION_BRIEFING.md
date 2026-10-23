# Брифинг на карте

`AIConflictCore/Scripts/Game/AIConflict/UI/AICF_MissionBriefing.c` дополняет
штатный путь `SCR_MapJournalUI` → `SCR_RespawnBriefingComponent.LoadJournalConfig`.
При `SCR_GameModeCampaign` и campaign header с `m_sGameMode "AI Conflict"`
component создаёт локальный `AICF_MissionJournal`. Для остальных mission/game
modes вызывается native `super.LoadJournalConfig()`. Новая integration должна
сохранять эту метку в своём тонком header. World/base/layout ресурсы не меняются.

Журнал содержит десять страниц: операция; правила/победа; командование;
найм; экипировка/возрождение; снабжение/транспорт; пополнение; строительство;
противник/сложность; настройка/ограничения. Раздел правил выбирается по
immutable `m_eAICFMatchMode`: STANDARD либо общий Co-op для обеих сторон.
Стартовая страница показывает локализованное имя active header, режим и
сложность. Это справка о defaults, а не копия текущего gameplay state.

Одна journal config с пустым faction key использует штатный fallback:
страницы доступны до выбора стороны и для concrete faction keys RHS/WCS.
Список создаётся один раз на component, повторный Load не добавляет записи;
native ResetConfig разрешает повторное создание. IDs начинаются с 10000,
поэтому штатные updates старых briefing entry IDs не переписывают справку.
Порядок отображения штатный, по array index, начальная страница 0.

Текст и заголовки хранятся в `AICF_Localization.st` под `AICF_Briefing_*`.
Native entry layout переводит их при показе. Overview параметр вычисляется
в `SetEntryLayoutTo` для текущего языка; используется native форматирование
с `<br/>`. Локальный header читается и на JIP без ожидания roster или RPC.
Новых подписок, timers, replicated state и authority effects в production нет.
Все кнопки, прокрутка и открытие журнала принадлежат штатному UI.

Это отдельная от frontend Setup guide справка: подготовка/включение модов
кратко упомянуты, но меню запуска не открывается посреди матча. В справке явно
сохранены ограничения лидерства и лимита найма, отсутствие автоармии и
авторазмещения у людей Co-op, расходы и очередь пополнения, отсутствие save/resume.

Терминальная проверка: `Test-LocalizationStatic.ps1`,
`Test-ScenarioHeadersStatic.ps1`, `Test-CoopScenarioStatic.ps1`, затем Workbench
и isolated `AICF_MissionBriefingProbe.c` (см. TEST_FIXTURES). Создание native
страниц и чтение виджетов не доказывают физический ввод/визуальную оценку.
