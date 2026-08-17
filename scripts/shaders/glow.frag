#version 120

// Named shader "Glow" -- also used standalone via Actors.CreateGlow().
// Paired with quad.vert.

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