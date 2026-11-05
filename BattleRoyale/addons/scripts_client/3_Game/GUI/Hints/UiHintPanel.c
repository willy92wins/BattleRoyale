#ifndef SERVER
// The vanilla class lives in 3_Game; a modded class in 5_Mission does not compile at mission load
// ("Unknown type 'UiHintPanel'"), so the override sits in this module.
modded class UiHintPanel
{
	//! Battle Royale hints, in the data PBO (prefix Vigrid-BattleRoyale\Data)
	protected static const string BR_HINTS_PATH = "Vigrid-BattleRoyale/Data/hints.json";

	//! Loads the Battle Royale hints instead of the vanilla ones; m_DataPath is const, so the loader itself is replaced
	override protected void LoadContentList()
	{
		string errorMessage;
		if (JsonFileLoader<array<ref HintPage>>.LoadFile(BR_HINTS_PATH, m_ContentList, errorMessage) && m_ContentList && m_ContentList.Count() > 0)
			return;

		// Fall back to the vanilla hints so the panel still has content
		ErrorEx("Battle Royale hints not loaded from " + BR_HINTS_PATH + ": " + errorMessage);
		super.LoadContentList();
	}
}
#endif
