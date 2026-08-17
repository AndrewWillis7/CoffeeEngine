#version 120

// Named shader "Textured" -- samples an RGBA texture with a UV sub-rect
// (u_UVOffset/u_UVScale), tinted by u_Color. Paired with quad.vert.

uniform sampler2D u_Texture;
uniform vec4 u_Color;   // tint -- white for no tint
uniform vec2 u_UVOffset;
uniform vec2 u_UVScale;

varying vec2 v_LocalPos;

void main() {
    vec2 uv = (v_LocalPos + 0.5) * u_UVScale + u_UVOffset;
    gl_FragColor = texture2D(u_Texture, uv) * u_Color;
}