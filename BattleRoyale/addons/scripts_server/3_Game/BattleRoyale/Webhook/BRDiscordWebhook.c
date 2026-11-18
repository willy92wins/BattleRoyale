#ifdef SERVER
//! Discord's allowed_mentions with an empty parse list: a name or a mention in the message pings nobody
class BRDiscordMentions
{
	ref array<string> parse = new array<string>;
}

//! Body of a Discord webhook message
class BRDiscordPayload
{
	string content;
	ref BRDiscordMentions allowed_mentions = new BRDiscordMentions;
}

//! Answer of one Discord webhook post; m_Label names it in the log ("<label> sent", "<label> error: <code>")
class BRDiscordCallback: RestCallback
{
	//! Set by whoever creates the callback, before Post; no constructor argument, so subclasses need not pass one up
	string m_Label = "Discord webhook";
	//! Set by the first answer of the engine, whichever it is
	bool m_Answered;

	//! Runs once Discord accepted the message; subclasses record that it went out
	void OnSent()
	{
	}

	override void OnError(int errorCode)
	{
		m_Answered = true;
		BattleRoyaleUtils.Warn(m_Label + " error: " + errorCode);
	}

	override void OnTimeout()
	{
		m_Answered = true;
		BattleRoyaleUtils.Warn(m_Label + " timeout");
	}

	override void OnSuccess(string data, int dataSize)
	{
		m_Answered = true;
		BattleRoyaleUtils.Info(m_Label + " sent");
		OnSent();
	}
}

//! Posts a message to a Discord webhook. The URL is never logged: whoever has it can post to the channel
class BRDiscordWebhook
{
	//! Keeps the callbacks of the requests in flight alive until the engine answers them
	protected static ref array<ref BRDiscordCallback> s_Callbacks = new array<ref BRDiscordCallback>;

	//! Drops the callbacks Discord already answered; called at the head of Post and IsPending, never from a callback
	protected static void PruneAnswered()
	{
		if (!s_Callbacks)
			s_Callbacks = new array<ref BRDiscordCallback>;

		for (int i = s_Callbacks.Count() - 1; i >= 0; i--)
		{
			BRDiscordCallback answered = s_Callbacks[i];
			if (!answered || answered.m_Answered)
				s_Callbacks.Remove(i);
		}
	}

	//! True while a posted message has no answer yet: the server exit waits for it (BattleRoyaleRestart.Shutdown)
	static bool IsPending()
	{
		PruneAnswered();
		return s_Callbacks.Count() > 0;
	}

	static void Post(string url, string content, BRDiscordCallback callback)
	{
		PruneAnswered();

		if (url == "" || content == "" || !callback)
			return;

		BRDiscordPayload payload = new BRDiscordPayload;
		payload.content = content;

		string body;
		string errorMessage;
		if (!JsonFileLoader<BRDiscordPayload>.MakeData(payload, body, errorMessage, false))
		{
			BattleRoyaleUtils.Warn(callback.m_Label + " JSON failed: " + errorMessage);
			return;
		}

		RestApi restApi = GetRestApi();
		if (!restApi)
		{
			BattleRoyaleUtils.Info(callback.m_Label + ": no RestApi yet, creating it");
			restApi = CreateRestApi();
		}

		if (!restApi)
		{
			BattleRoyaleUtils.Warn(callback.m_Label + ": RestApi unavailable");
			return;
		}

		RestContext ctx = restApi.GetRestContext(PostUrl(url));
		if (!ctx)
		{
			BattleRoyaleUtils.Warn(callback.m_Label + ": no REST context for the URL");
			return;
		}

		ctx.SetHeader("application/json");
		s_Callbacks.Insert(callback);
		ctx.POST(callback, "", body);
	}

	//! Discord answers a webhook with 204 and no body, which the engine reports as an error (OnError 8, measured with
	//! DayZDiag 1.30). With wait=true it answers 200 with the message it posted. Added to a Discord URL whose query has
	//! no wait parameter, before any #fragment (a fragment is not sent)
	static string PostUrl(string url)
	{
		if (!url.Contains("/api/webhooks/"))
			return url;

		string address = url;
		string fragment = "";
		int hash = url.IndexOf("#");
		if (hash != -1)
		{
			address = url.Substring(0, hash);
			fragment = url.Substring(hash, url.Length() - hash);
		}

		if (address.Contains("?wait=") || address.Contains("&wait="))
			return url;

		if (address.Contains("?"))
			return address + "&wait=true" + fragment;

		return address + "?wait=true" + fragment;
	}
}
#endif
