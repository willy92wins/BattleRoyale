#ifdef VPPADMINTOOLS
#ifndef SERVER
class MenuBattleRoyaleManager: AdminHudSubMenu
{
    protected static const int SUB_POLL_MS = 500;
    protected static const int SUB_RENEW_MS = 2000;

    protected static const int AVAIL_NEXT = 1;
    protected static const int AVAIL_FREEZE = 2;
    protected static const int AVAIL_ADJUST = 4;
    protected static const int AVAIL_FORCESTART = 8;
    protected static const int AVAIL_RESTART = 16;

    protected Widget m_BadgeFrozen;
    protected TextWidget m_TxtBadgeFrozen;
    protected TextWidget m_TxtPhase;
    protected TextWidget m_TxtState;
    protected TextWidget m_TxtTimeLeft;
    protected TextWidget m_TxtAlive;
    protected TextWidget m_TxtZone;

    protected ButtonWidget m_BtnNextPhase;
    protected ButtonWidget m_BtnFreeze;
    protected TextWidget m_LblFreeze;
    protected ImageWidget m_ImgFreeze;
    protected ButtonWidget m_BtnTimeMinus5;
    protected ButtonWidget m_BtnTimeMinus1;
    protected ButtonWidget m_BtnTimePlus1;
    protected ButtonWidget m_BtnTimePlus5;
    protected ButtonWidget m_BtnForceStart;
    protected ButtonWidget m_BtnRestart;
    protected TextWidget m_TxtFeedback;

    protected bool m_Subscribed;
    protected int m_LastRenewMs;
    protected bool m_IsFrozen;
    protected bool m_CanNext;
    protected bool m_CanFreeze;
    protected bool m_CanAdjust;
    protected bool m_CanForceStart;
    protected bool m_CanRestart;
    protected int m_LastStateIdx;
    protected int m_LastMatchId;
    protected int m_PendingConfirmStateIdx;
    protected int m_PendingConfirmMatchId;

    void MenuBattleRoyaleManager()
    {
        m_Subscribed = false;
        m_LastRenewMs = 0;
        m_IsFrozen = false;
        m_CanNext = false;
        m_CanFreeze = false;
        m_CanAdjust = false;
        m_CanForceStart = false;
        m_CanRestart = false;
        m_LastStateIdx = -1;
        m_LastMatchId = 0;
        m_PendingConfirmStateIdx = -1;
        m_PendingConfirmMatchId = 0;

        GetRPCManager().AddRPC(RPC_DAYZBR_NAMESPACE, "AdminStatusUpdate", this, SingleplayerExecutionType.Client);
        GetRPCManager().AddRPC(RPC_DAYZBR_NAMESPACE, "AdminActionResult", this, SingleplayerExecutionType.Client);
    }

    void ~MenuBattleRoyaleManager()
    {
        StopSubscription();
        if (GetGame() && GetRPCManager())
        {
            GetRPCManager().RemoveRPC(RPC_DAYZBR_NAMESPACE, "AdminStatusUpdate");
            GetRPCManager().RemoveRPC(RPC_DAYZBR_NAMESPACE, "AdminActionResult");
        }
    }

    override void OnCreate(Widget RootW)
    {
        super.OnCreate(RootW);

        M_SUB_WIDGET = CreateWidgets("Vigrid-BattleRoyale/Compat/VPP/GUI/layouts/MenuBattleRoyalePanel.layout");
        if (!M_SUB_WIDGET)
            return;

        M_SUB_WIDGET.SetHandler(this);
        m_TitlePanel = Widget.Cast(M_SUB_WIDGET.FindAnyWidget("Header"));
        m_closeButton = ButtonWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnClose"));

        m_BadgeFrozen = M_SUB_WIDGET.FindAnyWidget("BadgeFrozen");
        m_TxtBadgeFrozen = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("TxtBadgeFrozen"));
        m_TxtPhase = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("TxtPhase"));
        m_TxtState = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("TxtState"));
        m_TxtTimeLeft = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("TxtTimeLeft"));
        m_TxtAlive = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("TxtAlive"));
        m_TxtZone = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("TxtZone"));

        m_BtnNextPhase = ButtonWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnNextPhase"));
        m_BtnFreeze = ButtonWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnFreeze"));
        m_LblFreeze = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("LblFreeze"));
        m_ImgFreeze = ImageWidget.Cast(M_SUB_WIDGET.FindAnyWidget("ImgFreeze"));
        m_BtnTimeMinus5 = ButtonWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnTimeMinus5"));
        m_BtnTimeMinus1 = ButtonWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnTimeMinus1"));
        m_BtnTimePlus1 = ButtonWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnTimePlus1"));
        m_BtnTimePlus5 = ButtonWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnTimePlus5"));
        m_BtnForceStart = ButtonWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnForceStart"));
        m_BtnRestart = ButtonWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnRestart"));
        m_TxtFeedback = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("TxtFeedback"));

        ApplyAvailability();
    }

    override void OnAdminHudOpened()
    {
        super.OnAdminHudOpened();

        if (IsSubMenuVisible())
            StartSubscription();
    }

    override void OnMenuShow()
    {
        super.OnMenuShow();

        StartSubscription();
    }

    override void OnMenuHide()
    {
        StopSubscription();

        super.OnMenuHide();
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (super.OnClick(w, x, y, button))
            return true;

        if (w == m_BtnNextPhase)
        {
            if (m_CanNext)
                OpenConfirmDialog("#STR_BR_PANEL_CONFIRM_NEXT_TITLE", "#STR_BR_PANEL_CONFIRM_NEXT_MSG", "OnConfirmNext");
            return true;
        }

        if (w == m_BtnFreeze)
        {
            if (m_CanFreeze)
            {
                if (m_IsFrozen)
                    GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "AdminResume", NULL, true);
                else
                    GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "AdminFreeze", NULL, true);
            }
            return true;
        }

        if (w == m_BtnTimeMinus5)
        {
            if (m_CanAdjust)
                GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "AdminAdjustTime", new Param1<int>(-300), true);
            return true;
        }

        if (w == m_BtnTimeMinus1)
        {
            if (m_CanAdjust)
                GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "AdminAdjustTime", new Param1<int>(-60), true);
            return true;
        }

        if (w == m_BtnTimePlus1)
        {
            if (m_CanAdjust)
                GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "AdminAdjustTime", new Param1<int>(60), true);
            return true;
        }

        if (w == m_BtnTimePlus5)
        {
            if (m_CanAdjust)
                GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "AdminAdjustTime", new Param1<int>(300), true);
            return true;
        }

        if (w == m_BtnForceStart)
        {
            if (m_CanForceStart)
                OpenConfirmDialog("#STR_BR_PANEL_CONFIRM_FORCE_TITLE", "#STR_BR_PANEL_CONFIRM_FORCE_MSG", "OnConfirmForceStart");
            return true;
        }

        if (w == m_BtnRestart)
        {
            if (m_CanRestart)
                OpenConfirmDialog("#STR_BR_PANEL_CONFIRM_RESTART_TITLE", "#STR_BR_PANEL_CONFIRM_RESTART_MSG", "OnConfirmRestart");
            return true;
        }

        return false;
    }

    //! Called by modded VPPAdminHud.HideMenu so status stops immediately when the toolbar closes
    void OnToolbarHidden()
    {
        StopSubscription();
    }

    protected void StartSubscription()
    {
        if (!GetGame())
            return;

        GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "SubscribeAdminStatus", NULL, true);
        m_Subscribed = true;
        m_LastRenewMs = GetGame().GetTime();

        GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(this.SubscriptionTick);
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.SubscriptionTick, SUB_POLL_MS, true);
    }

    protected void StopSubscription()
    {
        if (GetGame())
            GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(this.SubscriptionTick);

        if (m_Subscribed)
        {
            m_Subscribed = false;
            if (GetGame())
                GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "UnsubscribeAdminStatus", NULL, true);
        }
    }

    void SubscriptionTick()
    {
        VPPAdminHud toolbar = GetToolbarMenu();
        bool active = IsSubMenuVisible() && toolbar && toolbar.IsShowing();
        if (!active)
        {
            StopSubscription();
            return;
        }

        int nowMs = GetGame().GetTime();
        if ((nowMs - m_LastRenewMs) >= SUB_RENEW_MS)
        {
            m_LastRenewMs = nowMs;
            GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "SubscribeAdminStatus", NULL, true);
        }
    }

    protected void OpenConfirmDialog(string titleKey, string msgKey, string cbFunc)
    {
        if (!M_SUB_WIDGET || !GetVPPUIManager())
            return;

        VPPDialogBox diag = GetVPPUIManager().CreateDialogBox(M_SUB_WIDGET);
        if (!diag)
            return;

        m_PendingConfirmStateIdx = m_LastStateIdx;
        m_PendingConfirmMatchId = m_LastMatchId;
        diag.InitDiagBox(DIAGTYPE.DIAG_YESNO, Widget.TranslateString(titleKey), Widget.TranslateString(msgKey), this, cbFunc);
    }

    void OnConfirmNext(int result)
    {
        if (result == DIAGRESULT.YES)
            GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "AdminNextPhase", new Param2<int, int>(m_PendingConfirmStateIdx, m_PendingConfirmMatchId), true);
    }

    void OnConfirmForceStart(int result)
    {
        if (result == DIAGRESULT.YES)
            GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "AdminForceStart", new Param2<int, int>(m_PendingConfirmStateIdx, m_PendingConfirmMatchId), true);
    }

    void OnConfirmRestart(int result)
    {
        if (result == DIAGRESULT.YES)
            GetRPCManager().SendRPC(RPC_DAYZBRSERVER_NAMESPACE, "AdminRestartMatch", new Param2<int, int>(m_PendingConfirmStateIdx, m_PendingConfirmMatchId), true);
    }

    void AdminStatusUpdate(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        Param9<int, int, bool, int, int, int, int, float, int> data;
        if (!ctx.Read(data))
            return;

        if (type != CallType.Client)
            return;

        int phaseEnum = data.param1;
        int stateIdx = data.param2;
        bool frozen = data.param3;
        int timeLeft = data.param4;
        int alive = data.param5;
        int starting = data.param6;
        int zoneNum = data.param7;
        float zoneRadius = data.param8;
        int flagsAndTotal = data.param9;

        int availMask = flagsAndTotal & 0xFF;
        int totalRounds = (flagsAndTotal >> 8) & 0xFF;
        int matchId = (flagsAndTotal >> 16) & 0x7FFF;

        m_LastStateIdx = stateIdx;
        m_LastMatchId = matchId;

        m_IsFrozen = frozen;
        m_CanNext = (availMask & AVAIL_NEXT) != 0;
        m_CanFreeze = (availMask & AVAIL_FREEZE) != 0;
        m_CanAdjust = (availMask & AVAIL_ADJUST) != 0;
        m_CanForceStart = (availMask & AVAIL_FORCESTART) != 0;
        m_CanRestart = (availMask & AVAIL_RESTART) != 0;

        if (m_TxtPhase)
            m_TxtPhase.SetText(FormatPhaseLabel(phaseEnum, zoneNum, totalRounds));

        if (m_BadgeFrozen)
            m_BadgeFrozen.Show(frozen);

        if (m_TxtState)
        {
            if (frozen)
            {
                m_TxtState.SetText("#STR_BR_PANEL_FROZEN");
                m_TxtState.SetColor(ARGB(255, 62, 142, 227));
            }
            else
            {
                m_TxtState.SetText("#STR_BR_PANEL_RUNNING");
                m_TxtState.SetColor(ARGB(255, 255, 255, 255));
            }
        }

        if (m_TxtTimeLeft)
        {
            if (timeLeft >= 0)
                m_TxtTimeLeft.SetText(FormatTime(timeLeft));
            else
                m_TxtTimeLeft.SetText("--:--");
        }

        if (m_TxtAlive)
        {
            if (starting > 0)
                m_TxtAlive.SetText(alive.ToString() + " / " + starting.ToString());
            else
                m_TxtAlive.SetText(alive.ToString());
        }

        if (m_TxtZone)
        {
            if ((phaseEnum == EBRMatchPhase.ROUND || phaseEnum == EBRMatchPhase.LAST_ROUND) && zoneNum > 0)
            {
                //! No active circle yet (first round before its lock): the zone number alone
                int roundedRadius = Math.Round(zoneRadius);
                if (zoneRadius > 0.0 && roundedRadius < 1)
                    roundedRadius = 1;
                if (roundedRadius > 0)
                    m_TxtZone.SetText(string.Format(Widget.TranslateString("#STR_BR_PANEL_ZONE_FMT"), zoneNum.ToString(), roundedRadius.ToString()));
                else
                    m_TxtZone.SetText(string.Format(Widget.TranslateString("#STR_BR_PANEL_ZONE_NOCIRCLE_FMT"), zoneNum.ToString()));
            }
            else
            {
                m_TxtZone.SetText("-");
            }
        }

        if (m_LblFreeze)
        {
            if (frozen)
                m_LblFreeze.SetText("#STR_BR_PANEL_RESUME");
            else
                m_LblFreeze.SetText("#STR_BR_PANEL_FREEZE");
        }

        if (m_ImgFreeze)
        {
            if (frozen)
                m_ImgFreeze.LoadImageFile(0, "set:vpp_icons image:play");
            else
                m_ImgFreeze.LoadImageFile(0, "set:vpp_icons image:snowflake");
        }

        ApplyAvailability();
    }

    void AdminActionResult(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        Param3<bool, string, string> data;
        if (!ctx.Read(data))
            return;

        if (type != CallType.Client)
            return;

        bool ok = data.param1;
        string key = data.param2;
        string arg1 = data.param3;

        string msg = Widget.TranslateString("#" + key);
        if (arg1 != "")
            msg = string.Format(msg, arg1);

        if (m_TxtFeedback)
        {
            m_TxtFeedback.SetText(msg);
            if (ok)
                m_TxtFeedback.SetColor(ARGB(255, 160, 220, 160));
            else
                m_TxtFeedback.SetColor(ARGB(255, 231, 76, 60));
        }

        if (GetVPPUIManager())
            GetVPPUIManager().DisplayNotification(msg, Widget.TranslateString("#STR_BR_PANEL_TITLE"));
    }

    protected void ApplyAvailability()
    {
        SetButtonState(m_BtnNextPhase, m_CanNext);
        SetButtonState(m_BtnFreeze, m_CanFreeze);
        SetButtonState(m_BtnTimeMinus5, m_CanAdjust);
        SetButtonState(m_BtnTimeMinus1, m_CanAdjust);
        SetButtonState(m_BtnTimePlus1, m_CanAdjust);
        SetButtonState(m_BtnTimePlus5, m_CanAdjust);
        SetButtonState(m_BtnForceStart, m_CanForceStart);
        SetButtonState(m_BtnRestart, m_CanRestart);
    }

    protected void SetButtonState(ButtonWidget btn, bool enabled)
    {
        if (!btn)
            return;

        btn.Enable(enabled);
        if (enabled)
            btn.SetAlpha(1.0);
        else
            btn.SetAlpha(0.4);
    }

    protected string FormatPhaseLabel(int phaseEnum, int zoneNum, int totalRounds)
    {
        switch (phaseEnum)
        {
            case EBRMatchPhase.LOBBY:
                return Widget.TranslateString("#STR_BR_PHASE_LOBBY");
            case EBRMatchPhase.SPAWN_SELECTION:
                return Widget.TranslateString("#STR_BR_PHASE_SPAWN_SELECTION");
            case EBRMatchPhase.PREPARE:
                return Widget.TranslateString("#STR_BR_PHASE_PREPARING");
            case EBRMatchPhase.STARTING:
                return Widget.TranslateString("#STR_BR_PHASE_STARTING");
            case EBRMatchPhase.ROUND:
                if (zoneNum > 0 && totalRounds > 0)
                    return string.Format(Widget.TranslateString("#STR_BR_PANEL_PHASE_ROUND"), zoneNum.ToString(), totalRounds.ToString());
                return Widget.TranslateString("#STR_BR_PHASE_ZONE_CLOSING");
            case EBRMatchPhase.LAST_ROUND:
                return Widget.TranslateString("#STR_BR_PHASE_FINAL_ZONE");
            case EBRMatchPhase.FINISHED:
                return Widget.TranslateString("#STR_BR_PHASE_MATCH_OVER");
            case EBRMatchPhase.RESTARTING:
                return Widget.TranslateString("#STR_BR_PHASE_RESTARTING");
        }
        return "-";
    }

    protected string FormatTime(int value)
    {
        if (value < 0)
            return "--:--";

        int seconds = value % 60;
        string secStr = seconds.ToString();
        if (seconds < 10)
            secStr = "0" + secStr;

        int minutes = value / 60;
        string minStr = minutes.ToString();
        if (minutes < 10)
            minStr = "0" + minStr;

        return minStr + ":" + secStr;
    }
};
#endif
#endif
