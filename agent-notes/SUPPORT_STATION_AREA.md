# Визуальная зона support station

Штатный `SCR_SupportStationAreaMeshComponent.Init` сравнивает local position
с `SCR_BaseSupportStationComponent.GetOffset()` точным равенством. У реальных
реплицируемых Ural repair (USSR/FIA) и M923 repair серверный child имеет
точное `<0,0,-2.5>`, но клиент получает отклонение до сантиметров ещё до
`GenerateAreaMesh`. Native диагностика сохранялась в runtime от 18.10.2026;
это не compile error и не отказ Recruit. Полный baseline и координаты —
[TESTING.md](TESTING.md). Конкретный транспорт исходного старого run не был
идентифицирован; воспроизведён тот же дефект на всех трёх штатных repair trucks.

`Integration/AICF_SupportStationArea.c` перед native Init и GenerateAreaMesh
восстанавливает только translation визуального child из offset станции.
Применяется лишь при непосредственном parent, `UsesRange()` и ненулевом offset.
Rotation/scale матрицы сохраняются; точное совпадение не вызывает setter.
Сама станция, её offset/range, транспорт, экономика и gameplay ownership
не изменяются. Это локальная visual entity; replicated gameplay fields не
добавляются, BumpMe/RPC/собственных subscriptions или retries нет.

Native `super.Init` сохраняет проверки, mesh generation и enabled subscription;
`super.GenerateAreaMesh` сохраняет материал/геометрию. Иные иерархии и некорректные
ресурсы продолжают проходить native validation. Error не подавляется и не
исключается из log audit. Установленные ресурсы, vanilla world и Script Diff
не изменяются.

`Test-SupportStationAreaStatic.ps1` проверяет границу visual child и native
delegation отрицательными мутациями. Terminal Workbench обязателен.
`AICF_SupportStationAreaProbe.c` проверяет три реальных repair trucks на обеих
ролях; `Test-CoopScenarioLog.ps1 -RequireSupportArea` требует исходное клиентское
расхождение, исправленный центр и полные остановленные логи без SCRIPT(E).
Ручной внешний вид и UI input этим не доказываются.
