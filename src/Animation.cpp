#include "Animation.h"

Animation::Animation(int frames, float frameTime) :
    m_frameCount(frames),
    m_frameTime(frameTime),
    m_timer(0.0f),
    currentFrame(0) {}

void Animation::update(float dt) {
    // accumulate
    m_timer += dt;

    // advance frames when enough time has passed
    while (m_timer >= m_frameTime) {
        m_timer -= m_frameTime;

        currentFrame++;

        if (currentFrame >= m_frameCount) {
            currentFrame = 0;
        }
    }
}

int Animation::GetFrame() const {
    return currentFrame;
}

void Animation::Reset() {
    m_timer = 0.0f;
    currentFrame = 0;
}