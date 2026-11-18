#ifdef SERVER
//! Message posted to a Discord webhook when a match ends (server_settings.json: match_end_webhook_url,
//! match_end_message and match_end_message_no_winner). The URL is never logged: whoever has it can post to the channel
class BRMatchEndWebhook
{
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

		BRDiscordCallback callback = new BRDiscordCallback;
		callback.m_Label = "Match end webhook";
		BRDiscordWebhook.Post(url, message, callback);
	}
}
#endif
