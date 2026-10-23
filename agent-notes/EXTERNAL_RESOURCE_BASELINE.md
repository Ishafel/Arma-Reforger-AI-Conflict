# Внешние ошибки ресурсов и граница scripts-first

Владелец 23.10.2026 подтвердил: сохранить scripts-first, исправить код проекта,
а внешние дефекты зафиксировать. Собственные копии миров, navmesh, слоёв карт и
массовые prefab overrides ради чистого лога не добавляются; установленные файлы
игры/RHS/WCS не меняются. Это ограничение объёма исправления, не PASS чистоты логов.

## Доказательства источника

Исходная матрица `f5d2966`: девять dedicated runs по >=300000 ms, Standard/Hard,
Arland/Everon/North × Vanilla/RHS/WCS. Все native exits 0; 1325 console Error,
SCRIPT/VM/assertions 0. Evidence: `.codex-runtime/server-soak-20261023-215418/`.
`thematic-error-catalog.json` распределяет все 1325 строк без повторного подсчёта
дубликатов error.log. В `navigation-pair-proof.json` все 61 PATHFINDING сообщения
сопоставлены с непосредственно следующей ошибкой пустого ресурса на том же timestamp.

В `.codex-runtime/runtime-log-fixes-20261023-222235/` подготовлен отдельный
`native-control`: собственные addon GUID сохранены, все игровые `.c` проекта
исключены, собственные thin headers оставлены только с наследованием штатного
header. Единственный test script вызывает native `OnGameStart` и запрашивает
закрытие через 60 секунд; ни один world или установленный ресурс не копируется.
Сборка трёх control graphs прошла терминальный Workbench. Manifest, полный CLI,
exit и полные logs сохранены.

- Vanilla Everon: native exit 0, полный штатный shutdown; повторились unknown
  fields техники, duplicate Hierarchy, десять ошибок navigation tiles с пустым
  ресурсом и оставшаяся ссылка на шрифт. Игровых AICF scripts в этом control нет.
- RHS/WCS control: ошибки `m_fAILimitThreshold`, `CampaignTaskManager.et`,
  `SCR_BaseTaskManager` и Hierarchy появляются при загрузке мира без AICF.
  Затем native faction/base initialization без compatibility adapters проекта
  получает SCRIPT/VM errors и watcher останавливает процессы, exits -1.
  Это **FAIL**, не полноценный успешный матч и не контроль штатного shutdown.
  Для доказательства происхождения загрузочных ошибок достаточно их occurrence
  до native GameStart; этим контролем нельзя доказывать последующую стабильность.

## Зафиксированные внешние темы

| Тема | Конкретный ресурс/сообщение | Статус |
|---|---|---|
| Старые сериализованные поля | `Parent` в BRDM2/BTR70; `SlidingTrackMaterial` в M151A2; `m_fAILimitThreshold` в слоях Conflict при активном RHS | Требуется исправление ресурсов владельцем пакета; dummy fields в scripts не добавлять |
| RHS campaign logic | RHS `logic.layer` ссылается на `CampaignTaskManager.et` и `SCR_BaseTaskManager` | Не создавать пустой класс-заглушку; поведение штатных задач не доказано |
| Игровые ссылки | WCS vest `Vest_PCGen_II_tako3_holster.et` → `Pouch_PCGEN3_takoPouch.et`; материалы GPNVG18, одежды/снаряжения; таблицы локализации RHS/WCS | Нужны корректные GUID/paths в исходных пакетах; `Wrong GUID/name` не всегда означает отсутствие файла, а внешний вид этим прогоном не проверен |
| Editor/preview ссылки | `system/wbdata/Gizmo3D/sphere.xob`, `Circle_ground.et`, `PreviewWindow/TransparentMat.emat` | В части случаев engine явно создаёт default material; чистота ресурсов остаётся FAIL |
| Навигация Эверона | Десять одинаковых групп tile errors в Vanilla/RHS/WCS на старте, каждая вместе с `Failed to load ''` | Повторяется без AICF; не заменять navmesh/геометрию или отключать AI ради удаления сообщения |
| Duplicate Hierarchy | Штатные знаки складов, бутылки и другие entities карт | Повторяется без AICF при создании entities; runtime-удаление компонента после загрузки не исправляет исходный prefab |
| Native font reference | После `Game destroyed.`: `ui/fonts/robotomono_msdf_28.edds 1` | Повторяется без AICF; это одна оставшаяся ссылка на запуск, не доказательство роста памяти во время матча |

Одиночный runtime tile `<0, -1>` исходного EveronRHS сохранён отдельно. Контроль
установил внешнее происхождение десяти стартовых tile errors, но не причину
этого дополнительного запроса. Не приписывать его AICF или native без reproducer.

## Ошибка кода, выделенная из общего списка

180 исходных сообщений `SCR_CharacterDamageManagerComponent` unregistered RPC
нельзя относить к внешним ресурсам. Трассировка привязала их к снятию обуви:
`SCR_FeetLoadoutSlotInfo.OnDetachedEntity` создаёт `SCR_ExposedFeetDamageEffect`
на локальном inventory model либо при teardown уже незарегистрированного бойца.
Исправление и native oracle — [SHUTDOWN_LIFECYCLE.md](SHUTDOWN_LIFECYCLE.md).

Наличие native/resource baseline не отменяет FAIL строгой проверки всех Error.
PR остаётся draft при неустранённых обязательных gates. Фильтрация логов,
понижение severity и выключение игровых подсистем не считаются исправлением.
