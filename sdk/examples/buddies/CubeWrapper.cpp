/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "CubeWrapper.h"
#include <sifteo.h>
#include "Config.h"
#include "assets.gen.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace Buddies {

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

///////////////////////////////////////////////////////////////////////////////////////////////////
// || Various convenience functions...
// \/ 
///////////////////////////////////////////////////////////////////////////////////////////////////

const Sifteo::AssetImage &getBgAsset(int buddyId)
{
    switch (buddyId)
    {
        default:
        case 0: return Bg1;
        case 1: return Bg2;
        case 2: return Bg3;
        case 3: return Bg4;
        case 4: return Bg5;
        case 5: return Bg6;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

const Sifteo::PinnedAssetImage &getPieceAsset(int buddyId)
{
    switch (buddyId)
    {
        default:
        case 0: return Parts1;
        case 1: return Parts2;
        case 2: return Parts3;
        case 3: return Parts4;
        case 4: return Parts5;
        case 5: return Parts6;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// ||
// \/ Static data
///////////////////////////////////////////////////////////////////////////////////////////////////

const Vec2 kPartPositions[NUM_SIDES] =
{
    Vec2(32, -8),
    Vec2(-8, 32),
    Vec2(32, 72),
    Vec2(72, 32),
};

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

CubeWrapper::CubeWrapper()
    : mCube()
    , mEnabled(false)
    , mBuddyId(0)
    , mPieces()
    , mPiecesSolution()
    , mPieceOffsets()
    , mPieceAnimT(0.0f)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::Reset()
{
    for (unsigned int i = 0; i < arraysize(mPieceOffsets); ++i)
    {
        mPieceOffsets[i] = 0;
    }
    
    mPieceAnimT = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::Update(float dt)
{
    mPieceAnimT += dt;
    while (mPieceAnimT > kPieceAnimPeriod)
    {
        mPieceAnimT -= kPieceAnimPeriod;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::DrawBuddy()
{
    ASSERT(IsEnabled());
    
    EnableBg0SprBg1Video();
    Video().BG0_drawAsset(Vec2(0, 0), getBgAsset(mBuddyId));
    
    if (IsHinting())
    {
        DrawPiece(mPiecesSolution[SIDE_TOP], SIDE_TOP);
        DrawPiece(mPiecesSolution[SIDE_LEFT], SIDE_LEFT);
        DrawPiece(mPiecesSolution[SIDE_BOTTOM], SIDE_BOTTOM);
        DrawPiece(mPiecesSolution[SIDE_RIGHT], SIDE_RIGHT);
    }
    else
    {
        DrawPiece(mPieces[SIDE_TOP], SIDE_TOP);
        DrawPiece(mPieces[SIDE_LEFT], SIDE_LEFT);
        DrawPiece(mPieces[SIDE_BOTTOM], SIDE_BOTTOM);
        DrawPiece(mPieces[SIDE_RIGHT], SIDE_RIGHT);
    }
}  

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::DrawShuffleUi(GameState shuffleState, float shuffleScoreTime)
{
    ASSERT(kGameMode == GAME_MODE_SHUFFLE);
    
    switch (shuffleState)
    {
        case GAME_STATE_SHUFFLE_SHAKE_TO_SCRAMBLE:
        {
            DrawBanner(mCube.id() == 0 ? ui_top_shakeshuffle_blue :  ui_top_shakeshuffle_orange);
            break;
        }
        case GAME_STATE_SHUFFLE_UNSCRAMBLE_THE_FACES:
        {
            DrawBanner(mCube.id() == 0 ? ui_top_unscramble_blue : ui_top_unscramble_orange);
            break;
        }
        case GAME_STATE_SHUFFLE_PLAY:
        {
            if (IsSolved())
            {
                DrawBanner(mCube.id() == 0 ? ui_top_facecomplete_blue : ui_top_facecomplete_orange);
            }
            break;
        }
        case GAME_STATE_SHUFFLE_SCORE:
        {
            if (mCube.id() == 0)
            {
                int minutes = int(shuffleScoreTime) / 60;
                int seconds = int(shuffleScoreTime - (minutes * 60.0f));
                
                DrawScoreBanner(ui_top_time_blue, minutes, seconds);
            }
            else
            {
                DrawBanner(ui_top_shakeshuffle_orange);
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::DrawTitleCard(const char *text)
{
    ASSERT(text != NULL);
    
    EnableBg0SprBg1Video();
    Video().BG0_drawAsset(Vec2(0, 0), TitleCard);
    
    for (int i = 0; i < (NUM_SIDES * 2); ++i)
    {
        Video().hideSprite(i);
    }
    
    BG1Helper bg1helper(mCube);
    bg1helper.DrawText(Vec2(1, 1), Font, text);
    bg1helper.Flush();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::DrawTextBanner(const char *text)
{
    ASSERT(text != NULL);
    
    BG1Helper bg1helper(mCube);
    bg1helper.DrawAsset(Vec2(0, 0), BannerEmpty);
    bg1helper.DrawText(Vec2(0, 0), Font, text);
    bg1helper.Flush();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::EnableBg0SprBg1Video()
{
    Video().set();
    Video().clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::ClearBg1()
{
    BG1Helper bg1helper(mCube);
    bg1helper.Flush();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool CubeWrapper::IsLoadingAssets()
{
    ASSERT(IsEnabled());
    
    return mCube.assetDone(GameAssets);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::LoadAssets()
{
    ASSERT(IsEnabled());
    
    mCube.loadAssets(GameAssets);
    
    VidMode_BG0_ROM rom(mCube.vbuf);
    rom.init();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::DrawLoadingAssets()
{
    ASSERT(IsEnabled());
    
    VidMode_BG0_ROM rom(mCube.vbuf);
    rom.BG0_progressBar(Vec2(0, 0), mCube.assetProgress(GameAssets, VidMode_BG0::LCD_width), 16);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool CubeWrapper::IsEnabled() const
{
    return mEnabled;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::Enable(Cube::ID cubeId, unsigned int buddyId)
{
    ASSERT(!IsEnabled());
    
    mCube.enable(cubeId);
    
    mEnabled = true;
    mBuddyId = buddyId;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::Disable()
{
    ASSERT(IsEnabled());
    
    mEnabled = false;
    mCube.disable();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

const Piece &CubeWrapper::GetPiece(unsigned int side) const
{
    ASSERT(side < arraysize(mPieces));
    
    return mPieces[side];
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::SetPiece(unsigned int side, const Piece &piece)
{
    ASSERT(side < arraysize(mPieces));
    
    mPieces[side] = piece;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::SetPieceSolution(unsigned int side, const Piece &piece)
{
    ASSERT(side < arraysize(mPiecesSolution));
    
    mPiecesSolution[side] = piece;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::SetPieceOffset(unsigned int side, int offset)
{
    ASSERT(side < arraysize(mPieceOffsets));
    
    mPieceOffsets[side] = offset;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool CubeWrapper::IsSolved() const
{
    ASSERT(IsEnabled());
    
    for (unsigned int i = 0; i < arraysize(mPiecesSolution); ++i)
    {
        if (mPiecesSolution[i].mMustSolve)
        {
            if (mPieces[i].mBuddy != mPiecesSolution[i].mBuddy ||
                mPieces[i].mPart != mPiecesSolution[i].mPart)
            {
                return false;
            }
        }
    }
    
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool CubeWrapper::IsHinting() const
{
    return IsTouching();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool CubeWrapper::IsTouching() const
{
    return mCube.touching();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

Sifteo::VidMode_BG0_SPR_BG1 CubeWrapper::Video()
{
    return VidMode_BG0_SPR_BG1(mCube.vbuf);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::DrawPiece(const Piece &piece, unsigned int side)
{
    ASSERT(piece.mPart >= 0 && piece.mPart < NUM_SIDES);
    
    int spriteLayer0 = side;
    int spriteLayer1 = side + NUM_SIDES;
    
    if (piece.mAttribute == Piece::ATTR_FIXED)
    {
        Video().setSpriteImage(spriteLayer0, AttributeFixed);
    }
    else
    {
        Video().hideSprite(spriteLayer0);
    }
    
    if (piece.mAttribute == Piece::ATTR_HIDDEN)
    {
        Video().setSpriteImage(spriteLayer1, AttributeHidden);
    }
    else
    {
        int rotation = side - piece.mPart;
        if (rotation < 0)
        {
            rotation += 4;
        }
        
        const Sifteo::PinnedAssetImage &asset = getPieceAsset(piece.mBuddy);
        unsigned int frame = (rotation * NUM_SIDES) + piece.mPart;
        
        ASSERT(frame < asset.frames);
        Video().setSpriteImage(spriteLayer1, asset, frame);
    }
    
    Vec2 point = kPartPositions[side];
    
    switch(side)
    {
        case SIDE_TOP:
        {
            point.y += mPieceOffsets[side];
            break;
        }
        case SIDE_LEFT:
        {
            point.x += mPieceOffsets[side];
            break;
        }
        case SIDE_BOTTOM:
        {
            point.y -= mPieceOffsets[side];
            break;
        }
        case SIDE_RIGHT:
        {
            point.x -= mPieceOffsets[side];
            break;
        }
    }
    
    ASSERT(kPieceAnimPeriod > 0.0f);
    float w = 2.0f * M_PI / kPieceAnimPeriod;
    float x = kPieceAnimX * cosf(w * mPieceAnimT);
    float y = kPieceAnimY * sinf(w * mPieceAnimT);
    
    point.x += int(x);
    point.y += int(y);
    
    Video().moveSprite(spriteLayer0, point);
    Video().moveSprite(spriteLayer1, point);
    
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::DrawBanner(const Sifteo::AssetImage &asset)
{
    BG1Helper bg1helper(mCube);
    bg1helper.DrawAsset(Vec2(0, 0), asset);
    bg1helper.Flush();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeWrapper::DrawScoreBanner(const Sifteo::AssetImage &asset, int minutes, int seconds)
{
    BG1Helper bg1helper(mCube);
    bg1helper.DrawAsset(Vec2(0, 0), asset); // Banner Background
    
    const AssetImage &font = mCube.id() == 0 ? FontScoreBlue : FontScoreOrange;
    
    int x = 11;
    int y = 0;
    bg1helper.DrawAsset(Vec2(x++, y), font, minutes / 10); // Mintues (10s)
    bg1helper.DrawAsset(Vec2(x++, y), font, minutes % 10); // Minutes ( 1s)
    bg1helper.DrawAsset(Vec2(x++, y), font, 10); // ":"
    bg1helper.DrawAsset(Vec2(x++, y), font, seconds / 10); // Seconds (10s)
    bg1helper.DrawAsset(Vec2(x++, y), font, seconds % 10); // Seconds ( 1s)
    
    bg1helper.Flush();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

}
