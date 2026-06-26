#pragma once
#include "./Actor.h"
#include <functional>
#include <memory>

class Container : public Actor {
public:
    void AddChild(std::shared_ptr<Actor> actor) {
        m_children.push_back(actor);
    }

    void update(float dt) override {
        if (isPhysicsEnabled)
            Actor::update(dt); // Restores Physics
        
        for (auto& c : m_children)
            c->update(dt);
    }

    void CollectRenderData(std::vector<RenderData>& queue) {
        queue.push_back(GetRenderData());

        for (auto& c : m_children)
            queue.push_back(c->GetRenderData());
    }

    void HandleClick(int mx, int my) {
        for (auto& c : m_children) {
            if (auto i = dynamic_cast<Actor*>(c.get())) {
                i->OnClick(mx, my);
            }
        }
    }

    void togglePhysics(bool physTogg) {
        isPhysicsEnabled = physTogg;
    }

private:
    bool isPhysicsEnabled = true;
    std::vector<std::shared_ptr<Actor>> m_children;
};