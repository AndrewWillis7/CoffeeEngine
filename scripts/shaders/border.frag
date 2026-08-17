#version 120

// Named shader "Border" -- the engine's DEFAULT letterbox/pillarbox
// fill, an animated diagonal sin-wave sweep between two colors. Drawn
// by Renderer2D::SetActiveCamera as a single quad covering the FULL
// real window, before the GL viewport narrows down to the (possibly
// smaller, letterboxed) camera content rect -- so this is what shows
// through in whatever margin space the aspect-fit leaves behind,
// instead of a flat clear color. Paired with quad.vert like every
// other shader here; v_LocalPos is [-0.5, 0.5] across the WHOLE window
// in this case (the quad IS the window), not a single game object,
// since Renderer2D::DrawScreenQuad sizes it to (m_Width, m_Height).
//
// Swap this out at runtime with your own .frag file (see
// border_plain.frag for a minimal example) via:
//   Actors.LoadShaderFromFile("Border", "scripts/shaders/your_file.frag")

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