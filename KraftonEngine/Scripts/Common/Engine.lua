local Engine = {}

function Engine.IsValidObject(value)
    return value ~= nil and value.IsValid ~= nil and value:IsValid()
end

function Engine.IsValidActor(actor)
    return Engine.IsValidObject(actor)
end

function Engine.IsValidComponent(component)
    return Engine.IsValidObject(component)
end

function Engine.GetComponent(owner, ...)
    if not owner or not owner.GetComponent then
        return nil
    end

    local names = { ... }

    for index = 1, #names do
        local name = names[index]
        local component = owner:GetComponent(name)

        if Engine.IsValidComponent(component) then
            return component
        end
    end

    return nil
end

function Engine.GetRequiredComponent(owner, missing_message, ...)
    local component = Engine.GetComponent(owner, ...)
    if component then
        return component
    end

    local names = { ... }
    warn(missing_message or "Component missing:", table.concat(names, ", "))
    return nil
end

function Engine.GetComponentByType(owner, type_name)
    if not Engine.IsValidObject(owner) or not owner.GetComponentByType then
        return nil
    end

    local component = owner:GetComponentByType(type_name)
    if Engine.IsValidComponent(component) then
        return component
    end

    return nil
end

function Engine.GetStaticMeshComponent(actor)
    if not Engine.IsValidActor(actor) or not actor.GetStaticMeshComponent then
        return nil
    end

    local mesh = actor:GetStaticMeshComponent()
    if Engine.IsValidComponent(mesh) then
        return mesh
    end

    return nil
end

function Engine.FindCollisionShape(actor)
    local shape = Engine.GetComponentByType(actor, "UBoxComponent")
    if shape then
        return shape
    end

    shape = Engine.GetComponentByType(actor, "USphereComponent")
    if shape then
        return shape
    end

    return Engine.GetComponentByType(actor, "UCapsuleComponent")
end

return Engine
