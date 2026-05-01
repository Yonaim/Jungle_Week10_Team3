-- Lua Script Template --

function BeginPlay()
    print("[BeginPlay] " .. obj.UUID)
    obj:PrintLocation()
end

function Tick(dt)
    obj.Location = obj.Location + obj.Velocity * dt
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
    obj:PrintLocation()
end
