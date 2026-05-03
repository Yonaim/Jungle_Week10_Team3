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
    print("[Tick]" .. obj.UUID)
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
end
