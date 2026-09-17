// Только визуальная копия черновика. Камера и свет принадлежат этой форме,
// а не ItemPreviewManager кампании или его общему cache.
class AICF_LoadoutPreview
{
	protected static int s_iSequence;
	protected ref SharedItemRef m_WorldRef;
	protected BaseWorld m_World;
	protected IEntity m_Model;
	protected IEntity m_Lighting;
	protected LightEntity m_FillLight;
	protected vector m_vTarget;
	protected float m_fYaw = 180;
	protected float m_fDistance = 3;
	protected float m_fFOV = 45;
	protected float m_fPanY;
	protected float m_fPanLimit = 1;

	BaseWorld GetWorld() { return m_World; }
	bool HasLighting() { return m_Lighting && m_Lighting.GetWorld() == m_World; }
	bool HasFillLight() { return m_FillLight && m_FillLight.GetWorld() == m_World && m_FillLight.IsEnabled(); }
	bool IsReady() { return m_World && m_Model && m_Model.GetWorld() == m_World && HasLighting(); }

	bool Build(IEntity source)
	{
		if (!source || source.GetWorld() == GetGame().GetWorld())
			return false;
		InventoryItemComponent item = InventoryItemComponent.Cast(source.FindComponent(InventoryItemComponent));
		if (!item)
			return false;
		if (!m_WorldRef)
		{
			m_WorldRef = BaseWorld.CreateWorld("InspectionPreview", "AICF_LoadoutView_" + (++s_iSequence).ToString());
			if (!m_WorldRef || !m_WorldRef.IsValid())
				return false;
			m_World = m_WorldRef.GetRef();
			// Используем штатную сцену inventory inspection. LightHandle —
			// native pointer только для локальных переменных, хранить его нельзя.
			Resource lighting = Resource.Load("{4391FE7994EE6FE2}Prefabs/World/Game/InventoryPreviewWorld.et");
			if (lighting && lighting.IsValid())
				m_Lighting = GetGame().SpawnEntityPrefabLocal(lighting, m_World);
			if (!HasLighting())
			{
				Clear();
				return false;
			}
			m_World.SetCameraType(0, CameraType.PERSPECTIVE);
			m_World.SetCameraNearPlane(0, 0.05);
			m_World.SetCameraFarPlane(0, 30);
			CreateFillLight();
		}
		if (m_Model)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Model);
		m_Model = item.CreatePreviewEntity(m_World, 0);
		if (!m_Model || m_Model.GetWorld() != m_World)
			return false;
		vector mins, maxs;
		m_Model.GetBounds(mins, maxs);
		m_Model.SetOrigin(-(mins + maxs) * 0.5);
		m_Model.Update();
		m_vTarget = vector.Zero;
		m_fPanLimit = Math.Max(0.25, (maxs[1] - mins[1]) * 0.5);
		m_fPanY = Math.Clamp(m_fPanY, -m_fPanLimit, m_fPanLimit);
		m_fDistance = Math.Max(2.5, vector.Distance(mins, maxs) * 1.4);
		UpdateCamera();
		return IsReady();
	}

	void Rotate(float delta)
	{
		m_fYaw += delta;
		UpdateCamera();
	}

	void Zoom(float delta)
	{
		m_fFOV = Math.Clamp(m_fFOV + delta, 25, 70);
		UpdateCamera();
	}

	// Доля высоты viewport: одинаковое движение мыши при любом разрешении.
	// При приближении шаг уменьшается вместе с видимым полем камеры.
	void Pan(float delta)
	{
		float height = 2 * m_fDistance * Math.Tan(m_fFOV * Math.DEG2RAD * 0.5);
		m_fPanY = Math.Clamp(m_fPanY + delta * height, -m_fPanLimit, m_fPanLimit);
		UpdateCamera();
	}

	protected void UpdateCamera()
	{
		if (!m_World)
			return;
		vector transform[4];
		Math3D.AnglesToMatrix(Vector(m_fYaw, 0, 0), transform);
		transform[3] = m_vTarget + Vector(0, m_fPanY, 0) - transform[2] * m_fDistance;
		m_World.SetCameraEx(0, transform);
		m_World.SetCameraVerticalFOV(0, m_fFOV);
		UpdateFillLight(transform);
	}

	protected void CreateFillLight()
	{
		// У CreateLight нет параметра world. Штатный dynamic prefab можно
		// создать явно в мире формы и безопасно удерживать как LightEntity.
		Resource resource = Resource.Load("{106CAFF367CA4BCC}Prefabs/Editor/Common/CameraLight.et");
		if (resource && resource.IsValid())
		{
			IEntity entity = GetGame().SpawnEntityPrefabLocal(resource, m_World);
			m_FillLight = LightEntity.Cast(entity);
			if (entity && (!m_FillLight || entity.GetWorld() != m_World))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
				m_FillLight = null;
			}
		}
		if (!m_FillLight)
		{
			Print("[AICF][LOADOUT_PREVIEW] fill_light=UNAVAILABLE fallback=STOCK_LIGHTING", LogLevel.WARNING);
			return;
		}
		// Это заполняющий свет: сохраняем HDR/ambient штатной сцены,
		// не добавляем shadow pass и specular-засветку на одежде/лице.
		// Spot на расстоянии 3 м требует большей мощности, чем distant
		// source штатной сцены: прежние LV 3.5 оставляли контровой свет главным.
		m_FillLight.SetColor(Color.FromInt(0xFFFFFFFF), 6.5);
		m_FillLight.SetRadius(8);
		m_FillLight.SetConeAngle(140);
		m_FillLight.SetCastShadow(false);
		m_FillLight.SetLightFlags(LightFlags.DIFFUSE_ONLY);
		m_FillLight.SetLensFlareType(LightLensFlareType.Disabled);
		m_FillLight.SetLightCameraMask(1);
		m_FillLight.SetEnabled(true);
	}

	protected void UpdateFillLight(vector camera[4])
	{
		if (!HasFillLight())
			return;
		// Постоянная дистанция до куклы: масштаб/FOV и размер комплекта
		// не меняют яркость. Свет следует за ракурсом только при input.
		vector position = m_vTarget - camera[2] * 3 + camera[1] * 0.8;
		vector direction = m_vTarget - position;
		direction.Normalize();
		// Dynamic LightEntity берёт orientation из transform владельца.
		// Отдельный SetLightDirection не заменяет поворот самой сущности.
		vector lightTransform[4];
		Math3D.DirectionAndUpMatrix(direction, camera[1], lightTransform);
		lightTransform[3] = position;
		m_FillLight.SetWorldTransform(lightTransform);
		m_FillLight.Update();
	}

	void Clear()
	{
		if (m_Model)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Model);
		m_Model = null;
		if (m_FillLight)
			SCR_EntityHelper.DeleteEntityAndChildren(m_FillLight);
		m_FillLight = null;
		if (m_Lighting)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Lighting);
		m_Lighting = null;
		m_World = null;
		m_WorldRef = null;
		m_fPanY = 0;
	}

	void ~AICF_LoadoutPreview()
	{
		Clear();
	}
}
