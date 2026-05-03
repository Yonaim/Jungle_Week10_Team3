---@meta KraftonEngine

-- VSCode/LuaLS 자동완성 전용 stub입니다.
-- 런타임에서 require하지 않습니다. 실제 구현은 C++ LuaScriptRuntime/LuaScriptInstance 바인딩에 있습니다.

---@class Vector
---@field x number
---@field y number
---@field z number
---@operator add(Vector): Vector
---@operator sub(Vector): Vector
---@operator mul(number): Vector
---@operator div(number): Vector
local Vector = {}

---@param x number
---@param y number
---@param z number
---@return Vector
function Vector.new(x, y, z) end

---@param x number
---@param y number
---@param z number
---@return Vector
function vec3(x, y, z) end

---@param x number
---@param y number
---@param z number
---@return Vector
function Vector3(x, y, z) end

---@class ActorProxy
---@field Name string
---@field UUID integer
---@field Tag string 임시 프로토타입용 태그입니다. 장기적으로 정식 식별 방식으로 교체해야 합니다.
---@field Location Vector
---@field Rotation Vector
---@field Scale Vector
---@field Velocity Vector
local ActorProxy = {}

---@return boolean
function ActorProxy:IsValid() end

---@param tag string
---@return boolean
function ActorProxy:HasTag(tag) end

---@param componentName string
---@return ComponentProxy
function ActorProxy:GetComponent(componentName) end

---@param typeName string
---@return ComponentProxy
function ActorProxy:GetComponentByType(typeName) end

---@return ComponentProxy
function ActorProxy:GetStaticMeshComponent() end

---@return ComponentProxy
function ActorProxy:GetScriptComponent() end

---@param delta Vector
function ActorProxy:AddWorldOffset(delta) end

---@param x number
---@param y number
---@param z number
function ActorProxy:AddWorldOffset(x, y, z) end

---@param delta Vector
function ActorProxy:Translate(delta) end

---@param x number
---@param y number
---@param z number
function ActorProxy:Translate(x, y, z) end

---@param location Vector
function ActorProxy:MoveTo(location) end

---@param x number
---@param y number
---@param z? number
function ActorProxy:MoveTo(x, y, z) end

---@param delta Vector
function ActorProxy:MoveBy(delta) end

---@param x number
---@param y number
---@param z? number
function ActorProxy:MoveBy(x, y, z) end

---@param target ActorProxy
function ActorProxy:MoveToActor(target) end

function ActorProxy:StopMove() end

---@return boolean
function ActorProxy:IsMoveDone() end

---@param speed number
function ActorProxy:SetMoveSpeed(speed) end

---@return number
function ActorProxy:GetMoveSpeed() end

function ActorProxy:PrintLocation() end

function ActorProxy:Destroy() end

---@class ComponentProxy
---@field Name string
---@field Owner ActorProxy
---@field TypeName string
local ComponentProxy = {}

---@return boolean
function ComponentProxy:IsValid() end

---@return string
function ComponentProxy:GetTypeName() end

---@param active boolean
---@return boolean
function ComponentProxy:SetActive(active) end

---@return boolean
function ComponentProxy:IsActive() end

---@return Vector|nil
function ComponentProxy:GetLocation() end

---@param location Vector
---@return boolean
function ComponentProxy:SetLocation(location) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:SetLocation(x, y, z) end

---@param delta Vector
---@return boolean
function ComponentProxy:AddWorldOffset(delta) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:AddWorldOffset(x, y, z) end

---@param delta Vector
---@return boolean
function ComponentProxy:Translate(delta) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:Translate(x, y, z) end

---@return Vector|nil
function ComponentProxy:GetRotation() end

---@param rotation Vector
---@return boolean
function ComponentProxy:SetRotation(rotation) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:SetRotation(x, y, z) end

---@return Vector|nil
function ComponentProxy:GetScale() end

---@param scale Vector
---@return boolean
function ComponentProxy:SetScale(scale) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:SetScale(x, y, z) end

---@param enabled boolean
---@return boolean
function ComponentProxy:SetCollisionEnabled(enabled) end

---@param enabled boolean
---@return boolean
function ComponentProxy:SetGenerateOverlapEvents(enabled) end

---@param other ActorProxy
---@return boolean
function ComponentProxy:IsOverlappingActor(other) end

---@param meshPath string 현재 프로젝트의 OBJ/StaticMesh 경로 문자열을 사용합니다.
---@return boolean
function ComponentProxy:SetStaticMesh(meshPath) end

---@param text string
---@return boolean
function ComponentProxy:SetText(text) end

---@return string|nil
function ComponentProxy:GetText() end

---@param texturePath string
---@return boolean
function ComponentProxy:SetTexture(texturePath) end

---@return string|nil
function ComponentProxy:GetTexturePath() end

---@param tint Vector
---@return boolean
function ComponentProxy:SetTint(tint) end

---@param r number
---@param g number
---@param b number
---@param a number
---@return boolean
function ComponentProxy:SetTint(r, g, b, a) end

---@param label string
---@return boolean
function ComponentProxy:SetLabel(label) end

---@return string|nil
function ComponentProxy:GetLabel() end

---@return boolean
function ComponentProxy:IsHovered() end

---@return boolean
function ComponentProxy:IsPressed() end

---@return boolean
function ComponentProxy:WasClicked() end

---@param speed number
---@return boolean
function ComponentProxy:SetSpeed(speed) end

---@return number|nil
function ComponentProxy:GetSpeed() end

---@param target Vector
---@return boolean
function ComponentProxy:MoveTo(target) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:MoveTo(x, y, z) end

---@param delta Vector
---@return boolean
function ComponentProxy:MoveBy(delta) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:MoveBy(x, y, z) end

---@return boolean
function ComponentProxy:StopMove() end

---@return boolean
function ComponentProxy:IsMoveDone() end

---@type ActorProxy
obj = nil

---@param ... any
function log(...) end

---@param ... any
function warn(...) end

---@param ... any
function error_log(...) end

---@param ... any
function print(...) end

---@return number
function time() end

---@return number
function delta_time() end

---@param keyName string
---@return boolean
function GetKey(keyName) end

---@param keyName string
---@return boolean
function GetKeyDown(keyName) end

---@param keyName string
---@return boolean
function GetKeyUp(keyName) end

---@param keyName string
---@return boolean
function get_key(keyName) end

---@param keyName string
---@return boolean
function get_key_down(keyName) end

---@param keyName string
---@return boolean
function get_key_up(keyName) end

---@class InputAPI
Input = {}

---@param keyName string
---@return boolean
function Input.GetKey(keyName) end

---@param keyName string
---@return boolean
function Input.GetKeyDown(keyName) end

---@param keyName string
---@return boolean
function Input.GetKeyUp(keyName) end

---@param keyName string
---@return boolean
function Input.get_key(keyName) end

---@param keyName string
---@return boolean
function Input.get_key_down(keyName) end

---@param keyName string
---@return boolean
function Input.get_key_up(keyName) end

---@return number
function GetMouseDeltaX() end

---@return number
function GetMouseDeltaY() end

---@return number
function GetMouseWheel() end

---@return boolean
function MouseMoved() end

---@param buttonName string
---@return boolean
function IsDragging(buttonName) end

---@param buttonName string
---@return number
function GetDragDeltaX(buttonName) end

---@param buttonName string
---@return number
function GetDragDeltaY(buttonName) end

---@param buttonName string
---@return number
function GetDragDistance(buttonName) end

---@param functionName string
---@return boolean
function StartCoroutine(functionName) end

---@param seconds number
function wait(seconds) end

---@param seconds number
function Wait(seconds) end

---@param frames integer
function wait_frames(frames) end

function wait_until_move_done() end

---@param keyName string
function wait_key_down(keyName) end

-- TODO: wait_signal/signal은 현재 런타임 바인딩에서 꺼져 있습니다.
-- 필요해지면 C++ 바인딩을 켠 뒤 이 문서에 함수 stub을 다시 추가하세요.

---@alias ScriptPropertyType '"bool"'|'"int"'|'"float"'|'"string"'|'"vector"'

---@class ScriptPropertyDesc
---@field type ScriptPropertyType
---@field default? boolean|integer|number|string|Vector
---@field min? number
---@field max? number

---@param properties table<string, ScriptPropertyDesc>
function DeclareProperties(properties) end

---@generic T
---@param name string
---@param defaultValue T
---@return T
function property(name, defaultValue) end

---@param className string
---@param location Vector
---@return ActorProxy
function spawn_actor(className, location) end

---@param actorName string
---@return ActorProxy
function find_actor(actorName) end

---@param uuid integer
---@return ActorProxy
function find_actor_by_uuid(uuid) end

---@param tag string
---@return ActorProxy
function find_actor_by_tag(tag) end

---@param tag string
---@return ActorProxy[] ipairs로 순회 가능한 Lua table입니다.
function find_actors_by_tag(tag) end

---@param actor ActorProxy
function destroy_actor(actor) end

function BeginPlay() end

---@param dt number
function Tick(dt) end

function EndPlay() end

---@param otherActor ActorProxy
---@param otherComp ComponentProxy
---@param selfComp ComponentProxy
function OnBeginOverlap(otherActor, otherComp, selfComp) end

---@param otherActor ActorProxy
---@param otherComp ComponentProxy
---@param selfComp ComponentProxy
function OnEndOverlap(otherActor, otherComp, selfComp) end

---@param otherActor ActorProxy
---@param otherComp ComponentProxy
---@param selfComp ComponentProxy
---@param impactLocation Vector
---@param impactNormal Vector
function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal) end

-- TODO: InputMapping에서 발생한 ActionValue를 ScriptComponent까지 전달하는 정식 경로가 필요합니다.
-- 현재 C++에는 CallLuaInputAction 호출 함수만 있으며, 엔진 입력 루프와의 연결은 아직 정식 완료 상태가 아닙니다.
---@param actionName string
---@param value Vector
---@param scalar number
function OnInputAction(actionName, value, scalar) end
