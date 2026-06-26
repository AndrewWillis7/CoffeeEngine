class Animation {
public:
    Animation(int frames, float frameTime);

    void update(float dt);
    int GetFrame() const;

    void Reset();

private:
    int m_frameCount;
    float m_frameTime;
    float m_timer = 0.0f;

    int currentFrame = 0;
};