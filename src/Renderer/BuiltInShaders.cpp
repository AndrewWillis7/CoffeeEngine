#include "BuiltInShaders.h"

namespace BuiltInShaders {

const char* QuadVertexSrc = R"GLSL(
#version 120

attribute vec2 a_LocalPos;   // unit quad corner, range [-0.5, 0.5]

uniform vec2 u_Position;     // world-space center, pixels
uniform vec2 u_Size;         // width/height, pixels (already includes overdraw)
uniform float u_Rotation;    // radians
uniform vec2 u_Resolution;   // window size, pixels

varying vec2 v_LocalPos;

void main() {
    vec2 scaled = a_LocalPos * u_Size;

    float c = cos(u_Rotation);
    float s = sin(u_Rotation);
    vec2 rotated = vec2(scaled.x * c - scaled.y * s, scaled.x * s + scaled.y * c);

    vec2 worldPos = u_Position + rotated;

    // Matches the legacy glOrtho(0, width, height, 0, -1, 1) convention:
    // origin top-left, +y down.
    vec2 ndc = vec2(
        (worldPos.x / u_Resolution.x) * 2.0 - 1.0,
        1.0 - (worldPos.y / u_Resolution.y) * 2.0
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

} // namespace BuiltinShaders