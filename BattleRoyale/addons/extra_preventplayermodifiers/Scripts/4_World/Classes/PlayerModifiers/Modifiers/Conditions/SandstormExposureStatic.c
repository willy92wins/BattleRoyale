modded class SandstormExposureStaticMdfr
{
	override bool ActivateCondition(PlayerBase player)
	{
		return false;
	}

	// Direct requests skip ActivateCondition (DisableActivateCheck).
	override void ActivateRequest(EActivationType trigger)
	{
	}
}
