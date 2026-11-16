#ifndef SERVER
//! Silent mode: every call of the options menu that creates, tests, applies or reverts its GameOptions runs between
//! BattleRoyaleSilentMode.BeginOptions and EndOptions, so the options never see the 0 of the mode, the VOIP volume
//! they set is kept as the choice of the player, and the voices are muted again right after
modded class OptionsMenu
{
	override Widget Init()
	{
		BattleRoyaleSilentMode.BeginOptions(true);
		Widget root = super.Init();
		BattleRoyaleSilentMode.EndOptions(m_Options);
		return root;
	}

	override void Apply()
	{
		BattleRoyaleSilentMode.BeginOptions(false);
		super.Apply();
		BattleRoyaleSilentMode.EndOptions(m_Options);
	}

	//! Back may close the menu: the options are kept by a local reference until the end
	override void Back()
	{
		GameOptions options = m_Options;
		BattleRoyaleSilentMode.BeginOptions(false);
		super.Back();
		BattleRoyaleSilentMode.EndOptions(options);
	}

	override void Reset()
	{
		BattleRoyaleSilentMode.BeginOptions(false);
		super.Reset();
		BattleRoyaleSilentMode.EndOptions(m_Options);
	}

	override void ResetCurrentTab()
	{
		BattleRoyaleSilentMode.BeginOptions(false);
		super.ResetCurrentTab();
		BattleRoyaleSilentMode.EndOptions(m_Options);
	}

	override void ReloadOptions()
	{
		BattleRoyaleSilentMode.BeginOptions(false);
		super.ReloadOptions();
		BattleRoyaleSilentMode.EndOptions(m_Options);
	}

	//! The dialog that leaves without saving reverts the options and closes the menu
	override bool OnModalResult(Widget w, int x, int y, int code, int result)
	{
		GameOptions options = m_Options;
		BattleRoyaleSilentMode.BeginOptions(false);
		bool handled = super.OnModalResult(w, x, y, code, result);
		BattleRoyaleSilentMode.EndOptions(options);
		return handled;
	}
}

modded class OptionsMenuSounds
{
	//! The slider writes the VOIP option: moved by the player, reverted or set to its default (SetValue with update)
	override void UpdateVOIP(float value)
	{
		BattleRoyaleSilentMode.BeginOptions(false);
		super.UpdateVOIP(value);
		BattleRoyaleSilentMode.OnOptionsVOIPChanged();
		BattleRoyaleSilentMode.EndOptions(m_Options);
	}
}

#ifndef PLATFORM_PS4
#ifndef PLATFORM_XBOX
modded class OptionsMenuVideo
{
	//! Tests every changed option, the VOIP one too, before the options are created again
	override void OnOverallQualityChanged(int value)
	{
		BattleRoyaleSilentMode.BeginOptions(false);
		super.OnOverallQualityChanged(value);
		BattleRoyaleSilentMode.EndOptions(m_Options);
	}
}
#endif
#endif
#endif
