#ifdef VPPADMINTOOLS
#ifndef SERVER
modded class VPPAdminHud
{
    override void DefineButtons()
    {
        super.DefineButtons();
        
        InsertButton("MenuBattleRoyaleManager" , "BattleRoyale", "set:dayz_gui_vpp image:vpp_icon_obj_editor", "Manage the Battle Royale");
    }

    override void HideMenu()
    {
        MenuBattleRoyaleManager brMenu = MenuBattleRoyaleManager.Cast(GetSubMenuByType(MenuBattleRoyaleManager));
        if (brMenu)
            brMenu.OnToolbarHidden();

        super.HideMenu();
    }
};
#endif
#endif