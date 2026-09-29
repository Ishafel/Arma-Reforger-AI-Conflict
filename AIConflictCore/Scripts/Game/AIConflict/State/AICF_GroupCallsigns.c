// Отдельный Fisher–Yates для каждой стороны при создании faction state.
// ID записи не зависит от позиции в перемешанном пуле или языка клиента.
class AICF_GroupCallsigns
{
	static void BuildShuffledPool(FactionKey stableFaction, array<string> pool)
	{
		pool.Clear();
		if (!Replication.IsServer())
			return;
		if (stableFaction == "US")
		{
			pool.Insert("AICF_Callsign_US_Viper");
			pool.Insert("AICF_Callsign_US_Raptor");
			pool.Insert("AICF_Callsign_US_Hammer");
			pool.Insert("AICF_Callsign_US_Anvil");
			pool.Insert("AICF_Callsign_US_Falcon");
			pool.Insert("AICF_Callsign_US_Talon");
			pool.Insert("AICF_Callsign_US_Sabre");
			pool.Insert("AICF_Callsign_US_Raider");
			pool.Insert("AICF_Callsign_US_Nomad");
			pool.Insert("AICF_Callsign_US_Ghost");
			pool.Insert("AICF_Callsign_US_Titan");
			pool.Insert("AICF_Callsign_US_Sentinel");
			pool.Insert("AICF_Callsign_US_Havoc");
			pool.Insert("AICF_Callsign_US_Ranger");
			pool.Insert("AICF_Callsign_US_Coyote");
			pool.Insert("AICF_Callsign_US_Kodiak");
			pool.Insert("AICF_Callsign_US_Bulldog");
			pool.Insert("AICF_Callsign_US_Raven");
			pool.Insert("AICF_Callsign_US_Dagger");
			pool.Insert("AICF_Callsign_US_Wraith");
			pool.Insert("AICF_Callsign_US_Spartan");
			pool.Insert("AICF_Callsign_US_Hunter");
			pool.Insert("AICF_Callsign_US_Striker");
			pool.Insert("AICF_Callsign_US_Vanguard");
			pool.Insert("AICF_Callsign_US_Scorpion");
			pool.Insert("AICF_Callsign_US_Wolfpack");
			pool.Insert("AICF_Callsign_US_Specter");
			pool.Insert("AICF_Callsign_US_Reaper");
			pool.Insert("AICF_Callsign_US_Grizzly");
			pool.Insert("AICF_Callsign_US_Tombstone");
			pool.Insert("AICF_Callsign_US_Rattler");
			pool.Insert("AICF_Callsign_US_Pathfinder");
			pool.Insert("AICF_Callsign_US_Deadeye");
			pool.Insert("AICF_Callsign_US_Longbow");
			pool.Insert("AICF_Callsign_US_Nightwatch");
			pool.Insert("AICF_Callsign_US_Wildcat");
			pool.Insert("AICF_Callsign_US_Thunderbolt");
			pool.Insert("AICF_Callsign_US_Blackbird");
			pool.Insert("AICF_Callsign_US_Redtail");
			pool.Insert("AICF_Callsign_US_Cutlass");
			pool.Insert("AICF_Callsign_US_Ironwolf");
			pool.Insert("AICF_Callsign_US_Blackout");
			pool.Insert("AICF_Callsign_US_Warhawk");
			pool.Insert("AICF_Callsign_US_Bonebreaker");
			pool.Insert("AICF_Callsign_US_Hellhound");
			pool.Insert("AICF_Callsign_US_Crosshair");
			pool.Insert("AICF_Callsign_US_Drifter");
			pool.Insert("AICF_Callsign_US_Vulture");
			pool.Insert("AICF_Callsign_US_Fang");
			pool.Insert("AICF_Callsign_US_Ironfist");
		}
		else if (stableFaction == "USSR")
		{
			pool.Insert("AICF_Callsign_USSR_Berkut");
			pool.Insert("AICF_Callsign_USSR_Bars");
			pool.Insert("AICF_Callsign_USSR_Buran");
			pool.Insert("AICF_Callsign_USSR_Granit");
			pool.Insert("AICF_Callsign_USSR_Kedr");
			pool.Insert("AICF_Callsign_USSR_Rubezh");
			pool.Insert("AICF_Callsign_USSR_Shkval");
			pool.Insert("AICF_Callsign_USSR_Korshun");
			pool.Insert("AICF_Callsign_USSR_Vikhr");
			pool.Insert("AICF_Callsign_USSR_Sokol");
			pool.Insert("AICF_Callsign_USSR_Taifun");
			pool.Insert("AICF_Callsign_USSR_Kaskad");
			pool.Insert("AICF_Callsign_USSR_Zenit");
			pool.Insert("AICF_Callsign_USSR_Grom");
			pool.Insert("AICF_Callsign_USSR_Ratnik");
			pool.Insert("AICF_Callsign_USSR_Vostok");
			pool.Insert("AICF_Callsign_USSR_Uragan");
			pool.Insert("AICF_Callsign_USSR_Molniya");
			pool.Insert("AICF_Callsign_USSR_Strazh");
			pool.Insert("AICF_Callsign_USSR_Oplot");
			pool.Insert("AICF_Callsign_USSR_Almaz");
			pool.Insert("AICF_Callsign_USSR_Rys");
			pool.Insert("AICF_Callsign_USSR_Yastreb");
			pool.Insert("AICF_Callsign_USSR_Kobra");
			pool.Insert("AICF_Callsign_USSR_Smerch");
			pool.Insert("AICF_Callsign_USSR_Bastion");
			pool.Insert("AICF_Callsign_USSR_Volk");
			pool.Insert("AICF_Callsign_USSR_Krechet");
			pool.Insert("AICF_Callsign_USSR_Metel");
			pool.Insert("AICF_Callsign_USSR_Fakel");
			pool.Insert("AICF_Callsign_USSR_Dozor");
			pool.Insert("AICF_Callsign_USSR_Klin");
			pool.Insert("AICF_Callsign_USSR_Taran");
			pool.Insert("AICF_Callsign_USSR_Zarya");
			pool.Insert("AICF_Callsign_USSR_Tuman");
			pool.Insert("AICF_Callsign_USSR_Iskra");
			pool.Insert("AICF_Callsign_USSR_Plamya");
			pool.Insert("AICF_Callsign_USSR_Utyos");
			pool.Insert("AICF_Callsign_USSR_Kremen");
			pool.Insert("AICF_Callsign_USSR_Step");
			pool.Insert("AICF_Callsign_USSR_Polyus");
			pool.Insert("AICF_Callsign_USSR_Burya");
			pool.Insert("AICF_Callsign_USSR_Okhotnik");
			pool.Insert("AICF_Callsign_USSR_Skat");
			pool.Insert("AICF_Callsign_USSR_Volna");
			pool.Insert("AICF_Callsign_USSR_Sapsan");
			pool.Insert("AICF_Callsign_USSR_Voron");
			pool.Insert("AICF_Callsign_USSR_Zubr");
			pool.Insert("AICF_Callsign_USSR_Shchit");
			pool.Insert("AICF_Callsign_USSR_Krepost");
		}
		for (int i = pool.Count() - 1; i > 0; i--)
		{
			int other = Math.RandomInt(0, i + 1);
			string saved = pool[i];
			pool[i] = pool[other];
			pool[other] = saved;
		}
	}
}
