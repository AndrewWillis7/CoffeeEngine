#include "Actor.h"

Actor::Actor() {}

void Actor::update(float dt) {
    // Default Movement
    m_position.x += m_velocity.x * dt;
    m_position.y += m_velocity.y * dt;

    m_velocity.y += m_gravity * dt; 
    m_position.y += m_velocity.y * dt; // Changed from Velocity vector.y += gravity * time

    // Jump Height Peak-Adjustment;
    m_hopStrength = -m_gravity * 0.18f;

    if (m_position.y >= m_groundY) {
        m_position.y = m_groundY;
        m_velocity.y = 0.0f;

        if (m_velocity.x != 0.0f) {
            m_hopTimer += dt;

            if (m_hopTimer >= m_hopInterval) {
                m_hopTimer = 0.0f;
                m_velocity.y = m_hopStrength;
            }
        } else {
            m_hopTimer = 0.0f;
        }
    }

    if (m_activeAnim)
        m_activeAnim->update(dt);
    
    // Flip based on Velocity
    if (m_velocity.x != 0) {
        m_flip = (m_velocity.x > 0);
    }
}

void Actor::SetSpriteKey(const std::wstring& key) {
    m_spriteKey = key;
}

void Actor::SetPosition(float x, float y) {
    m_position = {x, y};
}

Vector2 Actor::GetPosition() const {
    return m_position;
}

void Actor::SetScale(float s) {
    m_scale = s;
}

float Actor::GetScale() const {
    return m_scale;
}

Vector2 Actor::GetHitBox() const {
    return m_hitbox;
}

void Actor::SetHitBox(int x, int y) {
    m_hitbox.x = x;
    m_hitbox.y = y;
}

void Actor::SetVelocity(float vx, float vy) {
    m_velocity = {vx, vy};
}

Vector2 Actor::GetVelocity() const {
    return m_velocity;
}

void Actor::SetGround(float g) {
    m_groundY = g;
}

RenderData Actor::GetRenderData() const {
    RenderData data{};
    data.spriteKey = m_spriteKey;
    data.frame = (m_activeAnim) ? m_activeAnim->GetFrame() : 0;
    data.x = (int)m_position.x;
    data.y = (int)m_position.y;
    data.scale = m_scale;
    data.flip = m_flip;

    return data;
}
 
// This is broken, but the animation pipeline needs a lot of work... (Should be a single action to change an actors sprite. Almost sprites should be animated so maybe we update that...)
void Actor::setAnimation(const std::wstring& key, int frames, float delta) const {
    Animation newAnim(frames, delta);
    m_spriteKey = key;
    m_activeAnim = &newAnim;
}

bool Actor::Contains(int mx, int my) const {
    return (mx >= m_position.x && mx <= m_position.x + m_hitbox.x &&
           my >= m_position.y && my <= m_position.y + m_hitbox.y);
}

void Actor::OnClick(int mx, int my) {
    if (Contains(mx, my)) {
        if (onClick) onClick();
    }
}