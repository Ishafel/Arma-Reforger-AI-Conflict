# Вооружение FIA в RHS и WCS

Issue #52. Core и vanilla integrations сохраняют stock FIA. Общая transaction
находится в RHS `AICF_RHSPMCEquipment`; `AICF_RHSPMCArmament` собирает оружие
в отдельном preview world. RHS/WCS content profiles определяют число вариантов
и overrides. Правило общее для Arland, Everon и Everon North: обычная/AC охрана,
патрули и экипаж/десант гарнизонов проходят character `EOnInit`. Faction identity,
группы, их численность, техника, вторичное оружие и ПТ-средства сохраняются.

## Выбор

Для каждого concrete FIA character prefab profile хранит циклический счётчик
`0..GetPMCVariantCount(source)-1`. Порядок определяется серверным порядком
первой подготовки бойцов. Доли относятся к полному циклу одной роли, а не
обещают точный процент в произвольной небольшой группе.
`AICF_PMCWeaponVariant` записывает результат на character один раз; retry не
потребляет следующий номер. Счётчики принадлежат экземпляру profile матча.
Одежда сохраняет прежний role-based `Variant(source)` и от оружия не зависит.

| Профиль / роль | Варианты в порядке цикла | Доли |
|---|---|---|
| RHS, обычные роли, включая AMG и AC Medic/Grenadier | M4A1 FSP VCOG/RMR SD; Mk18 RMR SD; AR15 UD145 ION1; АК-74М NPZ 1P87 | по 25% |
| RHS, Scout, включая AC Scout | M4A1 ION ACOG/ReapIR SD | 100% |
| RHS, Sharpshooter | SVD 1P21 TGPV | 100% |
| RHS/WCS, MG | ПКМ B51 EOTech | 100% |
| WCS, обычные роли, Scout и AC | M4A1 URGI Suppressed SQUAD; АК-12 EOTECH SQUAD; АК-105 SQUAD | по 1/3 |
| WCS, Sharpshooter | SVD 1P21 TGPV; G28 Suppressed SF SQUAD | по 50% |

Пул MG намеренно один: ПКМ сохраняется, AMG всегда поддерживает именно его.
AMG сохраняет собственную винтовку из обычного пула и получает **3 полные
коробки по 100** `Box_762x54_PK_100rnd_7N13_4Ball_1Tracer.et`
(`0110F6D92703114A`). Прежние UK59 boxes удаляются только из local draft.

## Ресурсы и боезапас

Ресурсные строки принадлежат integration; обязательных RHS/WCS dependencies
в Core нет. У RHS АК canonical spelling — `Rifle_AK74M_npz_rail_1p87.et`:
uppercase NPZ/Rail/1P87 загружается по GUID, но не совпадает с native readback.
WCS переименовывает ПКМ `Eot` в `EOT`; каждый профиль хранит свою строку.

| Оружие | GUID | Профиль |
|---|---|---|
| M4A1 FSP ION VCOGRMR SD | `7EC8EC2BE897F1B0` | RHS |
| Mk18 ION RMR SD | `40DC4A6EE53B969E` | RHS |
| AR15 GA UD145 BLK ION1 | `3E54573B11FB0578` | RHS |
| АК-74М NPZ Rail 1P87 | `805927DD98352C9B` | RHS |
| M4A1 ION ACOG ReapIR SD | `951E3D5E86826A47` | RHS Scout |
| ПКМ B51 Eot/EOT | `4BE4931DF7B1ABCD` | RHS/WCS |
| SVD 1P21 TGPV | `896DDA7B352187EF` | RHS/WCS |
| M4A1 URGI 145 Suppressed SQUAD | `4E0FB248B9FB1F5B` | WCS |
| АК-12 EOTECH SQUAD | `397B6CBE56D21341` | WCS |
| АК-105 SQUAD | `0A83316977E4E4B7` | WCS |
| G28 Suppressed SF SQUAD | `5C0C95DCC4471208` | WCS |

Каждый комплект использует loaded magazine из собственного weapon prefab;
по его canonical prefab создаются spares. `BaseMagazineWell.Type()` сверяется
при readback. Минимум положительных запасных магазинов: MG 3, Sharpshooter 5,
Ammo 26, остальные 6. Loaded magazine в минимум не входит. ПКМ/АК допускают
штатный обвес без глушителя; сохранённые M-серия, SVD и G28 требуют
`IsMuzzleSuppressed`. Реальные magazine prefab фиксирует
`[FIA_ARMAMENT_READBACK]` в полном server log.

| Профиль / оружие | Loaded и spare magazine prefab | GUID |
|---|---|---|
| RHS, четыре M-варианта | `Pmag/Magazine_556x45_PmagWindowed_30rnd_M855A1.et` | `C632D04DD25D3ABF` |
| RHS, АК-74М | `6l23_plastic/Magazine_545x39_plastic_AK_30rnd_Last_5Tracer.et` | `FD34D121FA55CB10` |
| RHS, SVD | `Magazine_762x54_SVD_10rnd_Sniper.et` | `9CCB46C6EE632C1A` |
| RHS/WCS, ПКМ и поддержка AMG | `Box_762x54_PK_100rnd_7N13_4Ball_1Tracer.et` | `0110F6D92703114A` |
| WCS, M4 URGI | `556x45_PMAG_Window_30rnd/Magazine_556x45_PMAG_Window_30rnd_4AP_1Tracer_M855A1_M856.et` | `FF8088BE4426164C` |
| WCS, АК-12 | `545x39_AK12_30rnd/Magazine_545x39_AK12_30rnd_4AP_1Tracer_7N10_7T3.et` | `62DEC638B985B3FE` |
| WCS, АК-105 | `545x39_RHS_AK74_6L23_Plastic_30rnd/Magazine_545x39_AK74_6L23_Plastic_30rnd_4AP_1Tracer_7N10_7T3.et` | `16EAEE16DD1B97F7` |
| WCS, SVD | `762x54_SVD_10rnd/Magazine_762x54_SVD_10rnd_4Ball_1Tracer_57N323S_7T2.et` | `50A6766E6DBDC61C` |
| WCS, G28 | `762x51_G28_20rnd/Magazine_762x51_G28_20rnd_4AP_1Tracer_M61_M62.et` | `3DC06D56E97E2C15` |

Пути в таблице относительно `Prefabs/Weapons/Magazines/`. Это readback
проверенных graph, не утверждение о будущих версиях dependencies.

## Transaction и readiness

Server/master, non-player, live FIA, immutable entity ID и concrete source
проверяются перед подготовкой и перед commit. Текущий inventory повторно
сравнивается с исходным непосредственно перед native restore. Local draft
проходит реальную вместимость storage; live commit проверяет serializer
signature, exact weapon prefab, loaded ammo, spares и AMG boxes. При отказе
восстанавливается исходный snapshot, выбор остаётся прежним.

После успешного commit character хранит флаг завершения. Повторный `Apply`
сразу возвращает успех, сохраняя изменения оружия/патронов/одежды после
initialization. Неудачная подготовка оставляет pending и закрывает оба boarding
gate: `AICF_FIAPatrolCrew.Board` и `AICF_FIAGarrisonCrew.BoardGarrison`.
Автоматического бесконечного retry нет; повторный вызов использует тот же выбор.
Одноразовый `CallLater` удаляется при уничтожении character.

## Проверки

`Test-FIAArmamentContracts.ps1`: authority, стабильный выбор, идемпотентность,
identity/snapshot fences, rollback, boarding, pools и native magazine wells;
положительный source и отрицательные мутации. RHS pending audit обновлён по
новому fail-closed контракту: failed preparation больше не открывает boarding.

В stage Core помещается `AICF_FIAArmamentProbe.c`, в RHS Content **после**
`AICF_RHSPMCEquipment.c` — `ZZ_AICF_RHSFIAArmamentProbe.c`; WCS дополнительно
получает `ZZ_AICF_WCSFIAArmamentProbe.c` после content profile. Production
каталоги probes не содержат. Запуск — штатный launcher с
`-aicfFIAArmamentProbe 1`, Medium, свежим profile. Compile/runtime команды
и manifests хранятся в `.codex-runtime/issue52/`.

Матрица: 13 stock FIA roles и 3 AC prefabs, все варианты каждой роли, восемь
обычных riflemen без принудительного выбора, отдельные unknown-selection,
unavailable-resource, native insertion refusal, rollback и retry. Проверяются
live weapon/ammo inventory, сохранение вторичного оружия, повторный `Apply`
после расхода патрона, реальные patrol/garrison hooks и pending gate.
`Test-FIAArmamentLog.ps1` требует точный размер матрицы и каждый обязательный
case; читает полный остановленный log вместе с native exit и launcher manifest.
Stock probe сравнивает weapon/ammo сразу после spawn и перед завершением;
два запуска исходного/нового source сравниваются также между собой.

Внешний вид, субъективный боевой баланс, client/JIP и soak не проверяются этим
probe. Script compile/VM errors и gameplay failures не списываются на внешние
resource/shutdown diagnostics. Фактические verdict и матрица карт — в
[TESTING.md](TESTING.md).
