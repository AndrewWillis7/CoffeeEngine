#pragma once
#include "Animation.h"
#include <string>
#include <functional>

struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct RenderData {
    std::wstring spriteKey;
    int frame;
    int x, y;
    bool flip;
    float scale;
};

class Actor {
public:
    Actor();
    virtual ~Actor() = default;

    virtual void update(float dt);

    // Transform
    void SetPosition(float x, float y);
    Vector2 GetPosition() const;
    void SetSpriteKey(const std::wstring& key);
    Vector2 GetHitBox() const;
    void SetHitBox(int x, int y);

    void SetScale(float s);
    float GetScale() const;
    void SetGround(float g);

    // Velocity
    void SetVelocity(float vx, float vy);
    Vector2 GetVelocity() const;

    // Render Output
    virtual RenderData GetRenderData() const;
    virtual void setAnimation(const std::wstring& key, int frames, float delta) const;

    // Interaction
    virtual void OnClick(int mx, int my);
    virtual bool IsInteractable() const { return m_isInteractable; }
    std::function<void()> onClick;
    bool Contains(int mx, int my) const;

protected:
    Vector2 m_position;
    Vector2 m_velocity;
    Vector2 m_hitbox;
    std::wstring m_spriteKey;

    float m_scale = 2.0f;

    //Animation Layer
    Animation* m_activeAnim = nullptr;
    bool m_flip = false;

    // All these values are chosen to make a 4-Frame walk cycle look good. Override as necessary
    float m_gravity = 400.0f;
    float m_groundY = 0.0f;
    
    float m_hopTimer = 0.0f;
    float m_hopInterval = 0.24f;
    float m_hopStrength = -72.0f;

    // Interaction
    bool m_isInteractable = false;
};