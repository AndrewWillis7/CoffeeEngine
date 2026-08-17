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

// On-grid pixel snapping: when > 0.5, the quad's CENTER is snapped to
// the nearest whole native/virtual pixel before the local (possibly
// rotated) corner offsets are added back on top -- so the quad's shape
// is never distorted, only its position is pinned to the grid. Without
// this, a smoothly-following camera (Camera2D::Follow's exponential
// lerp lands on a fractional position most frames) makes every sprite
// render at a fractional subpixel offset, which reads as shimmer/blur
// on pixel art. Renderer2D::ApplyCommonUniforms sets this to 1.0 for
// world-space draws (DrawQuad/DrawTexturedQuad -- actual game objects)
// and 0.0 for screen-space draws (DrawScreenQuad/DrawScreenTexturedQuad
// -- the debug UI panel and the border background), so UI never gets
// forced onto the game's pixel grid.
uniform float u_PixelSnap;

varying vec2 v_LocalPos;

void main() {
    vec2 scaled = a_LocalPos * u_Size;

    float c = cos(u_Rotation);
    float s = sin(u_Rotation);
    vec2 rotated = vec2(scaled.x * c - scaled.y * s, scaled.x * s + scaled.y * c);

    // Position relative to the camera's center -- NOT yet including the
    // local corner offset above, so snapping below only ever moves the
    // quad's CENTER onto the grid, never distorts its shape.
    vec2 centerRelative = u_Position - u_CameraPos;
    if (u_PixelSnap > 0.5) {
        // floor(x + 0.5) is GLSL 120's round-to-nearest (no built-in
        // round() until GLSL 130) -- snapping in this space (world units
        // relative to the camera) is exactly native/virtual pixel units,
        // since 1 world unit == 1 native pixel whenever the camera's
        // viewportSize matches the game's native resolution.
        centerRelative = floor(centerRelative + 0.5);
    }

    // Scaled by how many world units the viewport spans -- NOT the raw
    // window resolution -- so a small u_ViewportSize (e.g. 640x360)
    // fills the whole window with those pixels blown up, independent of
    // the window's actual size. Renderer2D::ApplyCommonUniforms feeds
    // u_CameraPos/u_ViewportSize the window's own center/size when no
    // camera is active (or for screen-space UI draws), which makes this
    // reduce exactly to the old glOrtho(0, width, height, 0, -1, 1)
    // mapping: origin top-left, +y down.
    vec2 relative = centerRelative + rotated;
    vec2 ndc = vec2(
        (relative.x / u_ViewportSize.x) * 2.0,
        -(relative.y / u_ViewportSize.y) * 2.0
    );

    v_LocalPos = a_LocalPos;
    gl_Position = vec4(ndc, 0.0, 1.0);
}