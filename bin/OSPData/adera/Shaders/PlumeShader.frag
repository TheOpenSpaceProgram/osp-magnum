/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
//#version 430 core
/* Rocket exhaust plume shader

 * Mixes together a number of noise textures to produce a decent looking
   rocket exhaust plume effect that scales with engine power
 * Tons of hardcoded parameters for now, later can generalize it and pass
   some sort of uniform block storing all the configuration options, but
   for now this is it (dimensions of the plume can still be specified via
   uniform, though)
*/
in vec3 normal;
in vec3 fragPos;

layout(location = 0, index = 0) out vec4 color;

/* Z position of plume ends

 * The origin is assumed to be located at the aperture of the nozzle. These
   values are used to compute the UV coordinates for the plume, with V
   increasing towards -Z.
*/
layout(location = 3) uniform float topZ;
layout(location = 4) uniform float bottomZ;

layout(location = 5) uniform sampler2D nozzleNoiseTex;
layout(location = 6) uniform sampler2D combustionNoiseTex;

/* Alpha currently unused, but later will control maximum opacity.
   It's nontrivial because even if the exhaust overall is very thin,
   that doesn't mean the exhaust will be dim at the nozzle
 */
layout(location = 7) uniform vec4 baseColor;

layout(location = 8) uniform float flowVelocity;
layout(location = 9) uniform float time;

// Engine power, which is the primary influence on plume density
layout(location = 10) uniform float power;

const float PI = 3.14159265358;
float height = topZ - bottomZ;
// The plume is defined to leave the nozzle at the mesh origin
float NOZZLE_THRESHOLD = abs(topZ / height);


/* Converts rectilinear plume coordinates to polar
 *
 * Assumes cylinderical symmetry, and that the plume is centered at the origin.
*/
vec2 plumeCoords(vec3 rect)
{
    float x = rect.x;
    float y = rect.y;
    float z = rect.z;

    float angle = atan(y, x) + PI;
    float vert = clamp((topZ - z) / height, 0.0, 1.0);

    return vec2(angle/(2*PI), vert);
}

// Offset UV by time uniform and wrap
vec2 scrollUV(vec2 UV)
{
    float tmpI;
    float scrollFac = modf(UV.y - time*flowVelocity, tmpI);
    scrollFac = float(scrollFac < 0.0)*(scrollFac + 1.0) + float(scrollFac >= 0.0)*scrollFac;
    return vec2(UV.x, scrollFac);
}

void main()
{
    // Generate cylinderical UVs from modelspace position of fragment
    vec2 polarUV  = plumeCoords(fragPos);

    /* Position Factors

     * PlumeFactor is zero at the nozzle outlet, and 1 at the tail of the plume
     * NozzleFactor is zero at the nozzle throat, and 1 at the nozzle outlet
     */
    float plumeFactor = clamp((polarUV.y - NOZZLE_THRESHOLD) / (1.0 - NOZZLE_THRESHOLD), 0.0, 1.0);
    float nozzleFactor = clamp(polarUV.y / NOZZLE_THRESHOLD, 0.0, 1.0);
    float fragInNozzle = float(polarUV.y / NOZZLE_THRESHOLD < 1.0);

    /* Plume values
     
       The plume is composed of three separate values: the base color, the
       combustion noise, and the nozzle noise.

     * The base color is simply the color of the exhaust. It can be any color
       and any opacity.
     * The combustion noise is used to simulate fluctuations in the exhaust
       caused by combustion instability. This effect scrolls down the length
       of the plume at a rate designated by the flowVelocity uniform.
     * The nozzle noise is used to simulate imperfections in the exhaust caused
       by variances in the nozzle construction. Imperfections in the nozzle
       result in azimuthal differences in plume density which remain static
       throughout the burn.
    */
    float nozzleValue = 1.0 - 0.2*texture(nozzleNoiseTex, vec2(polarUV.x, 0)).r;
    float combustValue = texture(combustionNoiseTex, scrollUV(polarUV)).r;

    /* Effect fade

     * The vertical fade factor is a smoothstep interpolation over the length
       of the plume. This is used to mix the combustion effect in, so that the
       effects magnify at the end of the plume.
     * The nozzle noise factor is a smoothstep interpolation at the top of the
       plume which fades in the effects of the nozzle noise. The nozzle noise
       begins at 0.01 and fades in fully by 0.2.
     * the throttle fade factor controls how much influence the engine power
       level influences the plume density. Since the gasses inside the nozzle
       are constrained by the nozzle walls, they will probably glow more and
       thus be more visible, whereas the free plume will fade out more quickly
       since it can freely expand. The factor is a piecewise function that is
       determined by the factor relevant to wherever the fragment is within
       the plume.
    */
    float vertFadeFactor = smoothstep(0.0, 1.0, polarUV.y);
    float nozzleNoiseFactor = smoothstep(0.01, 0.2, polarUV.y);
    float throttleFadeFactor =
            fragInNozzle*(nozzleFactor/3)
            + (1.0 - fragInNozzle)*(plumeFactor+.5);

    float combust = mix(1.0, combustValue, vertFadeFactor);
    float nozzle = mix(1.0, nozzleValue, nozzleNoiseFactor);
    float throttle = mix(1.0, power, throttleFadeFactor);
    float diffusion = (1.0 - plumeFactor);

    // The final plume density is the product of all these factors
    float plumeDensity = throttle*combust*nozzle*diffusion;

    // Square the magnitude of the fluctuation
    plumeDensity *= plumeDensity;

    color = vec4(vec3(baseColor), plumeDensity);
}

