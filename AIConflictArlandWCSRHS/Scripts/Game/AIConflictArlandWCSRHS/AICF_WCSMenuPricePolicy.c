// Дополнение той же политики для техники WCS. Core не знает об этих prefab.
modded class AICF_RHSMenuPricePolicy
{
	override int VehicleCost(string path)
	{
		if (path.StartsWith("Prefabs/Vehicles/Tracked/M1A1/"))
		{
			int cost = 3000;
			if (path.Contains("/M1A2/")) cost += 500;
			if (path.Contains("/SEPv2/")) cost += 250;
			if (path.Contains("_TUSK")) cost += 500;
			if (path.Contains("_TUSK_II")) cost += 250;
			return cost;
		}
		if (path.StartsWith("Prefabs/Vehicles/Tracked/T72A/")) return 3000;
		if (path.StartsWith("Prefabs/Vehicles/Tracked/T72B/")) return 3500;
		if (path.StartsWith("Prefabs/Vehicles/Tracked/BMP3/")) return 2250;
		if (path.StartsWith("Prefabs/Vehicles/Tracked/M2A2/"))
		{
			int cost = 2000;
			if (path.Contains("/M2A3/")) cost += 250;
			if (path.Contains("_BUSK")) cost += 250;
			return cost;
		}
		if (path == "Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_Kornet.et") return 675;
		return super.VehicleCost(path);
	}
}
