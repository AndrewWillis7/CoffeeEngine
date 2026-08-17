#version 120

// Named shader "Text" -- samples the GL_ALPHA font atlas (Font.cpp) and
// carries color entirely via u_Color, since the atlas has no RGB
// channels. Paired with quad.vert.

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