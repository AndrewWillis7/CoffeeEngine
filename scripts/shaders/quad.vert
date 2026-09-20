#version 120

// The shared vertex stage every fragment shader in scripts/shaders/ pairs
// against. Read once and cached by ShaderLibrary::SharedVertexSrc().

attribute vec2 a_LocalPos;   // unit quad corner, range [-0.5, 0.5]

uniform vec2 u_Position;     // world-space center, pixels
uniform vec2 u_Size;         // width/height, pixels (already includes overdraw)
uniform float u_Rotation;    // radians
uniform vec2 u_Resolution;   // real window size; for fragment shaders that want
                             // screen pixels. Position math uses u_ViewportSize.
uniform vec2 u_CameraPos;    // world-space point mapped to screen center
uniform vec2 u_ViewportSize; // world units visible across the full window

// When > 0.5, the quad's CENTRE snaps to the nearest whole native pixel before
// the rotated corner offsets are added back, so only its position is pinned to
// the grid, never its shape. Without it a smoothly-following camera renders
// every sprite at a fractional offset, which reads as shimmer on pixel art.
// Set for world draws and cleared for screen draws, so UI is never forced onto
// the game's grid.
uniform float u_PixelSnap;

varying vec2 v_LocalPos;

void main() {
    vec2 scaled = a_LocalPos * u_Size;

    float c = cos(u_Rotation);
    float s = sin(u_Rotation);
    vec2 rotated = vec2(scaled.x * c - scaled.y * s, scaled.x * s + scaled.y * c);

    // Relative to the camera centre, NOT yet including the corner offset, so
    // the snap below only ever moves the quad's centre onto the grid.
    vec2 centerRelative = u_Position - u_CameraPos;
    if (u_PixelSnap > 0.5) {
        // floor(x + 0.5) is GLSL 120's round-to-nearest. This space is exactly
        // native pixel units whenever viewportSize matches the native resolution.
        centerRelative = floor(centerRelative + 0.5);
    }

    // Scaled by world units spanned, NOT window resolution, so a small viewport
    // fills the window with those pixels blown up at any window size. With no
    // camera the renderer feeds the window's own centre and size, which reduces
    // this exactly to glOrtho(0, width, height, 0): origin top-left, +y down.
    vec2 relative = centerRelative + rotated;
    vec2 ndc = vec2(
        (relative.x / u_ViewportSize.x) * 2.0,
        -(relative.y / u_ViewportSize.y) * 2.0
    );

    v_LocalPos = a_LocalPos;
    gl_Position = vec4(ndc, 0.0, 1.0);
}