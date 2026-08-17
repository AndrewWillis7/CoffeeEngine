#version 120

// Named shader "Border" -- the engine's DEFAULT letterbox/pillarbox
// fill: a black night sky, sparse pixel-art stars fading in toward the
// top, and dark grey smoke/cloud shapes drifting near the bottom.
// Drawn by Renderer2D::SetActiveCamera as a single quad covering the
// FULL real window, before the GL viewport narrows down to the
// (possibly smaller, letterboxed) camera content rect -- so this is
// what shows through in whatever margin space the aspect-fit leaves
// behind. Paired with quad.vert like every other shader here;
// v_LocalPos is [-0.5, 0.5] across the WHOLE window in this case (the
// quad IS the window), not a single game object, since
// Renderer2D::DrawScreenQuad sizes it to (m_Width, m_Height).
//
// Everything below is quantized onto the native/virtual pixel grid
// (via u_PixelScale, set by Renderer2D::SetActiveCamera every frame)
// BEFORE any noise/star math runs, so stars and clouds render as flat,
// chunky pixel-art blocks -- matching the on-grid look of the rest of
// the game -- rather than smooth gradients, and stay pixel-perfect at
// any real window size.
//
// Swap this out at runtime with your own .frag file (see
// border_plain.frag for a minimal example) via:
//   Actors.LoadShaderFromFile("Border", "scripts/shaders/your_file.frag")

uniform float u_Time;
uniform vec2 u_Resolution;      // real window size, pixels
uniform float u_PixelScale;     // real screen pixels per native/virtual pixel

uniform vec3 u_SkyColor;
uniform vec3 u_StarColor;
uniform vec3 u_CloudColor;
uniform float u_StarCellSize;     // native pixels per star grid cell
uniform float u_CloudCellSize;    // native pixels per cloud noise wavelength
uniform float u_CloudBlockSize;   // native pixels per chunky cloud "pixel"
uniform float u_CloudSpeed;       // native pixels/sec the cloud layer drifts
uniform float u_StarTwinkleSpeed;

varying vec2 v_LocalPos; // [-0.5, 0.5] across the full window

// --- Cheap hash-based value noise, GLSL 120 (no built-in noise()) ---

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f); // smoothstep-shaped interpolant
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p) {
    float total = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 4; i++) {
        total += valueNoise(p) * amp;
        p *= 2.02;
        amp *= 0.5;
    }
    return total;
}

void main() {
    float pixelScale = max(u_PixelScale, 1.0);

    // Snap to the native/virtual pixel grid FIRST -- everything below
    // reads from this grid-quantized position.
    vec2 fragPixel = (v_LocalPos + 0.5) * u_Resolution;
    vec2 nativePixel = floor(fragPixel / pixelScale);
    float nativeHeight = u_Resolution.y / pixelScale;

    // v: 0 at the top of the window, 1 at the bottom -- drives the
    // vertical star/cloud density falloffs below.
    float v = nativePixel.y / max(nativeHeight, 1.0);

    vec3 col = u_SkyColor;

    // --- Stars: sparse grid of hard-edged dots, denser near the top ---
    vec2 starGrid = nativePixel / max(u_StarCellSize, 1.0);
    vec2 starCell = floor(starGrid);
    vec2 starLocal = fract(starGrid);

    float starRoll = hash21(starCell);
    float hasStar = step(0.90, starRoll); // ~10% of cells get a star
    vec2 starOffset = vec2(hash21(starCell + 3.7), hash21(starCell + 9.1));
    float starDist = length(starLocal - starOffset);
    float starDot = step(starDist, 0.18); // hard edge -- no soft glow, stays pixel-arty

    float twinkle = 0.55 + 0.45 * sin(u_Time * u_StarTwinkleSpeed * (0.5 + starRoll) + starRoll * 30.0);
    float starMask = 1.0 - smoothstep(0.0, 0.65, v); // fades out by ~65% down the window
    float star = hasStar * starDot * twinkle * starMask;

    col += u_StarColor * star;

    // --- Clouds: soft fbm shapes, denser near the bottom, drifting ---
    // Sampled on a coarser block grid than a single native pixel (see
    // u_CloudBlockSize) -- a single native pixel's worth of quantization
    // (like the star grid above) is too fine to read as chunky pixel art
    // once u_PixelScale is small (a modest window size), since one
    // native pixel can end up just 1-2 real screen pixels wide. Rounding
    // DOWN to a multiple of the block size first means every pixel
    // inside one block samples the exact same fbm value -- a flat color
    // -- so the cloud's edge steps in visible chunky increments
    // regardless of window size, the same dithered-cloud look classic
    // pixel-art skies use, instead of a smooth gradient.
    vec2 cloudBlock = floor(nativePixel / max(u_CloudBlockSize, 1.0)) * u_CloudBlockSize;
    vec2 cloudUV = (cloudBlock + vec2(u_Time * u_CloudSpeed, 0.0)) / max(u_CloudCellSize, 1.0);
    float density = smoothstep(0.42, 0.72, fbm(cloudUV));
    float cloudMask = smoothstep(0.5, 1.0, v); // ramps in over the bottom half
    float cloudAlpha = density * cloudMask;

    col = mix(col, u_CloudColor, cloudAlpha);

    gl_FragColor = vec4(col, 1.0);
}