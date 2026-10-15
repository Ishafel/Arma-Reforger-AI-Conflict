// Проверка габаритов техники дополняет точный trace пехотного spawn.
// Падающая машина может задеть бойца, хотя trace её hull возвращает clear.
class AICF_InfantryVehicleClearance
{
	protected static const float QUERY_RADIUS_METERS = 16;
	protected static const float VEHICLE_MARGIN_METERS = 2;
	protected vector m_vPosition;
	protected bool m_bBlocked;
	protected ref array<IEntity> m_aSeen = {};

	static bool IsClear(BaseWorld world, vector position)
	{
		if (!world)
			return false;
		AICF_InfantryVehicleClearance query = new AICF_InfantryVehicleClearance();
		query.m_vPosition = position;
		bool completed = world.QueryEntitiesBySphere(position, QUERY_RADIUS_METERS, query.Collect, null, EQueryEntitiesFlags.ALL);
		return completed && !query.m_bBlocked;
	}

	protected bool Collect(IEntity entity)
	{
		if (!entity)
			return true;
		IEntity vehicle = Vehicle.Cast(entity);
		if (!vehicle)
			vehicle = Vehicle.Cast(entity.GetRootParent());
		if (!vehicle || m_aSeen.Find(vehicle) >= 0)
			return true;
		m_aSeen.Insert(vehicle);
		vector mins, maxs;
		vehicle.GetWorldBounds(mins, maxs);
		bool valid = maxs[0] > mins[0] && maxs[2] > mins[2];
		// Высота не исключает опасность: машина ещё может оседать на землю.
		if (!valid || (m_vPosition[0] >= mins[0] - VEHICLE_MARGIN_METERS && m_vPosition[0] <= maxs[0] + VEHICLE_MARGIN_METERS &&
			m_vPosition[2] >= mins[2] - VEHICLE_MARGIN_METERS && m_vPosition[2] <= maxs[2] + VEHICLE_MARGIN_METERS))
		{
			m_bBlocked = true;
			return false;
		}
		return true;
	}
}
