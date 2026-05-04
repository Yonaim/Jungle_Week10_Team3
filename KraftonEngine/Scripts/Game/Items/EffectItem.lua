local ItemBase = require("Game.Items.ItemBase")

-- EffectItem은 실제 effect system이 붙기 전까지는 ApplyEffect placeholder log만 남깁니다.
-- 나중에 SpeedBoost/Shield 같은 시스템이 생기면 ItemBase:ApplyEffect를 override하거나 이 파일에서 교체하면 됩니다.
DeclareProperties({
    -- RequiredInteractorTag는 어떤 actor가 이 effect item을 먹을 수 있는지 정합니다.
    { name = "RequiredInteractorTag", type = "string", default = "Player" },
    -- EffectName은 나중에 실제 gameplay/visual effect 이름으로 연결하면 됩니다.
    { name = "EffectName", type = "string", default = "SpeedBoost" },
    -- EffectDuration은 effect가 유지될 시간입니다. 아직은 placeholder 로그에만 씁니다.
    { name = "EffectDuration", type = "float", default = 5.0 },
    -- ScoreValue는 effect item에 점수 보상을 같이 줄 때 쓰는 값입니다.
    { name = "ScoreValue", type = "int", default = 0 },
})

-- score_value는 property를 한 번만 읽어 feature 설정과 ItemBase config에 같이 쓰는 값입니다.
local score_value = property("ScoreValue", 0)

-- item은 EffectItem의 공통 pickup/효과 placeholder 처리 객체입니다.
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
    -- 지금은 끝 overlap에서 할 일이 없습니다. 나중에 근접 effect preview를 끌 때 쓰면 됨.
end

function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal)
    -- 지금은 Hit가 아니라 BeginOverlap으로만 먹습니다.
end
