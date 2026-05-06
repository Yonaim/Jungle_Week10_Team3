-- PlayerSlide.lua
-- Runner 슬라이드 상태 기계.
--
-- PlayerController.lua는 입력, 이동, 라이프사이클만 담당하게 두고,
-- 슬라이드 중 바뀌는 collision shape / mesh transform은 이 모듈이 관리한다.

local PlayerSlide = {}
PlayerSlide.__index = PlayerSlide

local function is_valid_component(component)
    return component and component:IsValid()
end

local function find_collision_shape(actor)
    -- 현재 Runner는 UBoxComponent가 root collision이다.
    -- 그래도 shape 공통 API를 쓰도록 sphere/capsule까지 순서대로 찾는다.
    local shape = actor:GetComponentByType("UBoxComponent")
    if is_valid_component(shape) then
        return shape
    end

    shape = actor:GetComponentByType("USphereComponent")
    if is_valid_component(shape) then
        return shape
    end

    shape = actor:GetComponentByType("UCapsuleComponent")
    if is_valid_component(shape) then
        return shape
    end

    return nil
end

function PlayerSlide.new(actor)
    -- PlayerSlide 객체는 한 Runner actor의 슬라이드 중 collision/mesh 변경값을 기억합니다.
    -- PlayerController가 입력을 판단하고, 실제 모양 변경은 여기 Begin/End만 호출하면 됩니다.
    return setmetatable({
        -- actor는 슬라이드를 적용할 Runner ActorProxy입니다.
        actor = actor,
        -- is_sliding은 현재 슬라이드 중인지 나타냅니다.
        is_sliding = false,

        -- collision_shape는 슬라이드 때 줄일 박스/캡슐/구 shape component입니다.
        collision_shape = nil,
        -- normal_shape_extent는 평상시 collision 크기입니다.
        normal_shape_extent = nil,
        -- slide_shape_extent는 슬라이드 중 collision 크기입니다.
        slide_shape_extent = nil,
        -- slide_shape_z_delta는 collision을 줄인 만큼 바닥에 붙여두기 위한 Z 보정값입니다.
        slide_shape_z_delta = 0.0,

        -- mesh는 슬라이드 중 시각 높이를 줄일 StaticMeshComponent입니다.
        mesh = nil,
        -- normal_mesh_local_scale은 평상시 mesh scale입니다.
        normal_mesh_local_scale = nil,
        -- slide_mesh_local_scale은 슬라이드 중 mesh scale입니다.
        slide_mesh_local_scale = nil,
        -- normal_mesh_local_location은 평상시 mesh local 위치입니다.
        normal_mesh_local_location = nil,
        -- slide_mesh_local_location은 슬라이드 중 mesh local 위치입니다.
        slide_mesh_local_location = nil
    }, PlayerSlide)
end

function PlayerSlide:Configure()
    -- Configure는 BeginPlay에서 한 번 호출해 평상시/슬라이드 중 크기 값을 준비합니다.
    -- 나중에 슬라이드 VFX나 애니메이션을 붙일 때도 이 초기화 뒤에 연결하면 됩니다.
    self.collision_shape = find_collision_shape(self.actor)
    if is_valid_component(self.collision_shape) then
        self.collision_shape:SetCollisionEnabled(true)
        self.collision_shape:SetGenerateOverlapEvents(true)

        self.normal_shape_extent = self.collision_shape:GetShapeExtent()
        if self.normal_shape_extent then
            self.slide_shape_extent = vec3(
                self.normal_shape_extent.x,
                self.normal_shape_extent.y,
                self.normal_shape_extent.z * 0.5
            )

            -- ShapeExtent.z는 box/capsule 계열에서 half-height로 본다.
            -- 슬라이드 때 줄어든 half-height만큼 아래로 내려야 바닥면이 그대로 유지된다.
            self.slide_shape_z_delta = self.normal_shape_extent.z - self.slide_shape_extent.z
        end

        print(
            "[TempleRun] Collision configured. Shape=" ..
            tostring(self.collision_shape:GetShapeType()) ..
            ", NormalExtent=(" ..
            tostring(self.normal_shape_extent and self.normal_shape_extent.x or 0.0) .. ", " ..
            tostring(self.normal_shape_extent and self.normal_shape_extent.y or 0.0) .. ", " ..
            tostring(self.normal_shape_extent and self.normal_shape_extent.z or 0.0) .. "), ShapeZDelta=" ..
            tostring(self.slide_shape_z_delta)
        )
    else
        warn("[TempleRun] Collision shape not found. Slide collision resize and ground height fallback may be inaccurate.")
    end

    self.mesh = self.actor:GetStaticMeshComponent()
    if is_valid_component(self.mesh) then
        self.normal_mesh_local_scale = self.mesh:GetLocalScale()
        self.normal_mesh_local_location = self.mesh:GetLocalLocation()

        self.slide_mesh_local_scale = vec3(
            self.normal_mesh_local_scale.x,
            self.normal_mesh_local_scale.y,
            self.normal_mesh_local_scale.z * 0.5
        )

        -- Static mesh scale은 pivot/center 기준으로 줄어든다.
        -- 그래서 시각 높이 감소분의 절반만큼 local Z를 내려야 mesh가 바닥에서 뜨지 않는다.
        self.slide_mesh_local_location = vec3(
            self.normal_mesh_local_location.x,
            self.normal_mesh_local_location.y,
            self.normal_mesh_local_location.z
        )

        print(
            "[TempleRun] Mesh slide configured. NormalScaleZ=" ..
            tostring(self.normal_mesh_local_scale.z) ..
            ", SlideScaleZ=" ..
            tostring(self.slide_mesh_local_scale.z)
        )
    else
        warn("[TempleRun] StaticMeshComponent not found. Slide visual scale will not work.")
    end
end

function PlayerSlide:GetCollisionShape()
    -- PlayerController의 바닥 높이 계산이 현재 collision half-height를 읽을 때 쓰는 getter입니다.
    if is_valid_component(self.collision_shape) then
        return self.collision_shape
    end

    return nil
end

function PlayerSlide:IsSliding()
    -- 슬라이드 중 Begin이 반복 호출되지 않게 PlayerController가 이 함수로 상태를 확인합니다.
    return self.is_sliding
end

function PlayerSlide:Begin()
    -- 슬라이드는 duration 기반 타이머가 아니라 누르고 있는 동안 유지하는 방식입니다.
    -- 이 함수는 슬라이드 시작 1회만 호출되어야 하며, 반복 호출은 바로 무시합니다.
    if self.is_sliding then
        return
    end

    self.is_sliding = true

    if is_valid_component(self.collision_shape) and self.slide_shape_extent then
        self.collision_shape:SetShapeExtent(self.slide_shape_extent)

        -- Runner의 collision box는 현재 root component다.
        -- 저장해 둔 local location으로 되돌리면 달리던 actor의 X/Y까지 예전 위치로 튈 수 있다.
        -- 그래서 현재 위치 기준 AddLocalOffset만 사용한다.
        if self.slide_shape_z_delta ~= 0.0 then
            self.collision_shape:AddLocalOffset(vec3(0.0, 0.0, -self.slide_shape_z_delta))
        end

        print(
            "[TempleRun] Slide collision extent=(" ..
            tostring(self.slide_shape_extent.x) .. ", " ..
            tostring(self.slide_shape_extent.y) .. ", " ..
            tostring(self.slide_shape_extent.z) .. "), ShapeZDelta=" ..
            tostring(self.slide_shape_z_delta)
        )
    end

    if is_valid_component(self.mesh) then
        if self.slide_mesh_local_scale then
            self.mesh:SetLocalScale(self.slide_mesh_local_scale)
        end
        if self.slide_mesh_local_location then
            self.mesh:SetLocalLocation(self.slide_mesh_local_location)
        end
    end

    print("[TempleRun] Input: Slide")
end

function PlayerSlide:End()
    -- End는 키를 떼거나 공중으로 뜬 순간 호출됩니다.
    -- 실제 복구는 Restore가 담당해서 EndPlay에서도 같은 복구 로직을 재사용합니다.
    if not self.is_sliding then
        return
    end

    self:Restore()
    print("[TempleRun] Slide end")
end

function PlayerSlide:Restore()
    -- Restore는 collision/mesh를 평상시 값으로 되돌립니다.
    -- GameOver/EndPlay에서도 호출되므로 nil 체크를 충분히 유지합니다.
    local was_sliding = self.is_sliding

    if is_valid_component(self.collision_shape) and self.normal_shape_extent then
        self.collision_shape:SetShapeExtent(self.normal_shape_extent)
        if was_sliding and self.slide_shape_z_delta ~= 0.0 then
            self.collision_shape:AddLocalOffset(vec3(0.0, 0.0, self.slide_shape_z_delta))
        end
    end

    if is_valid_component(self.mesh) then
        if self.normal_mesh_local_scale then
            self.mesh:SetLocalScale(self.normal_mesh_local_scale)
        end
        if self.normal_mesh_local_location then
            self.mesh:SetLocalLocation(self.normal_mesh_local_location)
        end
    end

    self.is_sliding = false
end

return PlayerSlide
