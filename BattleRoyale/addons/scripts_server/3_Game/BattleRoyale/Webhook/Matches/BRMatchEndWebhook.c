#ifdef SERVER
//! Discord's allowed_mentions with an empty parse list: a name or a mention in the message pings nobody
class BRMatchEndMentions
{
	ref array<string> parse = new array<string>;
}

//! Body of a Discord webhook message
class BRMatchEndPayload
{
	string content;
	ref BRMatchEndMentions allowed_mentions = new BRMatchEndMentions;
}

class BRMatchEndCallback: RestCallback
{
	//! Set by the first answer of the engine, whichever it is
	bool m_Answered;

	override void OnError(int errorCode)
	{
		m_Answered = true;
		BattleRoyaleUtils.Warn("Match end webhook error: " + errorCode);
	}

	override void OnTimeout()
	{
		m_Answered = true;
		BattleRoyaleUtils.Warn("Match end webhook timeout");
	}

	override void OnSuccess(string data, int dataSize)
	{
		m_Answered = true;
		BattleRoyaleUtils.Info("Match end webhook sent");
	}
}

//! Message posted to a Discord webhook when a match ends (server_settings.json: match_end_webhook_url,
//! match_end_message and match_end_message_no_winner). The URL is never logged: whoever has it can post to the channel
class BRMatchEndWebhook
{
	//! Keeps the callback of the request in flight alive until the engine answers; there is one request per match
	protected static ref BRMatchEndCallback s_Callback;

	//! True while the last message sent has no answer yet: the server exit waits for it (BattleRoyaleRestart.Shutdown)
	static bool IsPending()
	{
		return s_Callback && !s_Callback.m_Answered;
	}

	//! Fills the placeholders of the message. The names go in last, so a player called "{kills}" keeps that name
	static string BuildMessage(string messageFormat, string winners, int kills, int players, int minutes)
	{
		string message = messageFormat;
		message.Replace("{kills}", kills.ToString());
		message.Replace("{players}", players.ToString());
		message.Replace("{duration}", minutes.ToString());
		message.Replace("{winner}", winners);
		return message;
	}

	static void Send(string url, string message)
	{
		if (url == "" || message == "")
			return;

		BRMatchEndPayload payload = new BRMatchEndPayload;
		payload.content = message;

		string body;
		string errorMessage;
		if (!JsonFileLoader<BRMatchEndPayload>.MakeData(payload, body, errorMessage, false))
		{
			BattleRoyaleUtils.Warn("Match end webhook JSON failed: " + errorMessage);
			return;
		}

		RestApi restApi = GetRestApi();
		if (!restApi)
		{
			BattleRoyaleUtils.Info("Match end webhook: no RestApi yet, creating it");
			restApi = CreateRestApi();
		}

		if (!restApi)
		{
			BattleRoyaleUtils.Warn("Match end webhook: RestApi unavailable");
			return;
		}

		RestContext ctx = restApi.GetRestContext(url);
		if (!ctx)
		{
			BattleRoyaleUtils.Warn("Match end webhook: no REST context for the URL");
			return;
		}

		ctx.SetHeader("application/json");
		s_Callback = new BRMatchEndCallback;
		ctx.POST(s_Callback, "", body);
	}
}
#endif
