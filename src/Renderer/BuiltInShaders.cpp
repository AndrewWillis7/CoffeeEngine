#include "BuiltInShaders.h"

namespace BuiltInShaders {

const char* QuadVertexSrc = R"GLSL(
    #version 120

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
)GLSL";

const char* FlatFragmentSrc = R"GLSL(
#version 120

uniform vec4 u_Color;

void main() {
    gl_FragColor = u_Color;
}
)GLSL";

const char* GlowFragmentSrc = R"GLSL(
#version 120

uniform vec4 u_Color;          // base fill color of the square
uniform vec3 u_GlowColor;      // glow tint
uniform float u_GlowIntensity; // glow brightness multiplier
uniform float u_Time;          // seconds, for a subtle pulse

varying vec2 v_LocalPos;

void main() {
    // v_LocalPos is in [-0.5, 0.5] on each axis regardless of overdrawScale
    // (the quad geometry is just bigger in world space, not in local UV).
    // dist == 0 at the center, 1.0 at an edge midpoint, ~1.41 at a corner --
    // so shaping the falloff between those two lets the glow fill whatever
    // overdraw margin Shader::overdrawScale gave it.
    float dist = length(v_LocalPos) * 2.0;

    float core = 1.0 - smoothstep(0.0, 0.6, dist);
    float pulse = 0.85 + 0.15 * sin(u_Time * 4.0);
    float halo = (1.0 - smoothstep(0.0, 1.4, dist)) * u_GlowIntensity * pulse;

    vec3 rgb = u_Color.rgb * core + u_GlowColor * halo;
    float alpha = clamp(core + halo, 0.0, 1.0) * u_Color.a;

    gl_FragColor = vec4(rgb, alpha);
}
)GLSL";

// File: src/Renderer/BuiltInShaders.cpp -- add inside namespace BuiltInShaders

const char* RoundedPanelFragmentSrc = R"GLSL(
#version 120

uniform vec4 u_Color;         // fill
uniform vec2 u_Size;          // quad size, pixels (already includes overdraw)
uniform float u_CornerRadius; // pixels
uniform vec4 u_BorderColor;
uniform float u_BorderWidth;  // pixels
uniform vec4 u_ShadowColor;
uniform vec2 u_ShadowOffset;  // pixels
uniform float u_ShadowBlur;   // pixels

varying vec2 v_LocalPos; // [-0.5, 0.5] regardless of overdraw, same convention as GlowFragmentSrc

float RoundedBoxSDF(vec2 p, vec2 halfSize, float radius) {
    vec2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main() {
    vec2 pixelPos = v_LocalPos * u_Size; // back to pixel space, origin at center
    vec2 halfSize = u_Size * 0.5;

    float dist = RoundedBoxSDF(pixelPos, halfSize, u_CornerRadius);
    float shadowDist = RoundedBoxSDF(pixelPos - u_ShadowOffset, halfSize, u_CornerRadius);

    float fillAlpha = 1.0 - smoothstep(0.0, 1.5, dist);
    float borderMask = smoothstep(0.0, 1.5, dist + u_BorderWidth) - smoothstep(0.0, 1.5, dist);
    float shadowAlpha = (1.0 - smoothstep(0.0, u_ShadowBlur, shadowDist)) * u_ShadowColor.a;

    vec4 color = u_ShadowColor * shadowAlpha;
    color = mix(color, u_Color, fillAlpha);
    color = mix(color, u_BorderColor, borderMask);

    gl_FragColor = color;
}
)GLSL";

const char* TexturedFragmentSrc = R"GLSL(
#version 120

uniform sampler2D u_Texture;
uniform vec4 u_Color;   // tint -- white for no tint
uniform vec2 u_UVOffset;
uniform vec2 u_UVScale;

varying vec2 v_LocalPos;

void main() {
    vec2 uv = (v_LocalPos + 0.5) * u_UVScale + u_UVOffset;
    gl_FragColor = texture2D(u_Texture, uv) * u_Color;
}
)GLSL";

const char* TextFragmentSrc = R"GLSL(
#version 120

// Font atlas is a single-channel (GL_ALPHA) texture -- r/g/b sample as 0,
// only .a carries the glyph coverage. Color comes entirely from u_Color.
uniform sampler2D u_Texture;
uniform vec4 u_Color;
uniform vec2 u_UVOffset;
uniform vec2 u_UVScale;

varying vec2 v_LocalPos;

void main() {
    vec2 uv = (v_LocalPos + 0.5) * u_UVScale + u_UVOffset;
    float coverage = texture2D(u_Texture, uv).a;
    gl_FragColor = vec4(u_Color.rgb, u_Color.a * coverage);
}
)GLSL";

// Renderer2D::SetActiveCamera draws this as a single quad covering the
// FULL real window, before switching the GL viewport down to the
// (possibly smaller, letterboxed) camera content rect -- so this is what
// shows through in whatever margin space the aspect-fit leaves behind,
// instead of a flat clear color. Reuses QuadVertexSrc like every other
// built-in fragment shader here; v_LocalPos is [-0.5, 0.5] across the
// WHOLE window in this case (the quad IS the window), not a single game
// object, since Renderer2D::DrawScreenQuad sizes it to (m_Width, m_Height).
const char* BorderFragmentSrc = R"GLSL(
#version 120

uniform float u_Time;
uniform vec3 u_BorderColorA;
uniform vec3 u_BorderColorB;
uniform float u_BorderSpeed;      // cycle speed
uniform float u_BorderWaveScale;  // how many wave cycles sweep across the window

varying vec2 v_LocalPos; // [-0.5, 0.5] across the full window

void main() {
    // Diagonal sweep: phase depends on position (so the color moves
    // across the screen, not just pulses in place) plus time (so it
    // animates). 0.5 + 0.5*sin(...) remaps sin's [-1,1] into a usable
    // [0,1] blend factor between the two colors.
    float phase = (v_LocalPos.x + v_LocalPos.y) * u_BorderWaveScale + u_Time * u_BorderSpeed;
    float t = 0.5 + 0.5 * sin(phase);
    vec3 rgb = mix(u_BorderColorA, u_BorderColorB, t);
    gl_FragColor = vec4(rgb, 1.0);
}
)GLSL";

} // namespace BuiltinShaders