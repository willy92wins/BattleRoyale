modded class CarScript
{
	static const float CONTACT_DAMAGE_SCALE = 0.5;

	override void EEInit()
	{
		super.EEInit();
		m_dmgContactCoef = m_dmgContactCoef * CONTACT_DAMAGE_SCALE;
	}
}
