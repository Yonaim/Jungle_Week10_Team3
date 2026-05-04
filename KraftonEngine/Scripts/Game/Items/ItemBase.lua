local GameManager = require("Game.GameManager")

local ItemBase = {}
ItemBase.__index = ItemBase

-- C++ EItemFeatureFlags와 같은 이름을 사용
-- C++ actor config와 Lua script config 호환성 고려
local DefaultFeatures = {
    PickupOnOverlap = true,
    ConsumeOnPickup = true,
    ScoreReward = false,
    GameplayEffect = false,
    SingleUse = true,
    AutoRespawn = false,
    FloatingMotion = false,
    RotatingMotion = false,
    DebugLog = false,
    InteractOnClick = false,
    InteractOnKey = false,
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
    return actor and actor.IsValid and actor:IsValid()
end

function ItemBase.New(config)
    -- ItemBase는 기본 동작이 있는 Lua 객체
    -- CoinItem.lua처럼 이 객체를 만들고 OnBeginOverlap만 위임하면 바로 pickup item
    config = config or {}

    local self = setmetatable({}, ItemBase)
    self.ScoreValue = config.ScoreValue or 0
    self.RequiredInteractorTag = config.RequiredInteractorTag or "Player"
    self.EffectName = config.EffectName or ""
    self.EffectDuration = config.EffectDuration or 0.0
    self.RespawnDelay = config.RespawnDelay or 0.0
    self.Cooldown = config.Cooldown or 0.0
    self.bPicked = false
    self.bEnabled = config.bStartsEnabled ~= false
    self.Features = merge_features(config.Features)
    return self
end

function ItemBase:Log(message)
    if self:HasFeature("DebugLog") then
        print("[ItemBase] " .. tostring(message))
    end
end

function ItemBase:HasFeature(featureName)
    return self.Features and self.Features[featureName] == true
end

function ItemBase:SetFeatureEnabled(featureName, enabled)
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
    -- 보상/효과를 먼저 적용하고, override hook을 호출한 뒤 필요하면 actor를 제거
    if self:HasFeature("ScoreReward") then
        self:GrantScore(otherActor)
    end

    if self:HasFeature("GameplayEffect") then
        self:ApplyEffect(otherActor)
    end

    self:OnPickedUp(otherActor, otherComp, selfComp)

    if self:HasFeature("ConsumeOnPickup") then
        self:ConsumeIfNeeded()
    end
end

function ItemBase:GrantScore(otherActor)
    GameManager.AddScore(self.ScoreValue, "ItemPickup")
    self:Log("GrantScore amount=" .. tostring(self.ScoreValue))
end

function ItemBase:ApplyEffect(otherActor)
    -- 추후 PlayerStatus.ApplyEffect 같은 API가 생기면 여기서 연결
    self:Log(
        "ApplyEffect placeholder effect=" .. tostring(self.EffectName) ..
        " duration=" .. tostring(self.EffectDuration) ..
        " actor=" .. tostring(otherActor and otherActor.Name)
    )
end

function ItemBase:OnPickedUp(otherActor, otherComp, selfComp)
    -- item별 Lua script가 override해서 사운드, VFX, 추가 보상 등을 넣는 지점
end

function ItemBase:ConsumeIfNeeded()
    -- ConsumeOnPickup이 켜진 item은 pickup 후 owner actor를 제거
    -- AutoRespawn item을 구현할 때는 이 함수를 override해서 숨김/타이머 방식으로 변경
    if obj and obj.Destroy then
        self:Log("Destroy item actor")
        obj:Destroy()
    end
end

return ItemBase
