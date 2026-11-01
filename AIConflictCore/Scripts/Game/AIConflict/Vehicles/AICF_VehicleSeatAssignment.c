// Меняет только черновик. Binding, roster и физическая посадка принадлежат server lifecycle.
class AICF_VehicleSeatAssignment
{
	static bool AutoAssign(AICF_VehiclePreset preset, array<int> activeMembers, array<int> mandatoryIndices)
	{
		if (!preset || !activeMembers || !mandatoryIndices) return false;
		if (!preset.m_aSeats || !preset.m_aMembers) return false;
		string reason;
		if (!preset.ValidateShape(reason)) return false;
		if (activeMembers.IsEmpty() || activeMembers.Count() > AICF_Stage1Config.MAX_GROUP_SIZE) return false;
		if (mandatoryIndices.IsEmpty() || mandatoryIndices.Count() > preset.m_aSeats.Count()) return false;
		array<int> members = {};
		foreach (int member : activeMembers)
		{
			if (member < 0 || member >= AICF_Stage1Config.MAX_GROUP_SIZE || members.Contains(member)) return false;
			members.Insert(member);
		}
		array<int> required = {};
		foreach (int index : mandatoryIndices)
		{
			if (index < 0 || index >= preset.m_aSeats.Count() || required.Contains(index)) return false;
			required.Insert(index);
		}
		if (members.Count() < required.Count()) return false;
		array<int> ordered = {};
		for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)
			if (members.Contains(position)) ordered.Insert(position);
		array<int> assignments = {};
		for (int seat; seat < preset.m_aSeats.Count(); seat++) assignments.Insert(-1);
		int next;
		for (int mandatory; mandatory < assignments.Count(); mandatory++)
			if (required.Contains(mandatory)) assignments[mandatory] = ordered[next++];
		for (int cargo; cargo < assignments.Count() && next < ordered.Count(); cargo++)
			if (!required.Contains(cargo)) assignments[cargo] = ordered[next++];
		// Ни одна запись draft не меняется до завершения всех проверок.
		for (int assigned; assigned < assignments.Count(); assigned++) preset.m_aMembers[assigned] = assignments[assigned];
		return true;
	}
}
