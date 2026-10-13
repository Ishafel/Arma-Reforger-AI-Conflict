class AICF_SocialConfig
{
	static const int TICK_MS = 250;
	static const int COOLDOWN_MS = 30000;
	static const float ENTER_METERS = 5;
	static const float EXIT_METERS = 7;
	static const int PREPARE_MS = 4000;
	static const int SALUTE_MS = 3500;
	static const int IDLE_MS = 20000;
	static const ResourceName GREETING_PROJECT = "Sounds/VO/Tutorial/Tutorial_VO_Greetings_Soldier_E.acp";
	static const string GREETING_EVENT = "SOUND_TUTORIAL_AM_GREETING_C_SOLDIER_E_01";

	// Порядок не зависит от role-local имени и глобального random stream.
	static int Variant(int seed, int member)
	{
		return Math.AbsInt((seed % 997) + member) % 4;
	}

	static int Delay(int seed, int member)
	{
		return 750 + (Math.AbsInt((seed % 997) * 17 + member * 613) % 4250);
	}
}
