-- VSCode/LuaLS는 같은 Scripts 폴더의 EngineAPI.lua를 workspace library로 인식하면
-- require 없이 obj, vec3, property 같은 전역 API 자동완성을 제공한다.

DeclareProperties({
    -- Details UI에 노출되는 값들이다. 사용자가 바꾸지 않으면 default가 그대로 사용된다.
    Speed = { type = "float", default = 600.0, min = 0.0, max = 3000.0 },
    JumpPower = { type = "float", default = 900.0 },
    bEnabled = { type = "bool", default = true },
    Label = { type = "string", default = "Player" },
    Offset = { type = "vector", default = vec3(0, 0, 0) },
})

-- 여기서 변경
--[[local speed = 600.0
local enabled = true
local offset = vec3(0, 0, 0)]]--

function BeginPlay()
    print("[BeginPlay] " .. obj.UUID)
end

function Tick(dt)
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
end

function OnBeginOverlap(otherActor, otherComp, selfComp)
end

function OnEndOverlap(otherActor, otherComp, selfComp)
end

function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal)
end

-- ComponentProxy 사용 예시:
-- score = 0
-- runner = nil
-- text = nil
--
-- function BeginPlay()
--     runner = obj:GetComponentByType("InterpToMovementComponent")
--     text = obj:GetComponentByType("TextRenderComponent")
--
--     if runner and runner:IsValid() then
--         runner:SetSpeed(900)
--     end
--
--     if text and text:IsValid() then
--         text:SetText("Score: 0")
--     end
-- end
--
-- function Tick(dt)
--     if runner and runner:IsValid() then
--         if GetKeyDown("A") then
--             runner:MoveBy(vec3(0, -100, 0))
--         elseif GetKeyDown("D") then
--             runner:MoveBy(vec3(0, 100, 0))
--         end
--     end
-- end
--
-- function OnBeginOverlap(otherActor, otherComp, selfComp)
--     if not otherActor or not otherActor:IsValid() then
--         return
--     end
--
--     if otherActor.Name == "Coin" then
--         score = score + 10
--         otherActor:Destroy()
--
--         if text and text:IsValid() then
--             text:SetText("Score: " .. score)
--         end
--     elseif otherActor.Name == "Obstacle" then
--         print("Game Over")
--     end
-- end
