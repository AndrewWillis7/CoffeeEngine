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
