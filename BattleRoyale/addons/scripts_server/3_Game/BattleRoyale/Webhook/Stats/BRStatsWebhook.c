#ifdef SERVER
class BRStatsPayload
{
	string event_name;
	int players;
	string winner_name;
}

class BRStatsCallback: RestCallback
{
	override void OnError(int errorCode)
	{
		BattleRoyaleUtils.Warn("Stats webhook error: " + errorCode);
	}

	override void OnTimeout()
	{
		BattleRoyaleUtils.Warn("Stats webhook timeout");
	}
}

class BRStatsWebhook
{
	static void Send(string url, string eventName, int players, string winnerName)
	{
		if (url == "")
			return;

		BRStatsPayload payload = new BRStatsPayload();
		payload.event_name = eventName;
		payload.players = players;
		payload.winner_name = winnerName;

		string body;
		string errorMessage;
		if (!JsonFileLoader<BRStatsPayload>.MakeData(payload, body, errorMessage))
		{
			BattleRoyaleUtils.Warn("Stats webhook JSON failed: " + errorMessage);
			return;
		}

		RestApi restApi = GetRestApi();
		if (!restApi)
		{
			BattleRoyaleUtils.Warn("Stats webhook: RestApi unavailable");
			return;
		}

		RestContext ctx = restApi.GetRestContext(url);
		ctx.SetHeader("application/json");
		ctx.POST(new BRStatsCallback(), "", body);
	}
}
#endif
