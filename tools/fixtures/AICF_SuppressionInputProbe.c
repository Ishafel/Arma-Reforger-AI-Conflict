// Только isolated stage, -aicfSuppressionInputProbe 1. Позиции живых бойцов
// не меняются. Опасные входы проверяются до native API, допустимые проходят
// настоящий GetRandomPosition, включая ось X и все четыре квадранта.
// Режим 2 намеренно обходит guard и воспроизводит stock VM exception;
// его лог является reproduction evidence, а не успешным runtime gate.
class AICF_SuppressionNativeReproBox : SCR_AISuppressionVolumeBaseTargetBox
{
	vector AICF_ReproduceZeroSlope()
	{
		return GetOutsideEdgePos("0 0 1");
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFSuppressionProbeStarted;
	protected int m_iAICFSuppressionChecks;
	protected int m_iAICFSuppressionFailures;

	protected void AICF_SuppressionCheck(string name, bool passed)
	{
		m_iAICFSuppressionChecks++;
		if (!passed)
			m_iAICFSuppressionFailures++;
		Print(string.Format("[AICF][SUPPRESSION_INPUT_CHECK] name=%1 passed=%2", name, passed));
	}

	protected void AICF_SuppressionCase(IEntity shooter, string name, vector offset, bool expected)
	{
		vector center = shooter.GetOrigin() + offset;
		SCR_AISuppressionVolumeBaseTargetBox box = new SCR_AISuppressionVolumeBaseTargetBox(center - "3 2 3", center + "3 2 3");
		bool passed = true;
		for (int i; i < 64; i++)
		{
			bool allowed = AICF_SuppressionInputGuard.CanGenerateLine(box, shooter.GetOrigin());
			passed = passed && allowed == expected;
			if (!allowed || !expected)
				continue;
			vector start;
			vector end = box.GetRandomPosition(shooter, start);
			vector next = box.GetRandomPosition(shooter, end, vector.Direction(start, end).Normalized());
			for (int axis; axis < 3; axis++)
				passed = passed && Math.AbsFloat(start[axis]) < float.MAX && Math.AbsFloat(end[axis]) < float.MAX && Math.AbsFloat(next[axis]) < float.MAX;
		}
		AICF_SuppressionCheck(name, passed);
	}

	protected void AICF_TestSuppressionGeometry(AICF_GroupSlot slot, string side)
	{
		IEntity shooter = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		AICF_SuppressionCheck(side + "_SHOOTER", shooter != null);
		if (!shooter)
			return;
		AICF_SuppressionCase(shooter, side + "_SAME_Z_POSITIVE_X", "10 0 0", false);
		AICF_SuppressionCase(shooter, side + "_SAME_Z_NEGATIVE_X", "-10 0 0", false);
		AICF_SuppressionCase(shooter, side + "_SAME_XZ", vector.Zero, false);
		AICF_SuppressionCase(shooter, side + "_VERTICAL_ONLY", "0 10 0", false);
		AICF_SuppressionCase(shooter, side + "_SAME_X_POSITIVE_Z", "0 0 10", true);
		AICF_SuppressionCase(shooter, side + "_SAME_X_NEGATIVE_Z", "0 0 -10", true);
		AICF_SuppressionCase(shooter, side + "_QUADRANT_PP", "10 0 10", true);
		AICF_SuppressionCase(shooter, side + "_QUADRANT_PN", "10 0 -10", true);
		AICF_SuppressionCase(shooter, side + "_QUADRANT_NP", "-10 0 10", true);
		AICF_SuppressionCase(shooter, side + "_QUADRANT_NN", "-10 0 -10", true);
	}

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFSuppressionProbeStarted || !System.GetCLIParam("aicfSuppressionInputProbe", enabled))
			return;
		if (enabled != "1" && enabled != "2")
			return;
		m_bAICFSuppressionProbeStarted = true;
		if (enabled == "2")
		{
			GetGame().GetCallqueue().CallLater(AICF_FinishSuppressionProbe, 30000, false);
			Print("[AICF][SUPPRESSION_NATIVE_REPRO] direction=0,0,1 expected=DIVISION_BY_ZERO guard=BYPASSED");
			AICF_SuppressionNativeReproBox repro = new AICF_SuppressionNativeReproBox("7 -2 7", "13 2 13");
			repro.AICF_ReproduceZeroSlope();
			return;
		}
		AICF_TestSuppressionGeometry(m_USState.GetSlot(0), "US");
		AICF_TestSuppressionGeometry(m_USSRState.GetSlot(0), "USSR");
		AICF_SuppressionCheck("NULL_VOLUME", !AICF_SuppressionInputGuard.CanGenerateLine(null, vector.Zero));
		SCR_AISuppressionVolumeBox box = new SCR_AISuppressionVolumeBox("-1 -1 -1", "1 1 1");
		AICF_SuppressionCheck("NEAR_ZERO_SLOPE_REJECTED", !AICF_SuppressionInputGuard.CanGenerateLine(box, "10 0 0.000001"));
		AICF_SuppressionCheck("SMALL_NONZERO_SLOPE_ALLOWED", AICF_SuppressionInputGuard.CanGenerateLine(box, "10 0 0.0001"));
		AICF_SuppressionCheck("NEAR_CENTER_REJECTED", !AICF_SuppressionInputGuard.CanGenerateLine(box, "0 0 0.0005"));
		AICF_SuppressionCheck("OUTSIDE_CENTER_TOLERANCE_ALLOWED", AICF_SuppressionInputGuard.CanGenerateLine(box, "0 0 0.002"));
		box.m_vBBMax[0] = -1;
		AICF_SuppressionCheck("ZERO_WIDTH", !AICF_SuppressionInputGuard.CanGenerateLine(box, "10 0 10"));
		box.m_vBBMax = "1 1 -1";
		AICF_SuppressionCheck("ZERO_DEPTH", !AICF_SuppressionInputGuard.CanGenerateLine(box, "10 0 10"));
		box.m_vBBMax = "-2 1 1";
		AICF_SuppressionCheck("INVERTED_WIDTH", !AICF_SuppressionInputGuard.CanGenerateLine(box, "10 0 10"));
		box.m_vBBMax = "1 -2 1";
		AICF_SuppressionCheck("INVERTED_HEIGHT", !AICF_SuppressionInputGuard.CanGenerateLine(box, "10 0 10"));
		SCR_AISuppressionVolumeSphere sphere = new SCR_AISuppressionVolumeSphere(vector.Zero, 3);
		AICF_SuppressionCheck("SPHERE_SAME_Z_ALLOWED", AICF_SuppressionInputGuard.CanGenerateLine(sphere, "10 0 0"));
		AICF_SuppressionCheck("SPHERE_SAME_XZ_REJECTED", !AICF_SuppressionInputGuard.CanGenerateLine(sphere, "0 10 0"));
		Print(string.Format("[AICF][SUPPRESSION_INPUT_FINISHED] checks=%1 failures=%2", m_iAICFSuppressionChecks, m_iAICFSuppressionFailures));
		GetGame().GetCallqueue().CallLater(AICF_FinishSuppressionProbe, 30000, false);
	}

	protected void AICF_FinishSuppressionProbe()
	{
		GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(AICF_FinishSuppressionProbe);
		super.Stop(cleanupEntities);
	}
}
