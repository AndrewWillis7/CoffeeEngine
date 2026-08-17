#version 120

// Renderer2D's default shader -- used whenever a DrawQuad/DrawTexturedQuad
// call passes a null or invalid Shader*. Paired with quad.vert.

uniform vec4 u_Color;

void main() {
    gl_FragColor = u_Color;
}