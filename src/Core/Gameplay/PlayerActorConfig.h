#pragma once

// Marks a rigidBody2D as the actor the player controls,
// Holds the player physics properties

class PlayerActorConfig {
public:
    float moveSpeed = 200.0f;
    float jumpForce = 400.0f;
    bool inputEnabled = true;
};