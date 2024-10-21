#include <cpu/guest_code.h>
#include "api/SWA.h"
#include "ui/window.h"
#include "config.h"

constexpr float m_baseAspectRatio = 16.0f / 9.0f;

const char* m_pStageID;

uint32_t m_lastCheckpointScore = 0;

float m_lastDarkGaiaEnergy = 0.0f;

bool m_isUnleashCancelled = false;

#pragma region Aspect Ratio Hooks

bool CameraAspectRatioMidAsmHook(PPCRegister& r31)
{
    auto pCamera = (SWA::CCamera*)g_memory.Translate(r31.u32);
    auto newAspectRatio = (float)Window::s_width / (float)Window::s_height;

    // Dynamically adjust horizontal aspect ratio to window dimensions.
    pCamera->m_HorzAspectRatio = newAspectRatio;

    if (auto s_pVertAspectRatio = (be<float>*)g_memory.Translate(0x82028FE0))
    {
        // Dynamically adjust vertical aspect ratio for VERT+.
        *s_pVertAspectRatio = 2.0f * atan(tan(45.0f / 2.0f) * (m_baseAspectRatio / newAspectRatio));
    }

    // Jump to 4:3 code for VERT+ adjustments if using a narrow aspect ratio.
    return newAspectRatio < m_baseAspectRatio;
}

void CameraBoostAspectRatioMidAsmHook(PPCRegister& r31, PPCRegister& f0)
{
    auto pCamera = (SWA::CCamera*)g_memory.Translate(r31.u32);

    if (Window::s_width < Window::s_height)
    {
        // Use horizontal FOV for narrow aspect ratios.
        f0.f32 = pCamera->m_HorzFieldOfView;
    }
    else
    {
        // Use vertical FOV for wide aspect ratios.
        f0.f32 = pCamera->m_VertFieldOfView;
    }
}

void CSDAspectRatioMidAsmHook(PPCRegister& f1, PPCRegister& f2)
{
    auto newAspectRatio = (float)Window::s_width / (float)Window::s_height;

    if (newAspectRatio > m_baseAspectRatio)
    {
        f1.f64 = 1280.0f / ((newAspectRatio * 720.0f) / 1280.0f);
    }
    else if (newAspectRatio < m_baseAspectRatio)
    {
        f2.f64 = 720.0f / ((1280.0f / newAspectRatio) / 720.0f);
    }
}

#pragma endregion

#pragma region Score Hooks

/* Hook function for when checkpoints are activated
   to preserve the current checkpoint score. */
PPC_FUNC_IMPL(__imp__sub_82624308);
PPC_FUNC(sub_82624308)
{
    __imp__sub_82624308(ctx, base);

    if (Config::ScoreBehaviour != EScoreBehaviour::CheckpointRetain)
        return;

    auto pGameDocument = SWA::CGameDocument::GetInstance();

    if (!pGameDocument)
        return;

    m_lastCheckpointScore = pGameDocument->m_pMember->m_Score;
}

/* Hook function that resets the score
   and restore the last checkpoint score. */
PPC_FUNC_IMPL(__imp__sub_8245F048);
PPC_FUNC(sub_8245F048)
{
    __imp__sub_8245F048(ctx, base);

    if (Config::ScoreBehaviour != EScoreBehaviour::CheckpointRetain)
        return;

    auto pGameDocument = SWA::CGameDocument::GetInstance();

    if (!pGameDocument)
        return;

    printf("Resetting score to %d\n", m_lastCheckpointScore);

    pGameDocument->m_pMember->m_Score = m_lastCheckpointScore;
}

void ResetScoreOnRestartMidAsmHook()
{
    m_lastCheckpointScore = 0;
}

#pragma endregion

#pragma region Loading Hooks

// Loading screen hook.
PPC_FUNC_IMPL(__imp__sub_824DCF38);
PPC_FUNC(sub_824DCF38)
{
    /* Force the Werehog transition ID
       to use a different transition. */
    if (!Config::WerehogHubTransformVideo)
    {
        /*
            0 - Tails Electric NOW LOADING
            1 - No Transition
            2 - Werehog Transition
            3 - Tails Electric NOW LOADING w/ Info (requires context)
            4 - Arrows In/Out
            5 - NOW LOADING
            6 - Event Gallery
            7 - NOW LOADING
            8 - Black Screen
        */
        if (ctx.r4.u32 == 2)
            ctx.r4.u32 = 4;
    }

    if (m_pStageID)
    {
        /* Fix restarting Eggmanland as the Werehog
           erroneously using the Event Gallery transition. */
        if (ctx.r4.u32 == 6 && !strcmp(m_pStageID, "Act_EggmanLand"))
            ctx.r4.u32 = 5;
    }

    __imp__sub_824DCF38(ctx, base);
}

#pragma endregion

#pragma region Player Hooks

// Dark Gaia energy change hook.
PPC_FUNC_IMPL(__imp__sub_823AF7A8);
PPC_FUNC(sub_823AF7A8)
{
    auto pEvilSonicContext = (SWA::Player::CEvilSonicContext*)g_memory.Translate(ctx.r3.u32);

    m_lastDarkGaiaEnergy = pEvilSonicContext->m_DarkGaiaEnergy;

    // Don't drain energy if out of control.
    if (!Config::UnleashOutOfControlDrain && pEvilSonicContext->m_OutOfControlCount && ctx.f1.f64 < 0.0)
        return;
    
    __imp__sub_823AF7A8(ctx, base);

    if (!Config::UnleashCancel)
        return;

    auto pInputState = SWA::CInputState::GetInstance();

    // Don't allow cancelling Unleash if the intro anim is still playing.
    if (!pInputState || pEvilSonicContext->m_AnimationID == 39)
        return;

    if (pInputState->GetPadState().IsTapped(SWA::eKeyState_RightBumper))
    {
        pEvilSonicContext->m_DarkGaiaEnergy = 0.0f;
        m_isUnleashCancelled = true;
    }
}

void PostUnleashMidAsmHook(PPCRegister& r30)
{
    if (m_isUnleashCancelled)
    {
        if (auto pEvilSonicContext = (SWA::Player::CEvilSonicContext*)g_memory.Translate(r30.u32))
            pEvilSonicContext->m_DarkGaiaEnergy = m_lastDarkGaiaEnergy;

        m_isUnleashCancelled = false;
    }
}

#pragma endregion

#pragma region Miscellaneous Hooks

bool DisableHintsMidAsmHook()
{
    return !Config::Hints;
}

void SetXButtonHomingMidAsmHook(PPCRegister& r30)
{
    r30.u32 = Config::XButtonHoming;
}

/* Hook function that gets the game region
   and force result to zero for Japanese
   to display the correct logos. */
PPC_FUNC_IMPL(__imp__sub_825197C0);
PPC_FUNC(sub_825197C0)
{
    if (Config::Language == ELanguage::Japanese)
    {
        ctx.r3.u64 = 0;
        return;
    }

    __imp__sub_825197C0(ctx, base);
}

void HighFrameRateDeltaTimeFixMidAsmHook(PPCRegister& f1)
{
    // Having 60 FPS threshold ensures we still retain
    // the original game behavior when locked to 30/60 FPS.
    constexpr double threshold = 1.0 / 60.0;

    if (f1.f64 < threshold)
        f1.f64 = threshold;
}

void GetStageIDMidAsmHook(PPCRegister& r5)
{
    m_pStageID = *(xpointer<const char>*)g_memory.Translate(r5.u32);
}

// Logo skip
PPC_FUNC_IMPL(__imp__sub_82547DF0);
PPC_FUNC(sub_82547DF0)
{
    if (Config::LogoSkip)
    {
        ctx.r4.u64 = 0;
        ctx.r5.u64 = 0;
        ctx.r6.u64 = 1;
        ctx.r7.u64 = 0;
        sub_825517C8(ctx, base);
    }
    else
    {
        __imp__sub_82547DF0(ctx, base);
    }
}

#pragma endregion
