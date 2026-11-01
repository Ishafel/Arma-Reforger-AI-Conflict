// Test-only read-only present-world geometry at captured historical poses.
// Does not reconstruct historical dynamic occupants or claim original-run PASS.
modded class AICF_MatchController
{
	protected int m_iMSGStarted;
	protected int m_iMSGNext;
	protected int m_iMSGSamples;
	protected bool m_bMSGDone;
	override protected void Update()
	{
		super.Update();
		string flag;
		if (m_bMSGDone || !Replication.IsServer() || !m_bRosterReady || !System.GetCLIParam("aicfManualStagingGeometryProbe", flag) || flag != "1") return;
		int now = System.GetTickCount();
		if (!m_iMSGStarted) m_iMSGStarted = now;
		if (now < m_iMSGNext) return;
		m_iMSGNext = now + 3000;
		SCR_AIGroup group;
		if (m_USSRState && m_USSRState.GetSlot(0)) group = m_USSRState.GetSlot(0).GetGroup();
		if (!group) return;
		m_iMSGSamples++;
		bool emit = now - m_iMSGStarted >= 15000;
		AICF_MSGSample(group, "MULTI_V9_USSR2", "1203.26 1.205 3266.16", "1180.06 1.125 3293.75", "1157.67 1.10898 3330.46", emit);
		AICF_MSGSample(group, "AFTER_V3_SETUP", "1152.42 1.19235 3290.87", "1187.73 1.19235 3297.89", "1157.67 1.066 3266.46", emit);
		AICF_MSGSample(group, "AFTER_V4_SETUP", "1623.41 4.01873 894.6", "1648.87 5.29919 920.055", "1621.77 4.23637 886.667", emit);
		if (!emit) return;
		m_bMSGDone = true;
		Print(string.Format("[AICF][MANUAL_STAGING_GEOMETRY_FINISHED] samples=%1 historical_dynamic_occupancy_reconstructed=0 present_world_only=1 production_mutations=0", m_iMSGSamples));
		GetGame().RequestClose();
	}
	protected void AICF_MSGSample(SCR_AIGroup group, string label, vector origin, vector staging, vector padPosition, bool emit)
	{
		array<float> rings = {3, 5, 7, 2, 4, 6, 7.9};
		array<float> angles = {0, 45, -45, 11.25, -11.25, 22.5, -22.5, 33.75, -33.75, 56.25, -56.25, 67.5, -67.5};
		vector forward = staging - origin; forward[1] = 0; forward.Normalize();
		vector right = Vector(-forward[2], 0, forward[0]);
		int oldSafe;
		int addedSafe;
		int sampled;
		for (int phase; phase < 2; phase++)
			for (int ring; ring < rings.Count(); ring++)
				for (int direction; direction < angles.Count(); direction++)
				{
					bool old = ring < 3 && direction < 3;
					if ((phase == 0 && !old) || (phase == 1 && old)) continue;
					float angle = angles[direction] * Math.DEG2RAD;
					vector query = origin + (forward * Math.Cos(angle) + right * Math.Sin(angle)) * rings[ring];
					vector destination;
					bool usable = AICF_InfantrySpawnPlacement.IsUsable(group, query, destination);
					float shift = vector.Distance(origin, destination);
					float vertical = Math.AbsFloat(origin[1] - destination[1]);
					float projection = vector.DistanceSqXZ(query, destination);
					float pad = vector.DistanceXZ(destination, padPosition);
					float progress = vector.DistanceXZ(origin, staging) - vector.DistanceXZ(destination, staging);
					bool safe = usable && shift >= 2 && shift <= 8 && vertical <= 1 && projection <= 0.25 && pad >= 10 && pad <= 90 && progress > 1;
					if (safe && old) oldSafe++;
					if (safe && !old) addedSafe++;
					sampled++;
					if (!emit) continue;
					string details = string.Format("label=%1 candidate=%2 old=%3 native_usable=%4 safe=%5 query=%6 destination=%7 shift_3d_m=%8 vertical_m=%9", label, sampled, old, usable, safe, query, destination, shift, vertical);
					details += string.Format(" projection_sq=%1 pad_m=%2 progress_m=%3 no_actor_or_vehicle_mutation=1", projection, pad, progress);
					Print("[AICF][MANUAL_STAGING_GEOMETRY_CANDIDATE] " + details);
				}
		if (emit) Print(string.Format("[AICF][MANUAL_STAGING_GEOMETRY_SUMMARY] label=%1 candidates=%2 old_safe=%3 added_safe=%4 source_origin=%5 source_staging=%6 source_pad=%7 present_world_only=1", label, sampled, oldSafe, addedSafe, origin, staging, padPosition));
	}
}
