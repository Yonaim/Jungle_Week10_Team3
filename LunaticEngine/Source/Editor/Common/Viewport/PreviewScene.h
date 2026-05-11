#pragma once

#include "Render/Scene/FScene.h"

#include <cstddef>
#include <vector>

class UPrimitiveComponent;
class FPrimitiveSceneProxy;

// Asset preview 전용 scene wrapper.
// World 타입을 만들지 않고 FScene만 소유하며, Preview 전용 Component/Proxy 생명주기를 함께 관리한다.
class FPreviewScene
{
public:
    FPreviewScene() = default;
    ~FPreviewScene() { Clear(); }

    FScene& GetScene() { return Scene; }
    const FScene& GetScene() const { return Scene; }

    FPrimitiveSceneProxy* AddPrimitive(UPrimitiveComponent* Component) { return AddComponent(Component); }
    void RemovePrimitive(FPrimitiveSceneProxy* Proxy) { RemoveProxy(Proxy); }

    FPrimitiveSceneProxy* AddComponent(UPrimitiveComponent* Component)
    {
        if (!Component)
        {
            return nullptr;
        }

        FPrimitiveSceneProxy* Proxy = Scene.AddPrimitive(Component);
        if (Proxy)
        {
            Components.push_back(Component);
            Proxies.push_back(Proxy);
        }
        return Proxy;
    }

    void RemoveComponent(UPrimitiveComponent* Component)
    {
        if (!Component)
        {
            return;
        }

        for (size_t Index = 0; Index < Components.size(); ++Index)
        {
            if (Components[Index] == Component)
            {
                if (Index < Proxies.size() && Proxies[Index])
                {
                    Scene.RemovePrimitive(Proxies[Index]);
                }
                Components.erase(Components.begin() + static_cast<std::ptrdiff_t>(Index));
                Proxies.erase(Proxies.begin() + static_cast<std::ptrdiff_t>(Index));
                return;
            }
        }
    }

    void RemoveProxy(FPrimitiveSceneProxy* Proxy)
    {
        if (!Proxy)
        {
            return;
        }

        for (size_t Index = 0; Index < Proxies.size(); ++Index)
        {
            if (Proxies[Index] == Proxy)
            {
                Scene.RemovePrimitive(Proxy);
                Components.erase(Components.begin() + static_cast<std::ptrdiff_t>(Index));
                Proxies.erase(Proxies.begin() + static_cast<std::ptrdiff_t>(Index));
                return;
            }
        }

        Scene.RemovePrimitive(Proxy);
    }

    void Clear()
    {
        for (FPrimitiveSceneProxy* Proxy : Proxies)
        {
            if (Proxy)
            {
                Scene.RemovePrimitive(Proxy);
            }
        }
        Components.clear();
        Proxies.clear();
        Scene.ClearFrameData();
    }

    void ClearFrameData() { Scene.ClearFrameData(); }

private:
    FScene Scene;
    std::vector<UPrimitiveComponent*> Components;
    std::vector<FPrimitiveSceneProxy*> Proxies;
};
