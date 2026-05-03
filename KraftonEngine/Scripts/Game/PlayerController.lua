-- PlayerController.lua
-- Runner Pawn의 입력, 이동, 중력, 게임오버 처리를 담당한다.
--
-- 슬라이드처럼 컴포넌트 캐시와 Local transform 보정이 많이 필요한 기능은
-- Game.PlayerSlide 모듈로 분리해서 이 파일이 너무 커지지 않게 유지한다.

local Config = require("Game.Config")
local GameManager = require("Game.GameManager")
local PlayerSlide = require("Game.PlayerSlide")

DeclareProperties({
    { name = "ForwardSpeed", type = "float", default = Config.forward_speed },
    { name = "LaneWidth", type = "float", default = Config.lane_width },
    { name = "LaneChangeSpeed", type = "float", default = Config.lane_change_speed },
    { name = "Gravity", type = "float", default = Config.gravity },
    { name = "JumpPower", type = "float", default = Config.jump_power },
})

-- =========================================================
-- Runtime state
-- =========================================================

local forward_speed = property("ForwardSpeed", Config.forward_speed)
local lane_width = property("LaneWidth", Config.lane_width)
local lane_change_speed = property("LaneChangeSpeed", Config.lane_change_speed)
local gravity = property("Gravity", Config.gravity)
local jump_power = property("JumpPower", Config.jump_power)

local current_lane = 0
local target_lane = 0

local vertical_velocity = 0.0
local is_grounded = false
local current_ground_z = nil

local ground_probe_distance = 300.0
local ground_snap_distance = 8.0
local skin_width = 2.0

local is_dead = false
local initial_location = nil

-- PlayerSlide 모듈 인스턴스. Collision shape와 mesh slide 보정 상태를 소유한다.
local slide = nil

-- =========================================================
-- Utility
-- =========================================================

local function clamp(value, min_value, max_value)
    if value < min_value then
        return min_value
    end
    if value > max_value then
        return max_value
    end
    return value
end

local function abs(value)
    if value < 0.0 then
        return -value
    end
    return value
end

local function sign(value)
    if value < 0.0 then
        return -1.0
    end
    if value > 0.0 then
        return 1.0
    end
    return 0.0
end

local function round(value)
    if value >= 0.0 then
        return math.floor(value + 0.5)
    end
    return math.ceil(value - 0.5)
end

local function copy_vec(v)
    return vec3(v.x, v.y, v.z)
end

local function key_down(name)
    return GetKeyDown(name) or get_key_down(name)
end

local function key_held(name)
    return GetKey(name) or get_key(name)
end

-- =========================================================
-- Ground query helpers
-- =========================================================

local function get_player_half_height()
    local collision_shape = slide and slide:GetCollisionShape() or nil
    if collision_shape and collision_shape.GetShapeHalfHeight then
        local half_height = collision_shape:GetShapeHalfHeight()
        if half_height then
            return half_height
        end
    end

    return 90.0
end

local function update_ground_state(dt, allow_snap)
    local loc = obj:GetWorldLocation()
    local half_height = get_player_half_height()
    local ground = obj:FindGround(ground_probe_distance, skin_width)

    if ground and ground.hit then
        current_ground_z = ground.ground_z

        local desired_center_z = current_ground_z + half_height
        local distance_to_ground = loc.z - desired_center_z

        -- 떨어지는 중이고 바닥이 충분히 가까울 때만 snap한다.
        -- 점프 상승 중에는 위쪽 플랫폼에 강제로 붙지 않게 vertical_velocity를 확인한다.
        if allow_snap and vertical_velocity <= 0.0 and distance_to_ground <= ground_snap_distance then
            loc.z = desired_center_z
            obj:SetWorldLocation(loc)
            vertical_velocity = 0.0
            is_grounded = true
            return
        end
    else
        current_ground_z = nil
    end

    is_grounded = false
end

-- =========================================================
-- Movement input
-- =========================================================

local function move_lane(delta)
    if is_dead then
        return
    end

    target_lane = clamp(target_lane + delta, -1, 1)

    if delta < 0 then
        print("[TempleRun] Input: Move Left -> lane " .. tostring(target_lane))
    elseif delta > 0 then
        print("[TempleRun] Input: Move Right -> lane " .. tostring(target_lane))
    end
end

local function try_jump()
    if is_dead or not is_grounded then
        return
    end

    vertical_velocity = jump_power
    is_grounded = false
    print("[TempleRun] Input: Jump")
end

-- =========================================================
-- Gravity
-- =========================================================

local function apply_gravity(dt)
    -- 먼저 현재 위치에서 바닥을 확인한다. 이미 바닥 위라면 중력을 적용하지 않는다.
    update_ground_state(dt, true)

    if not is_grounded then
        vertical_velocity = vertical_velocity + gravity * dt

        local loc = obj:GetWorldLocation()
        loc.z = loc.z + vertical_velocity * dt
        obj:SetWorldLocation(loc)

        -- 중력으로 이동한 뒤 다시 검사해서 이번 프레임에 착지할 수 있게 한다.
        update_ground_state(dt, true)
    end
end

-- =========================================================
-- Slide input
-- =========================================================

local function slide_key_pressed()
    return key_down("S") or key_down("DOWN") or key_down("CTRL")
end

local function slide_key_held()
    return key_held("S") or key_held("DOWN") or key_held("CTRL")
end

-- =========================================================
-- Game state
-- =========================================================

local function reset_player()
    is_dead = false
    vertical_velocity = 0.0
    is_grounded = false
    current_ground_z = nil

    -- 재시작 중 슬라이드 상태가 남아 있으면 collision/mesh를 먼저 원복한다.
    if slide then
        slide:Restore()
    end

    if initial_location then
        local loc = copy_vec(initial_location)
        obj:SetWorldLocation(loc)
        current_lane = clamp(round(loc.y / lane_width), -1, 1)
        target_lane = current_lane
    else
        current_lane = 0
        target_lane = 0
    end

    -- 시작 지점의 Z를 임의로 고정하지 않는다.
    -- 실제 바닥이 있으면 ground query로 붙고, 없으면 다음 Tick부터 떨어진다.
    update_ground_state(0.0, true)
    GameManager.Restart()
    print("[TempleRun] Restart")
end

local function game_over(reason)
    if is_dead then
        return
    end

    is_dead = true
    vertical_velocity = 0.0
    if slide then
        slide:Restore()
    end
    GameManager.GameOver(reason or "Unknown")
end

-- =========================================================
-- Collision callbacks helpers
-- =========================================================

local function has_obstacle_tag(actor)
    if not actor or not actor:IsValid() then
        return false
    end

    if actor:HasTag("Obstacle") then
        return true
    end

    if actor.Name and string.find(actor.Name, "Obstacle") then
        actor.Tag = "Obstacle"
        return true
    end

    return false
end

-- =========================================================
-- Lifecycle
-- =========================================================

function BeginPlay()
    local loc = obj:GetWorldLocation()
    initial_location = copy_vec(loc)

    current_lane = clamp(round(loc.y / lane_width), -1, 1)
    target_lane = current_lane

    vertical_velocity = 0.0
    is_grounded = false
    current_ground_z = nil
    is_dead = false

    -- 런타임 tag는 obstacle collision callback에서 플레이어 판별용으로 쓴다.
    obj.Tag = "Player"

    -- Runner mesh/material은 C++ BeginPlay에서 먼저 적용된다.
    -- 그 뒤 여기서 LocalScale/LocalLocation을 읽어 slide 기준값으로 캐시한다.
    slide = PlayerSlide.new(obj)
    slide:Configure()

    GameManager.Start()
    update_ground_state(0.0, true)

    print("[TempleRun] Player BeginPlay")
end

function Tick(dt)
    if key_down("R") then
        reset_player()
        return
    end

    if is_dead then
        return
    end

    if key_down("A") or key_down("LEFT") then
        move_lane(-1)
    elseif key_down("D") or key_down("RIGHT") then
        move_lane(1)
    end

    if key_down("SPACE") then
        try_jump()
    end

    -- 슬라이드는 시간 기반이 아니라 키 hold 기반이다.
    -- 누른 순간 줄이고, 모든 slide key가 떼어진 프레임에 바로 복구한다.
    if slide_key_pressed() then
        if slide then
            slide:Begin()
        end
    elseif slide and slide:IsSliding() and not slide_key_held() then
        slide:End()
    end

    -- X+ 자동 전진과 Y 레인 이동을 먼저 계산하고, 최종 위치 반영 후 중력을 적용한다.
    local loc = obj:GetWorldLocation()
    loc.x = loc.x + forward_speed * dt

    local target_y = target_lane * lane_width
    local diff_y = target_y - loc.y
    if abs(diff_y) > 0.1 then
        local step = lane_change_speed * dt
        if abs(diff_y) <= step then
            loc.y = target_y
            current_lane = target_lane
        else
            loc.y = loc.y + sign(diff_y) * step
        end
    else
        loc.y = target_y
        current_lane = target_lane
    end

    obj:SetWorldLocation(loc)
    apply_gravity(dt)
    GameManager.Tick(dt, forward_speed * dt)
end

function EndPlay()
    if slide then
        slide:Restore()
    end
    print("[TempleRun] Player EndPlay")
end

-- =========================================================
-- Collision callbacks
-- =========================================================

function OnBeginOverlap(otherActor, otherComp, selfComp)
    if is_dead then
        return
    end

    if not otherActor or not otherActor:IsValid() then
        return
    end

    print("[TempleRun] BeginOverlap with " .. tostring(otherActor.Name))

    -- 에디터 저장 tag가 누락된 임시 장애물은 이름 fallback으로 보정한다.
    if has_obstacle_tag(otherActor) then
        game_over("Hit obstacle: " .. tostring(otherActor.Name))
    end
end

function OnEndOverlap(otherActor, otherComp, selfComp)
    if not otherActor or not otherActor:IsValid() then
        return
    end

    print("[TempleRun] EndOverlap with " .. tostring(otherActor.Name))
end

function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal)
    if is_dead then
        return
    end

    if not otherActor or not otherActor:IsValid() then
        return
    end

    print("[TempleRun] Hit with " .. tostring(otherActor.Name))

    if has_obstacle_tag(otherActor) then
        game_over("Hit obstacle: " .. tostring(otherActor.Name))
    end
end
