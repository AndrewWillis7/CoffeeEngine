#version 120

// The default letterbox fill: a night sky with sparse stars toward the top and
// grey smoke drifting near the bottom. Drawn as one quad covering the FULL
// window before the viewport narrows to the camera's content rect, so this is
// what shows through the margins. v_LocalPos therefore spans the whole window
// rather than one game object.
//
// Everything is quantized onto the native pixel grid (via u_PixelScale) BEFORE
// any noise runs, so stars and clouds render as flat chunky blocks matching the
// rest of the game rather than smooth gradients, at any window size.
//
// Swap it at runtime -- border_plain.frag is a minimal example:
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

    // Snap to the native grid FIRST; everything below reads this position.
    vec2 fragPixel = (v_LocalPos + 0.5) * u_Resolution;
    vec2 nativePixel = floor(fragPixel / pixelScale);
    float nativeHeight = u_Resolution.y / pixelScale;

    // 0 at the top of the window, 1 at the bottom -- drives both falloffs.
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
    // Sampled on a coarser grid than a single native pixel: at a modest window
    // size one native pixel can be 1-2 real pixels, too fine to read as chunky.
    // Rounding down to a block first means every pixel in a block samples the
    // same fbm value, so the cloud's edge steps in visible increments at any
    // window size -- the dithered look classic pixel-art skies use.
    vec2 cloudBlock = floor(nativePixel / max(u_CloudBlockSize, 1.0)) * u_CloudBlockSize;
    vec2 cloudUV = (cloudBlock + vec2(u_Time * u_CloudSpeed, 0.0)) / max(u_CloudCellSize, 1.0);
    float density = smoothstep(0.42, 0.72, fbm(cloudUV));
    float cloudMask = smoothstep(0.5, 1.0, v); // ramps in over the bottom half
    float cloudAlpha = density * cloudMask;

    col = mix(col, u_CloudColor, cloudAlpha);

    gl_FragColor = vec4(col, 1.0);
}