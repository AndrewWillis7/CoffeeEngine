#pragma once

// Marks a RigidBody2D as the player-controlled actor.
class PlayerActorConfig {
public:
    float moveSpeed = 200.0f;
    float jumpForce = 400.0f;
    bool inputEnabled = true;
};
