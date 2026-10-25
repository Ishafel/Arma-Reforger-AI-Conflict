// Только стабильные адреса compartment и numeric position. Entity identity
// принадлежит текущему trip и никогда не попадает в сохранённый рецепт.
class AICF_VehiclePreset
{
	static const int MAX_BYTES = 16384;
	static const int MAX_SEATS = 64;
	string m_sProfile;
	string m_sFaction;
	string m_sName;
	ResourceName m_sPrefab;
	ref array<string> m_aSeats = {};
	ref array<int> m_aMembers = {};

	string Encode()
	{
		JsonSaveContext json = new JsonSaveContext();
		json.WriteValue("schemaVersion", 1);
		json.WriteValue("profile", m_sProfile);
		json.WriteValue("faction", m_sFaction);
		json.WriteValue("name", m_sName);
		json.WriteValue("prefab", m_sPrefab);
		json.WriteValue("seats", m_aSeats);
		json.WriteValue("members", m_aMembers);
		return json.SaveToString();
	}

	static AICF_VehiclePreset Decode(string data)
	{
		if (data.IsEmpty() || data.Length() > MAX_BYTES) return null;
		JsonLoadContext json = new JsonLoadContext();
		AICF_VehiclePreset preset = new AICF_VehiclePreset();
		int schema;
		if (!json.LoadFromString(data) || !json.ReadValue("schemaVersion", schema) || schema != 1 ||
			!json.ReadValue("profile", preset.m_sProfile) || !json.ReadValue("faction", preset.m_sFaction) ||
			!json.ReadValue("name", preset.m_sName) || !json.ReadValue("prefab", preset.m_sPrefab) ||
			!json.ReadValue("seats", preset.m_aSeats) || !json.ReadValue("members", preset.m_aMembers)) return null;
		string reason;
		if (!preset.ValidateShape(reason)) return null;
		return preset;
	}

	bool ValidateShape(out string reason)
	{
		reason = "INVALID_PRESET";
		if (m_sProfile.IsEmpty() || m_sProfile.Length() > 80 || (m_sFaction != "US" && m_sFaction != "USSR") ||
			!AICF_LoadoutPreset.ValidName(m_sName) || m_sPrefab.IsEmpty() || m_sPrefab.Length() > 256 ||
			m_aSeats.IsEmpty() || m_aSeats.Count() > MAX_SEATS || m_aMembers.Count() != m_aSeats.Count()) return false;
		array<string> seats = {};
		array<int> members = {};
		for (int i; i < m_aSeats.Count(); i++)
		{
			string seat = m_aSeats[i];
			array<string> parts = {};
			seat.Split(":", parts, false);
			if (parts.Count() != 3) { reason = "INVALID_SEAT"; return false; }
			foreach (string part : parts)
			{
				int value = part.ToInt(-1);
				if (value < 0 || value.ToString() != part) { reason = "INVALID_SEAT"; return false; }
			}
			if (seats.Contains(seat)) { reason = "DUPLICATE_SEAT"; return false; }
			seats.Insert(seat);
			int member = m_aMembers[i];
			if (member < -1 || member >= AICF_Stage1Config.MAX_GROUP_SIZE) { reason = "INVALID_MEMBER"; return false; }
			if (member < 0) continue;
			if (members.Contains(member)) { reason = "DUPLICATE_MEMBER"; return false; }
			members.Insert(member);
		}
		reason = "READY";
		return true;
	}

	bool Assign(int seatIndex, int member)
	{
		if (seatIndex < 0 || seatIndex >= m_aMembers.Count() || member < -1 || member >= AICF_Stage1Config.MAX_GROUP_SIZE) return false;
		// Переназначение освобождает прежнее место этого бойца.
		if (member >= 0)
			for (int i; i < m_aMembers.Count(); i++)
				if (m_aMembers[i] == member) m_aMembers[i] = -1;
		m_aMembers[seatIndex] = member;
		return true;
	}

	bool SameAssignment(AICF_VehiclePreset other)
	{
		if (!other || m_sProfile != other.m_sProfile || m_sFaction != other.m_sFaction || m_sPrefab != other.m_sPrefab ||
			m_aSeats.Count() != other.m_aSeats.Count()) return false;
		for (int i; i < m_aSeats.Count(); i++)
			if (m_aSeats[i] != other.m_aSeats[i] || m_aMembers[i] != other.m_aMembers[i]) return false;
		return true;
	}

	static string SeatKey(BaseCompartmentSlot seat)
	{
		return string.Format("%1:%2:%3", RelativeOwnerIndex(seat), seat.GetCompartmentSlotID(), seat.GetType());
	}

	// Native manager IDs растут между инстансами и мирами. В сохранённом
	// адресе используем ordinal владельца в иерархии конкретного prefab.
	protected static void CollectHierarchy(IEntity entity, array<IEntity> owners)
	{
		if (!entity) return;
		if (entity.FindComponent(BaseCompartmentManagerComponent)) owners.Insert(entity);
		IEntity child = entity.GetChildren();
		while (child)
		{
			CollectHierarchy(child, owners);
			child = child.GetSibling();
		}
	}

	static int RelativeOwnerIndex(BaseCompartmentSlot seat)
	{
		array<IEntity> owners = {};
		CollectHierarchy(seat.GetVehicle(), owners);
		return owners.Find(seat.GetOwner());
	}

	static void Collect(IEntity vehicle, array<BaseCompartmentSlot> seats)
	{
		seats.Clear();
		if (!vehicle) return;
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager) return;
		array<BaseCompartmentSlot> found = {};
		manager.GetCompartments(found);
		foreach (BaseCompartmentSlot seat : found)
		{
			if (!seat || !seat.IsCompartmentAccessible() || seat.GetVehicle() != vehicle) continue;
			int index = seats.Count();
			for (int i; i < seats.Count(); i++)
				if (RelativeOwnerIndex(seat) < RelativeOwnerIndex(seats[i]) ||
					(RelativeOwnerIndex(seat) == RelativeOwnerIndex(seats[i]) && seat.GetCompartmentSlotID() < seats[i].GetCompartmentSlotID()))
				{ index = i; break; }
			seats.InsertAt(seat, index);
		}
	}

	bool MatchesSeats(array<BaseCompartmentSlot> seats)
	{
		if (seats.Count() != m_aSeats.Count()) return false;
		for (int i; i < seats.Count(); i++)
			if (SeatKey(seats[i]) != m_aSeats[i]) return false;
		return true;
	}

	bool ValidateRoster(AICF_GroupSlot slot, array<BaseCompartmentSlot> seats, out string reason)
	{
		if (!ValidateShape(reason)) return false;
		reason = "SEAT_TOPOLOGY_CHANGED";
		if (!MatchesSeats(seats)) return false;
		reason = "ROSTER_NOT_READY";
		if (!slot || !slot.IsCombatReady() || !slot.GetGroup()) return false;
		reason = "INSUFFICIENT_SEATS";
		if (slot.GetDesiredSize() > seats.Count()) return false;
		int pilots;
		for (int seatIndex; seatIndex < seats.Count(); seatIndex++)
		{
			BaseCompartmentSlot requiredSeat = seats[seatIndex];
			if (requiredSeat.GetType() == ECompartmentType.PILOT) pilots++;
			if ((requiredSeat.GetType() == ECompartmentType.PILOT || requiredSeat.GetType() == ECompartmentType.TURRET || requiredSeat.GetAttachedTurret()) && m_aMembers[seatIndex] < 0)
				{ reason = "MANDATORY_CREW_MISSING"; return false; }
		}
		reason = "DRIVER_SEAT_MISSING";
		if (pilots != 1) return false;
		foreach (int assignedMember : m_aMembers)
			if (assignedMember >= 0 && (assignedMember >= slot.GetDesiredSize() || !AICF_GroupRuntime.IsAliveCharacter(slot.GetRosterMember(assignedMember))))
				{ reason = "MEMBER_INACTIVE"; return false; }
		int alive;
		for (int member; member < AICF_Stage1Config.MAX_GROUP_SIZE; member++)
		{
			IEntity entity = slot.GetRosterMember(member);
			bool active = member < slot.GetDesiredSize() && AICF_GroupRuntime.IsAliveCharacter(entity);
			int assigned = m_aMembers.Find(member);
			if (!active && assigned >= 0) { reason = "MEMBER_INACTIVE"; return false; }
			if (!active) continue;
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(entity)) { reason = "MEMBER_PLAYER_CONTROLLED"; return false; }
			alive++;
			if (assigned < 0) { reason = "MEMBER_WITHOUT_SEAT"; return false; }
			ChimeraCharacter character = ChimeraCharacter.Cast(entity);
			if (!character || !character.GetAIControlComponent() || !character.GetAIControlComponent().GetControlAIAgent() ||
				character.GetAIControlComponent().GetControlAIAgent().GetParentGroup() != slot.GetGroup())
				{ reason = "ROSTER_IDENTITY_CHANGED"; return false; }
		}
		reason = "INSUFFICIENT_SEATS";
		if (alive <= 0 || alive > seats.Count() || alive != AICF_GroupRuntime.CountAliveAgents(slot.GetGroup())) return false;
		reason = "READY";
		return true;
	}
}
