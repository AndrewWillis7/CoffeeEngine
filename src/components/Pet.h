#pragma once
#include "./Actor.h"

enum class PetState {
    Idle,
    Walking
};

class Pet : public Actor {
public:
    Pet();

    void update(float dt) override;
    RenderData GetRenderData() const override;

    void SetBounds(int minX, int maxX);

private:
    void EnterState(PetState state);

    PetState m_state;

    float m_timeInState = 0.0f;
    float m_stateTimer = 0.0f;

    float m_minIdle = 1.0f;
    float m_maxIdle = 3.0f;

    float m_minWalk = 2.0f;
    float m_maxWalk = 5.0f;

    float m_speed = 100.0f;

    int m_minX = 0;
    int m_maxX = 1000;

    // Animations
    Animation m_idleAnim;
    Animation m_walkAnim;
};