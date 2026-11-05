#ifdef SERVER
// Local load tests only, never shipped: fills the lobby with server-side dummy survivors so a
// match runs with many players on one machine. The number of dummies is the first line of
// $profile:br_loadtest.txt (no file or 0 = none). Dummies have no PlayerIdentity: they load the
// server logic and the simulation, not the network or any client.

modded class BattleRoyaleDebug
{
    protected int m_LoadTestSpawned;

    override void Activate()
    {
        super.Activate();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SpawnLoadTestDummies, 5000, false);
    }

    protected int ReadLoadTestCount()
    {
        if (!FileExist("$profile:br_loadtest.txt"))
            return 0;

        FileHandle file = OpenFile("$profile:br_loadtest.txt", FileMode.READ);
        if (!file)
            return 0;

        string line;
        FGets(file, line);
        CloseFile(file);
        return line.ToInt();
    }

    void SpawnLoadTestDummies()
    {
        int count = ReadLoadTestCount();
        vector center = GetCenter();

        for (int i = 0; i < count; i++)
        {
            vector pos = center;
            pos[0] = center[0] + Math.RandomFloatInclusive(-15, 15);
            pos[2] = center[2] + Math.RandomFloatInclusive(-15, 15);
            pos[1] = GetGame().SurfaceY(pos[0], pos[2]);

            PlayerBase dummy = PlayerBase.Cast(GetGame().CreateObjectEx("SurvivorM_Mirek", pos, ECE_PLACE_ON_SURFACE | ECE_INITAI | ECE_EQUIP_ATTACHMENTS));
            if (!dummy)
                continue;

            dummy.OnSpawnedFromConsole();
            dummy.player_steamid = "loadtest" + i.ToString();
            AddPlayer(dummy);
            m_LoadTestSpawned++;
        }

        BattleRoyaleUtils.Info("[LoadTest] spawned " + m_LoadTestSpawned.ToString() + " dummies of " + count.ToString());
    }
}
#endif
