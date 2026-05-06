local GameManager = require("Game.GameManager")

-- ItemBase는 아이템 Lua 스크립트들이 공통으로 쓰는 pickup 처리 객체
-- OnPickedUp/GrantLogFragment/GrantCrashDump
local ItemBase = {}
ItemBase.__index = ItemBase

-- C++ EItemFeatureFlags와 같은 이름을 사용
local DefaultFeatures = {
    -- PickupOnOverlap은 플레이어와 닿자마자 먹는 아이템인지 여부입니다.
    PickupOnOverlap = true,
    -- Actor 생명주기는 C++ ItemActorBase가 담당합니다.
    ConsumeOnPickup = false,
    -- ScoreReward는 단순 점수 보상입니다. Log Fragment는 별도 규칙을 쓰므로 보통 false입니다.
    ScoreReward = false,
    -- LogFragmentReward는 logs/score/trace/coach approval을 한 번에 올리는 규칙입니다.
    LogFragmentReward = false,
    -- CrashDumpReward는 dumps를 올리고 3개째에 Critical Analysis를 발동하는 규칙입니다.
    CrashDumpReward = false,
    -- SingleUse는 같은 overlap이 여러 번 들어와도 한 번만 처리하게 합니다.
    SingleUse = true,
    -- DebugLog는 아이템 pickup 흐름을 콘솔로 확인할 때 켭니다.
    DebugLog = false,
}

local function merge_features(features)
    -- item script가 일부 feature만 넘겨도 나머지는 안전한 기본값을 유지
    local merged = {}
    for key, value in pairs(DefaultFeatures) do
        merged[key] = value
    end

    if features then
        for key, value in pairs(features) do
            merged[key] = value == true
        end
    end

    return merged
end

local function is_valid_actor(actor)
    -- is_valid_actor는 Lua proxy가 아직 살아 있는 actor인지 확인합니다.
    return actor and actor.IsValid and actor:IsValid()
end

function ItemBase.New(config)
    -- ItemBase는 기본 동작이 있는 Lua 객체
    -- LogItem.lua/CrashDumpItem.lua처럼 이 객체를 만들고 OnBeginOverlap만 위임하면 바로 pickup item
    config = config or {}

    local self = setmetatable({}, ItemBase)
    -- ScoreValue는 단순 점수 아이템 보상입니다.
    self.ScoreValue = config.ScoreValue or 0
    -- RequiredInteractorTag는 어떤 actor가 먹을 수 있는지 제한합니다. 기본은 Player입니다.
    self.RequiredInteractorTag = config.RequiredInteractorTag or "Player"
    -- RespawnDelay는 AutoRespawn 구현을 붙일 때 쓸 예비 값입니다.
    self.RespawnDelay = config.RespawnDelay or 0.0
    -- Cooldown은 키/클릭 상호작용 아이템에 쓸 예비 값입니다.
    self.Cooldown = config.Cooldown or 0.0
    -- bPicked는 SingleUse 아이템이 중복 처리되지 않도록 막는 내부 상태입니다.
    self.bPicked = false
    -- bEnabled는 아이템이 현재 pickup 가능한지 나타냅니다.
    self.bEnabled = config.bStartsEnabled ~= false
    -- Features는 이 아이템이 어떤 규칙을 쓸지 모아둔 플래그입니다.
    self.Features = merge_features(config.Features)
    return self
end

function ItemBase:Log(message)
    -- Log는 DebugLog feature가 켜진 아이템만 콘솔에 흐름을 남깁니다.
    if self:HasFeature("DebugLog") then
        print("[ItemBase] " .. tostring(message))
    end
end

function ItemBase:HasFeature(featureName)
    -- HasFeature는 feature 플래그를 안전하게 읽는 함수입니다.
    return self.Features and self.Features[featureName] == true
end

function ItemBase:SetFeatureEnabled(featureName, enabled)
    -- SetFeatureEnabled는 런타임에 아이템 규칙을 켜고 끄고 싶을 때 쓰는 함수입니다.
    self.Features = self.Features or merge_features(nil)
    self.Features[featureName] = enabled == true
end

function ItemBase:OnBeginOverlap(otherActor, otherComp, selfComp)
    -- overlap pickup은 입력에 의존하지 않음
    -- Title/Game input policy가 바뀌어도 item pickup은 collision event만으로 동작합니다.
    if not self.bEnabled or not self:HasFeature("PickupOnOverlap") then
        return
    end

    if self:HasFeature("SingleUse") and self.bPicked then
        -- overlap pair가 여러 프레임 유지되거나 begin이 중복 호출되어도 한 번만 처리
        return
    end

    if not self:IsValidInteractor(otherActor, otherComp, selfComp) then
        return
    end

    self.bPicked = true
    self:Interact(otherActor, otherComp, selfComp)
end

function ItemBase:IsValidInteractor(otherActor, otherComp, selfComp)
    -- 기본은 Player tag만 반응
    -- RequiredInteractorTag를 빈 문자열로 두면 모든 actor와 상호작용 가능
    if not is_valid_actor(otherActor) then
        return false
    end

    if self.RequiredInteractorTag == nil or self.RequiredInteractorTag == "" then
        return true
    end

    return otherActor.HasTag and otherActor:HasTag(self.RequiredInteractorTag)
end

function ItemBase:Interact(otherActor, otherComp, selfComp)
    -- 공통 feature 순서
    -- Lua는 보상만 처리하고 Actor 생명주기는 C++에 맡깁니다.
    if self:HasFeature("ScoreReward") then
        self:GrantScore(otherActor)
    end

    if self:HasFeature("LogFragmentReward") then
        self:GrantLogFragment(otherActor)
    end

    if self:HasFeature("CrashDumpReward") then
        self:GrantCrashDump(otherActor)
    end

    self:OnPickedUp(otherActor, otherComp, selfComp)

    if self:HasFeature("ConsumeOnPickup") then
        self:ConsumeIfNeeded()
    end
end

function ItemBase:GrantScore(otherActor)
    -- GrantScore는 단순 점수 보상입니다. Log Fragment 규칙은 GrantLogFragment를 씁니다.
    GameManager.AddScore(self.ScoreValue, "ItemPickup")
    self:Log("GrantScore amount=" .. tostring(self.ScoreValue))
end

function ItemBase:GrantLogFragment(otherActor)
    -- logs +1, score +10, trace +2, coach approval 상승을 GameManager가 처리합니다.
    -- trace 100%가 되면 여기서 Hotfix가 발동되고, 화면에 HOTFIX APPLIED 띄우면 됨.
    GameManager.CollectLogFragment()
    self:Log("GrantLogFragment actor=" .. tostring(otherActor and otherActor.Name))
end

function ItemBase:GrantCrashDump(otherActor)
    -- dumps가 3개가 되면 Critical Analysis가 발동되고 score/shield/coach approval이 올라갑니다.
    -- TODO: 여기서 화면에 CRITICAL ANALYSIS 연출 붙이면 됨.
    local result = GameManager.CollectCrashDump()
    self:Log(
        "GrantCrashDump actor=" .. tostring(otherActor and otherActor.Name) ..
        " critical=" .. tostring(result and result.critical_analysis_applied)
    )
end

function ItemBase:OnPickedUp(otherActor, otherComp, selfComp)
    -- item별 Lua script가 override해서 추가 보상을 넣는 지점입니다.
end

function ItemBase:ConsumeIfNeeded()
    -- Actor 제거는 C++ ItemActorBase가 담당한다.
    -- Lua는 보상 처리만 수행한다.
    self:Log("Consume 요청됨. Actor 생명주기는 C++에서 처리한다.")
end

return ItemBase
