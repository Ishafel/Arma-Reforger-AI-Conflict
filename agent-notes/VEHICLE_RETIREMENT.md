# Завершение trip после исчезновения исходной группы

`SCR_AIGroup` может штатно исчезнуть после гибели последнего бойца. Live
`assignment.IsValid()`/`trip.IsValid()` при этом остаются false. Это не должно
оставлять authoritative lease/cap/owned waypoint навсегда в TRANSIT.

`AICF_TripRetirementIdentity` при admission сохраняет faction/slot, group EntityID
и component RplId, group/trip generation, operation и исходный roster. Отдельный
retirement guard допускает только FAILED_CLOSED для именно этой отсутствующей
группы: и native World lookup, и Replication lookup должны подтвердить отсутствие.
Живая replacement group не получает старую identity. Controller повторяет
актуальные fleet/lease/vehicle fences вокруг Exit effects; flow не вызывает cleanup.

Cleanup по-прежнему принадлежит `AICF_VehicleCleanupManager`. Общие human/foreign/
unknown occupancy и physical clearance scans не ослаблены. Узкий orphan helper
может вызвать native `KickFromVehicle` для admission actor с теми же EntityID,
component RplId и фракцией, actual DEAD, отсутствующей исходной группой и без
нового foreign AIGroup owner. PlayerManager controlled/main ownership, native seat,
car, authoritative realm, lease generations, fence и privacy policy проверяются
повторно непосредственно перед вызовом. Максимум три существующие попытки;
лечения, воскрешения, SetOrigin и удаления corpse нет.

Полная естественная гибель отделена от технической ошибки типизированным
`AICF_TripRetirementObservation`. Положительная причина разрешается только когда
все admission actors уже native DEAD до собственного stale-terminal перехода.
Ранее завершённый trip, иной terminal cause, новая lease/epoch, неизвестное тело,
исчезновение actor без предварительного native DEAD дают NONE/REJECTED. Если
раннее тело исчезло до смерти последнего бойца, текущая cohort-проверка не умеет
доказать всю историю: это остаётся UNKNOWN, а не предположение о гибели.

PENDING не является PASS. CONFIRMED требует собственного точного cleanup context,
реального physical release и acknowledgement в старый cleanup deadline. Здоровая
машина может штатно перейти в WorldPool; разрушенная должна пройти quarantine и
native absence. После `Fleet.ReleaseLeaseAt` lease уже передаёт reference новому
asset: physical receipt читает машину из job WorldPoolAsset/RetirementAsset.
Историческая квитанция release сохраняется при следующем захвате машины; cap0
проверяется для старой lease, наличие других leases фракции допустимо.

AcceptanceMonitor принимает immutable projection и не сбрасывает предыдущие
failure latches/count/history. Legit retirement не подтверждает original full
route. Coordinator удерживает только диагностическую ссылку на pending old trip
после удаления из live registry, пока cleanup не вернёт окончательную квитанцию.
Stop ограничивает ожидание и снимает список; pending на остановке не считается
завершённым восстановлением.

Проверки: `Test-OrphanTripRetirementContracts.ps1` (identity/DEAD/player/foreign),
`Test-CapturedDeathRetirementProjectionContracts.ps1` (extracted classifier,
monitor, cause/epoch и prior errors), `Test-OrphanTripRetirementLogContracts.ps1`
(негативы полного controlled ledger). Runtime fixture
`AICF_VehicleSoakRetirementProbe.c` использует настоящий native Kill только в
изолированном controlled case. After-stage дополнительно загружает
`AICF_OrphanTripConfirmationProbe.c` и завершает case лишь после фактического
typed CONFIRMED callback и повторных native postconditions. Before-stage старого
production этот дополнительный observer не загружает.

`Test-OrphanTripRetirementLog.ps1 -EvidenceRoot <new-run> [-Before]
[-DestroyVehicle]` отдельно сохраняет функциональный outcome, source freeze,
остановку, все sibling fatal/resource ошибки и raw shutdown warnings. Только
точный committed COORDINATOR_STOP новой generation после доказанного DONE имеет
отдельную shutdown-классификацию; original-operation/early/uncommitted Stop
остаётся FAIL. Полные native/resource ошибки не исключаются.
