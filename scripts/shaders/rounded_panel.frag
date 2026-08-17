#version 120

// Named shader "RoundedPanel" -- SDF rounded-rect with border/drop
// shadow, used by the engine debug UI (UIPanel). Paired with quad.vert.

uniform vec4 u_Color;         // fill
uniform vec2 u_Size;          // quad size, pixels (already includes overdraw)
uniform float u_CornerRadius; // pixels
uniform vec4 u_BorderColor;
uniform float u_BorderWidth;  // pixels
uniform vec4 u_ShadowColor;
uniform vec2 u_ShadowOffset;  // pixels
uniform float u_ShadowBlur;   // pixels

varying vec2 v_LocalPos; // [-0.5, 0.5] regardless of overdraw, same convention as glow.frag

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