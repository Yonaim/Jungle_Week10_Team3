local ItemBase = require("Game.Items.ItemBase")

-- CoinItem은 ItemBase를 그대로 사용하되, 점수 보상 feature만 켠 가장 단순한 예시입니다.
-- ScriptComponent에 이 파일을 지정하면 Player tag actor와 overlap할 때 한 번만 점수를 주고 자기 자신을 제거합니다.
DeclareProperties({
    { name = "ScoreValue", type = "int", default = 10 },
    { name = "RequiredInteractorTag", type = "string", default = "Player" },
})

local item = ItemBase.New({
    ScoreValue = property("ScoreValue", 10),
    RequiredInteractorTag = property("RequiredInteractorTag", "Player"),
    Features = {
        PickupOnOverlap = true,
        ConsumeOnPickup = true,
        ScoreReward = true,
        SingleUse = true,
        DebugLog = true,
    },
})

function OnBeginOverlap(otherActor, otherComp, selfComp)
    -- ScriptComponent가 C++ overlap event를 이 전역 함수로 전달합니다.
    -- 실제 판정/보상/소멸 순서는 ItemBase가 처리합니다.
    item:OnBeginOverlap(otherActor, otherComp, selfComp)
end

function OnEndOverlap(otherActor, otherComp, selfComp)
end

function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal)
end
