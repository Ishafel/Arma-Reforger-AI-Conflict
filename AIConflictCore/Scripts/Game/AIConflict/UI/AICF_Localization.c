// Сервер передаёт ключи и параметры, не выбирая язык получателя.
// Формат не содержит разделителей campaign summary: |, ~, ;.
// Длины параметров позволяют вкладывать сообщения и сохранять знаки пунктуации.
class AICF_Localization
{
	// Коды остаются неизменными в gameplay, RPC и логах.
	static string Code(string code)
	{
		switch (code)
		{
			case "ATTACK": return "{AICF:AICF_State_ATTACK}";
			case "DEFEND": return "{AICF:AICF_State_DEFEND}";
			case "RESERVE": return "{AICF:AICF_State_RESERVE}";
			case "INFANTRY": return "{AICF:AICF_State_INFANTRY}";
			case "MOTORIZED_LIGHT": return "{AICF:AICF_State_MOTORIZED_LIGHT}";
			case "MOTORIZED_TRUCK": return "{AICF:AICF_State_MOTORIZED_TRUCK}";
			case "MOTORIZED_ARMED_LIGHT": return "{AICF:AICF_State_MOTORIZED_ARMED_LIGHT}";
			case "LIGHT 4X4": return "{AICF:AICF_State_LIGHT_4X4}";
			case "TRUCK": return "{AICF:AICF_State_TRUCK}";
			case "ARMED 4X4": return "{AICF:AICF_State_ARMED_4X4}";
			case "EMPTY": return "{AICF:AICF_State_EMPTY}";
			case "SPAWNING": return "{AICF:AICF_State_SPAWNING}";
			case "READY": return "{AICF:AICF_State_READY}";
			case "DESTROYED": return "{AICF:AICF_State_DESTROYED}";
			case "WAITING": return "{AICF:AICF_State_WAITING}";
			case "ORDER_RECOVERY": return "{AICF:AICF_State_ORDER_RECOVERY}";
			case "AWAITING_PLAYER_COMMAND": return "{AICF:AICF_State_AWAITING_PLAYER_COMMAND}";
			case "HEALTHY": return "{AICF:AICF_State_HEALTHY}";
			case "STRAINED": return "{AICF:AICF_State_STRAINED}";
			case "ISOLATED": return "{AICF:AICF_State_ISOLATED}";
			case "BLOCKED": return "{AICF:AICF_State_BLOCKED}";
			case "UNKNOWN": return "{AICF:AICF_State_UNKNOWN}";
			case "UNAVAILABLE": return "{AICF:AICF_State_UNAVAILABLE}";
			case "NONE": return "{AICF:AICF_State_NONE}";
			case "BASE": return "{AICF:AICF_State_BASE}";
			case "QRF": return "{AICF:AICF_State_QRF}";
			case "GARRISON": return "{AICF:AICF_State_GARRISON}";
			case "ASSAULT": return "{AICF:AICF_State_ASSAULT}";
			case "HOLD": return "{AICF:AICF_State_HOLD}";
			case "MANEUVER": return "{AICF:AICF_State_MANEUVER}";
			case "ADVANCE": return "{AICF:AICF_State_ADVANCE}";
			case "SYSTEM_HOLD": return "{AICF:AICF_State_SYSTEM_HOLD}";
			case "ATTACK_PRIMARY": return "{AICF:AICF_State_ATTACK_PRIMARY}";
			case "ATTACK_SECONDARY": return "{AICF:AICF_State_ATTACK_SECONDARY}";
			case "ATTACK_SUPPORT": return "{AICF:AICF_State_ATTACK_SUPPORT}";
			case "FORWARD_DEFEND": return "{AICF:AICF_State_FORWARD_DEFEND}";
			case "IDLE_RESERVE": return "{AICF:AICF_State_IDLE_RESERVE}";
			case "PLAYER_ATTACK": return "{AICF:AICF_State_PLAYER_ATTACK}";
			case "PLAYER_DEFEND": return "{AICF:AICF_State_PLAYER_DEFEND}";
			case "PLAYER_RESERVE": return "{AICF:AICF_State_PLAYER_RESERVE}";
			case "MOVE_AND_HOLD": return "{AICF:AICF_State_MOVE_AND_HOLD}";
		}
		return code;
	}

	static string Key(string text)
	{
		if (text.StartsWith("#"))
			return "{AICF:" + text.Substring(1, text.Length() - 1) + "}";
		return text;
	}

	static string Format(string key, string p1 = "", string p2 = "", string p3 = "", string p4 = "", string p5 = "", string p6 = "", string p7 = "", string p8 = "", string p9 = "")
	{
		array<string> args = {p1, p2, p3, p4, p5, p6, p7, p8, p9};
		string result = key.Substring(0, key.Length() - 1) + ":";
		foreach (string arg : args)
			result += arg.Length().ToString() + ":" + arg;
		return result + "}";
	}

	// Вызывается только на границе отображения. Язык задаёт сам клиент через
	// штатный WidgetManager; shared state и язык dedicated server не меняются.
	static string Resolve(string text, int depth = 0)
	{
		if (depth >= 8 || text.Length() > 8191) return text;
		string result;
		int cursor;
		int count;
		while (cursor < text.Length())
		{
			int start = text.IndexOfFrom(cursor, "{AICF:");
			if (start < 0) break;
			result += text.Substring(cursor, start - cursor);
			int end;
			string translated;
			if (++count > 128 || !ReadToken(text, start, depth, end, translated))
				return result + text.Substring(start, text.Length() - start);
			result += translated;
			cursor = end;
		}
		result += text.Substring(cursor, text.Length() - cursor);
		if (count == 0 && result.StartsWith("#")) return WidgetManager.Translate(result);
		return result;
	}

	protected static bool ReadToken(string text, int start, int depth, out int end, out string translated)
	{
		int keyStart = start + 6;
		int close = text.IndexOfFrom(keyStart, "}");
		int colon = text.IndexOfFrom(keyStart, ":");
		if (close < 0) return false;
		if (colon < 0 || close < colon)
		{
			translated = Template("#" + text.Substring(keyStart, close - keyStart));
			end = close + 1;
			return true;
		}
		string key = "#" + text.Substring(keyStart, colon - keyStart);
		int cursor = colon + 1;
		array<string> args = {};
		for (int i = 0; i < 9; i++)
		{
			int separator = text.IndexOfFrom(cursor, ":");
			if (separator < 0 || separator - cursor > 4 || separator == cursor) return false;
			string digits = text.Substring(cursor, separator - cursor);
			int length = digits.ToInt(-1);
			if (length < 0 || length.ToString() != digits || length > text.Length() - separator - 1) return false;
			cursor = separator + 1;
			args.Insert(Resolve(text.Substring(cursor, length), depth + 1));
			cursor += length;
		}
		if (cursor >= text.Length() || text.Substring(cursor, 1) != "}") return false;
		translated = string.Format(Template(key), args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);
		end = cursor + 1;
		return true;
	}

	protected static string Template(string key)
	{
		string text = WidgetManager.Translate(key);
		// В config-ресурсе slash-n хранится буквально. Обрабатываем шаблон
		// до подстановки параметров, не меняя пользовательский текст.
		text.Replace("\\n", "\n");
		return text;
	}
}
