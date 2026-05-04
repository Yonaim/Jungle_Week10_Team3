local ItemBase = require("Game.Items.ItemBase")

-- EffectItem은 실제 effect system이 붙기 전까지는 ApplyEffect placeholder log만 남깁니다.
-- 나중에 SpeedBoost/Shield 같은 시스템이 생기면 ItemBase:ApplyEffect를 override하거나 이 파일에서 교체하면 됩니다.
DeclareProperties({
    { name = "RequiredInteractorTag", type = "string", default = "Player" },
    { name = "EffectName", type = "string", default = "SpeedBoost" },
    { name = "EffectDuration", type = "float", default = 5.0 },
    { name = "ScoreValue", type = "int", default = 0 },
})

local score_value = property("ScoreValue", 0)

local item = ItemBase.New({
    ScoreValue = score_value,
    RequiredInteractorTag = property("RequiredInteractorTag", "Player"),
    EffectName = property("EffectName", "SpeedBoost"),
    EffectDuration = property("EffectDuration", 5.0),
    Features = {
        PickupOnOverlap = true,
        ConsumeOnPickup = true,
        ScoreReward = score_value > 0,
        GameplayEffect = true,
        SingleUse = true,
        DebugLog = true,
    },
})

function OnBeginOverlap(otherActor, otherComp, selfComp)
    -- overlap event signature는 C++ UScriptComponent::OnShapeBeginOverlap과 맞춰 둡니다.
    item:OnBeginOverlap(otherActor, otherComp, selfComp)
end

function OnEndOverlap(otherActor, otherComp, selfComp)
end

function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal)
end
