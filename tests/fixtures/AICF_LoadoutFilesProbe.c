// Дополнение к AICF_LoadoutQolProbe.c; только isolated Core/UI, после него.
// Читает реальные bytes всех canonical legacy/generation файлов до/после Apply.
class AICF_LoadoutFileImage
{
	static bool Read(out string image)
	{
		image = string.Empty;
		for (int id; id < AICF_LoadoutStore.MAX_TEMPLATES; id++)
		{
			for (int generation = -1; generation <= 1; generation++)
			{
				string suffix = ".json";
				if (generation >= 0) suffix = "_" + generation.ToString() + ".json";
				string path = AICF_LoadoutStore.DIRECTORY + "/template_" + id.ToString() + suffix;
				image += id.ToString() + "/" + generation.ToString() + "=";
				if (!FileIO.FileExists(path)) { image += "MISSING\n"; continue; }
				FileHandle file = FileIO.OpenFile(path, FileMode.READ);
				if (!file) return false;
				int length = file.GetLength();
				if (length < 0 || length > 600000) { file.Close(); return false; }
				string data;
				int count = file.Read(data, length);
				file.Close();
				if (count != length) return false;
				image += length.ToString() + ":" + data + "\n";
			}
		}
		return true;
	}
}

modded class AICF_MatchController
{
	override void RequestLoadout(SCR_PlayerController player, int token, int slot, int member, int revision, int operation, string payload,
		string targetFaction = "", int libraryRevision = -1)
	{
		string enabled;
		bool apply = (operation == 1 || operation == 5 || operation == 7) &&
			System.GetCLIParam("aicfLoadoutFilesProbe", enabled) && enabled == "1";
		string before, after;
		bool readable;
		if (apply) readable = AICF_LoadoutFileImage.Read(before);
		super.RequestLoadout(player, token, slot, member, revision, operation, payload, targetFaction, libraryRevision);
		if (apply)
			AICF_QolChecks.Check("FILE_IMAGE_UNCHANGED_" + token.ToString(), readable && AICF_LoadoutFileImage.Read(after) && before == after);
	}
}
