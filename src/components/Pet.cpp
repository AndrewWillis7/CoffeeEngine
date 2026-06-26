#include "Pet.h"
#include <cstdlib>

float RandomRange(float min, float max) {
    return min + (float(rand()) / RAND_MAX) * (max - min);
}

Pet::Pet() :
    m_idleAnim(1, 0.5f),
    m_walkAnim(4, 0.12f) {
        EnterState(PetState::Walking);
    }

void Pet::SetBounds(int minX, int maxX) {
    m_minX = minX;
    m_maxX = maxX;
}

void Pet::EnterState(PetState state) {
    m_state = state;
    m_timeInState = 0.0f;

    if (state == PetState::Walking) {
        m_stateTimer = RandomRange(m_minWalk, m_maxWalk);

        float dir = (rand() % 2 == 0) ? 1.0f : -1.0f;
        SetVelocity(dir * m_speed, 0.0f);

        m_activeAnim = &m_walkAnim;
        m_walkAnim.Reset();
    }
    else {
        m_stateTimer = RandomRange(m_minIdle, m_maxIdle);

        SetVelocity(0.0f, 0.0f);

        m_activeAnim = &m_idleAnim;
        m_idleAnim.Reset();
    }
}

void Pet::update(float dt) {
    m_timeInState += dt;

    // State Change
    if (m_timeInState >= m_stateTimer) {
        if (m_state == PetState::Walking)
            EnterState(PetState::Idle);
        else
            EnterState(PetState::Walking);
    }

    // Random Turning
    if (m_state == PetState::Walking) {
        if ((rand() % 1000) < 5) {
            m_velocity.x *= -1.0f;
        }
    }

    Actor::update(dt);

    //Boundary Clamp
    if (m_position.x <= m_minX) {
        m_position.x = (float)m_minX;
        m_velocity.x = m_speed;
    }
    else if (m_position.x >= m_maxX) {
        m_position.x = (float)m_maxX;
        m_velocity.x = -m_speed;
    }
}

RenderData Pet::GetRenderData() const {
    RenderData data = Actor::GetRenderData();

    if (m_state == PetState::Walking)
        data.spriteKey = L"walk";
    else
        data.spriteKey = L"idle";
    return data;
}