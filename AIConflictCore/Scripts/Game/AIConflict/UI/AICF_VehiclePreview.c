// Собственная local модель без crew, physics кампании и общего preview cache.
class AICF_VehiclePreview : AICF_LoadoutPreview
{
	protected ResourceName m_sPrefab;

	bool BuildVehicle(ResourceName prefab)
	{
		if (prefab == m_sPrefab && IsReady()) return true;
		Clear();
		m_sPrefab = ResourceName.Empty;
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid()) return false;
		m_WorldRef = BaseWorld.CreateWorld("ChimeraWorld", "AICF_VehicleView_" + (++s_iSequence).ToString());
		if (!m_WorldRef || !m_WorldRef.IsValid()) return false;
		ChimeraWorld world = m_WorldRef.GetRef();
		if (!world || world == GetGame().GetWorld()) { Clear(); return false; }
		m_World = world;
		world.LoadSystems("{3A1DEDB2A5818B53}Configs/Systems/MapPreviewSystemsConfig.conf");
		m_Model = GetGame().SpawnEntityPrefabLocal(resource, world);
		if (!Vehicle.Cast(m_Model) || m_Model.GetWorld() != world) { Clear(); return false; }
		m_Lighting = GetGame().SpawnEntityPrefabLocal(Resource.Load("{4391FE7994EE6FE2}Prefabs/World/Game/InventoryPreviewWorld.et"), world);
		if (!HasLighting()) { Clear(); return false; }
		CreateFillLight();
		m_World.SetCameraType(0, CameraType.PERSPECTIVE);
		m_World.SetCameraNearPlane(0, 0.1);
		m_World.SetCameraFarPlane(0, 500);
		vector mins, maxs;
		m_Model.GetBounds(mins, maxs);
		m_Model.SetOrigin(-(mins + maxs) * 0.5);
		m_Model.Update();
		float radius = (maxs - mins).Length() * 0.5;
		m_vTarget = vector.Zero;
		m_fYaw = 225;
		m_fFOV = 45;
		m_fDistance = Math.Max(3, radius / Math.Sin(m_fFOV * Math.DEG2RAD * 0.5) * 1.15);
		m_fPanY = 0;
		m_fPanLimit = Math.Max(1, radius);
		UpdateCamera();
		m_sPrefab = prefab;
		return IsReady();
	}
}
