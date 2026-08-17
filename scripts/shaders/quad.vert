#version 120

// The engine's shared vertex stage -- every fragment shader in
// scripts/shaders/ (built-in or custom) pairs against this same vertex
// stage. See ShaderLibrary::SharedVertexSrc(), which reads this file
// once and caches it.

attribute vec2 a_LocalPos;   // unit quad corner, range [-0.5, 0.5]

uniform vec2 u_Position;     // world-space center, pixels
uniform vec2 u_Size;         // width/height, pixels (already includes overdraw)
uniform float u_Rotation;    // radians
uniform vec2 u_Resolution;   // actual window size, pixels -- kept for any
                              // fragment shader that wants real screen
                              // pixels; NOT used for position math below
                              // anymore, that's u_ViewportSize's job.
uniform vec2 u_CameraPos;    // world-space point mapped to screen center
uniform vec2 u_ViewportSize; // world units visible across the full window

varying vec2 v_LocalPos;

void main() {
    vec2 scaled = a_LocalPos * u_Size;

    float c = cos(u_Rotation);
    float s = sin(u_Rotation);
    vec2 rotated = vec2(scaled.x * c - scaled.y * s, scaled.x * s + scaled.y * c);

    vec2 worldPos = u_Position + rotated;

    // Position relative to the camera's center, then scaled by how many
    // world units the viewport spans -- NOT the raw window resolution --
    // so a small u_ViewportSize (e.g. 320x180) fills the whole window
    // with those pixels blown up, independent of the window's actual
    // size. Renderer2D::ApplyCommonUniforms feeds u_CameraPos/
    // u_ViewportSize the window's own center/size when no camera is
    // active (or for screen-space UI draws), which makes this reduce
    // exactly to the old glOrtho(0, width, height, 0, -1, 1) mapping:
    // origin top-left, +y down.
    vec2 relative = worldPos - u_CameraPos;
    vec2 ndc = vec2(
        (relative.x / u_ViewportSize.x) * 2.0,
        -(relative.y / u_ViewportSize.y) * 2.0
    );

    v_LocalPos = a_LocalPos;
    gl_Position = vec4(ndc, 0.0, 1.0);
}