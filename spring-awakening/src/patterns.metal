// patterns.metal — procedural materials. Included into kernel.metal by the host.
// Outputs: sf (colours etc.), g3 = object-space gradient of a height field, guv = (dh/du, dh/dv) in object length units.

// leaf venation: returns vein strength 0..1 (x) and a secondary "tertiary network" term (y)
static float2 leafVein(float u, float v, float nSec, float slope, float ret) {
    float av = fabs(v);
    float wm = 0.012 + 0.05 * (1.0 - u);
    float mid = smoothstep(wm, wm * 0.35, av);
    float sec = 0.0;
    if (nSec > 0.0) {
        float s = (u - slope * av) * nSec;
        float d = fabs(fract(s + 0.5) - 0.5) / nSec;      // distance to the vein in u units
        float w = 0.010 * (1.0 - 0.6 * av);
        sec = smoothstep(w, w * 0.3, d) * smoothstep(1.02, 0.8, av) * smoothstep(0.0, 0.08, u);
    }
    float ter = 0.0;
    if (ret > 0.0) {
        float3 wv = worley(float3(u * ret * 2.2, v * ret, 3.7));
        ter = smoothstep(0.09, 0.02, wv.y - wv.x);
    }
    return float2(max(mid, sec * 0.85), ter);
}

static void evalPattern(const device Mat& M, float3 Po, float3 Pw, float3 No, float2 uv, float attr, float prnd, float irnd, float3 tint, bool front, float time,
                        thread Surf& sf, thread float3& g3, thread float2& guv) {
    uint pat = M.pattern;
    sf.diff *= tint; sf.tcol *= tint;
    switch (pat) {
    case 0u: { float v = 0.8 + 0.4 * prnd; sf.diff *= v; sf.tcol *= v; break; }   // plain, with per-primitive value variation
    case 1u: { // soil / mineral: base..base2 by fbm, bump, sparse bright grains; p0 = (bump lo, bump hi, isCrumb, wet)
        float3 q = Po * M.scale + float3(irnd * 37.0, irnd * 11.0, 0.0);
        float4 n = fbmd(q, 4);
        float k = smoothstep(-0.3, 0.3, n.x);
        float3 c = mix(M.base.xyz, M.base2.xyz, k) * tint;
        float4 n2 = noised(q * 7.0);
        float3 wv = worley(q * 5.0);
        c *= 0.75 + 0.5 * smoothstep(0.0, 0.6, wv.x);              // darker pores between grains
        c = mix(c, float3(0.42, 0.36, 0.28) * tint, step(0.94, wv.z) * smoothstep(0.35, 0.1, wv.x)); // mineral grains
        sf.diff = c;
        g3 = n.yzw * M.scale * M.p0.x + n2.yzw * M.scale * 7.0 * M.p0.y;
        float3 e1 = worley(q * 5.0 + float3(0.03, 0.0, 0.0)), e2 = worley(q * 5.0 + float3(0.0, 0.03, 0.0)), e3 = worley(q * 5.0 + float3(0.0, 0.0, 0.03));
        g3 += float3(e1.x - wv.x, e2.x - wv.x, e3.x - wv.x) / 0.03 * (-M.p0.y * 5.0 * M.scale);
        sf.rough = clamp(M.rough - 0.35 * M.p0.w + 0.15 * n2.x, 0.12, 1.0);
        sf.coat = M.coat * (0.5 + M.p0.w);
        break; }
    case 2u: { // leaf: p0 = (nSec, slope, veinStrength, reticulation), p1 = (edgeTint, backLighten, mottling, veinBump), p2 = edge/young colour, base2 = transmission colour
        float u = uv.x, v = uv.y, av = fabs(v);
        float2 vn = leafVein(u, v, M.p0.x, M.p0.y, M.p0.w);
        float vein = max(vn.x, vn.y * 0.45) * M.p0.z;
        float mot = fbm(float3(u * 9.0, v * 5.0, irnd * 20.0), 3);
        float3 lam = M.base.xyz * (1.0 + M.p1.z * mot);
        float3 tc = M.base2.xyz * (1.0 + 0.5 * M.p1.z * mot);
        float edge = smoothstep(0.55, 1.0, av + 0.25 * mot) * M.p1.x * (0.4 + attr);
        lam = mix(lam, M.p2.xyz, clamp(edge + attr * M.p2.w, 0.0, 1.0));
        tc = mix(tc, M.p2.xyz * 1.6, clamp(edge + attr * M.p2.w, 0.0, 1.0) * 0.8);
        float3 vc = lam * float3(1.5, 1.45, 1.1) + float3(0.03, 0.035, 0.0);
        sf.diff = mix(lam, vc, vein * 0.7) * tint;
        sf.tcol = mix(tc, tc * float3(0.8, 0.75, 0.45), vein) * tint;
        sf.trans = M.trans * (1.0 - 0.55 * vein);
        if (!front) { sf.diff = mix(sf.diff, sf.diff * 1.25 + float3(0.025, 0.03, 0.02), M.p1.y); sf.coat *= 0.35; sf.rough = min(1.0, sf.rough + 0.25); }
        if (M.p1.w != 0.0) {
            float e = 0.004;
            float2 a = leafVein(u + e, v, M.p0.x, M.p0.y, M.p0.w), b = leafVein(u, v + e * 2.0, M.p0.x, M.p0.y, M.p0.w);
            float h0 = max(vn.x, vn.y * 0.45), hu = max(a.x, a.y * 0.45), hv = max(b.x, b.y * 0.45);
            float sgn = front ? -1.0 : 1.0;
            guv = float2(hu - h0, (hv - h0) * 0.5) / e * M.p1.w * sgn;
        }
        break; }
    case 3u: { // petal: p0 = (nVeins, veinStrength, streak, throatEnd), p1 = (tipDarken, backLighten, veinBump, 0), p2 = throat colour (w = throat start)
        float u = uv.x, v = uv.y;
        float d = fabs(fract(v * M.p0.x * 0.5 + 0.5 + 0.08 * noised(float3(u * 6.0, v * 3.0, irnd * 9.0)).x) - 0.5) * 2.0;
        float vein = smoothstep(0.35, 0.05, d) * M.p0.y * smoothstep(0.0, 0.2, u);
        float st = fbm(float3(u * 3.0, v * 38.0, irnd * 13.0), 3) * M.p0.z;
        float th = smoothstep(M.p0.w, M.p2.w, u);            // 1 near the throat
        float3 c = mix(M.base.xyz, M.p2.xyz, th);
        float3 tc = mix(M.base2.xyz, M.p2.xyz * 1.2, th);
        c *= (1.0 + st) * (1.0 - 0.45 * vein) * (1.0 - M.p1.x * smoothstep(0.7, 1.0, u));
        tc *= (1.0 + st) * (1.0 - 0.6 * vein);
        sf.diff = c * tint; sf.tcol = tc * tint;
        if (!front) { sf.diff = mix(sf.diff, sf.diff * 1.2 + float3(0.04), M.p1.y); sf.coat *= 0.5; }
        break; }
    case 4u: { // stem: base at attr=0 -> base2 at attr=1 (p0.x..p0.y ramp), fine longitudinal striation, p2 = speckle colour (w amount)
        float k = smoothstep(M.p0.x, M.p0.y, attr);
        float3 c = mix(M.base.xyz, M.base2.xyz, k);
        float sr = noised(float3(uv.y * 40.0, uv.x * 3.0, irnd * 7.0)).x;
        c *= 1.0 + 0.12 * sr;
        float3 wv = worley(float3(uv.x * M.p0.z, uv.y * 14.0, irnd * 5.0));
        c = mix(c, M.p2.xyz, M.p2.w * smoothstep(0.25, 0.1, wv.x));
        sf.diff = c * tint; sf.tcol = c * tint * 1.3;
        break; }
    case 5u: { // snow: p0 = (grain scale, bump, dirt, blueness of transmission)
        float3 q = Po * M.p0.x;
        float3 wv = worley(q);
        float3 e1 = worley(q + float3(0.04, 0.0, 0.0)), e2 = worley(q + float3(0.0, 0.04, 0.0)), e3 = worley(q + float3(0.0, 0.0, 0.04));
        g3 = float3(e1.x - wv.x, e2.x - wv.x, e3.x - wv.x) / 0.04 * (-M.p0.y * M.p0.x);
        float4 n = fbmd(Po * M.p0.x * 0.13, 3);
        g3 += n.yzw * M.p0.x * 0.13 * M.p0.y * 3.0;
        float dirt = M.p0.z * smoothstep(0.1, 0.6, fbm(Po * M.p0.x * 0.05 + 3.0, 3));
        float thin = clamp(attr, 0.0, 1.0);                      // thin, wet, icy snow near a melting edge
        sf.diff = mix(M.base.xyz, M.base2.xyz, max(dirt, thin * 0.55)) * tint;
        sf.tcol = float3(0.75, 0.88, 1.0) * tint; sf.trans = M.trans + 0.3 * thin; sf.wrap = 0.45;
        sf.rough = 0.55 - 0.3 * thin; sf.coat = M.coat * (0.6 + 0.8 * wv.z) * (1.0 + thin);
        break; }
    case 6u: { // ice / water with surface relief: p0 = (bump amp, frost amount, rib freq, rib amp), p1.xyz = absorption
        float3 q = Po * M.scale;
        float4 n = fbmd(q, 3);
        g3 = n.yzw * M.scale * M.p0.x;
        if (M.p0.w != 0.0) { float ph = uv.x * M.p0.z + 2.0 * n.x; guv.x = cos(ph) * M.p0.z * M.p0.w; }
        float fr = smoothstep(0.1, 0.5, fbm(q * 0.37 + 5.0, 3) + 0.2) * M.p0.y;
        sf.rough = M.rough + fr;
        sf.sigma = M.p1.xyz;
        break; }
    case 7u: { // bark: p0 = (ridge freq around, stretch along, bump, lichen amount), p2 = lichen colour
        float3 q = float3(cos(uv.y * 2.0 * PI), sin(uv.y * 2.0 * PI), 0.0) * M.p0.x * 0.16 + float3(0.0, 0.0, uv.x * M.p0.y);
        float4 n = fbmd(q + float3(irnd * 9.0), 5);
        float rdg = 1.0 - fabs(n.x) * 2.2;
        float3 c = mix(M.base2.xyz, M.base.xyz, smoothstep(0.2, 0.95, rdg));
        float4 n3 = fbmd(Po * M.scale, 4);
        c *= 0.8 + 0.5 * n3.x;
        float li = smoothstep(0.25, 0.5, fbm(Po * M.scale * 0.5 + 9.0, 3)) * M.p0.w;
        c = mix(c, M.p2.xyz * (0.8 + 0.4 * n3.x), li);
        sf.diff = c * tint;
        g3 = n3.yzw * M.scale * M.p0.z;
        float e = 0.01;
        float4 nu = fbmd(q + float3(irnd * 9.0) + float3(-sin(uv.y * 2.0 * PI), cos(uv.y * 2.0 * PI), 0.0) * e, 5);
        guv.y = (fabs(n.x) - fabs(nu.x)) / e * 2.2 * M.p0.z * 6.0 * 0.16 * M.p0.x * 2.0 * PI;
        sf.rough = 0.8;
        break; }
    case 8u: { // insect hind wing (thin film): uv = (span, chord -1..1); veins carry pigment
        float u = uv.x, v = uv.y;
        float lead = smoothstep(0.80, 0.93, v) * smoothstep(1.0, 0.6, u);
        float vein = lead;
        for (int k = 0; k < 4; k++) {
            float fk = float(k);
            float vv = 0.55 - 0.42 * fk + (0.25 * fk - 0.2) * u * u + 0.03 * sin(u * 9.0 + fk);
            vein = max(vein, smoothstep(0.035, 0.012, fabs(v - vv)) * smoothstep(1.0 - 0.12 * fk, 0.55 - 0.1 * fk, u));
        }
        vein = max(vein, smoothstep(0.03, 0.01, fabs(u - 0.52 - 0.1 * v)) * 0.6);
        sf.diff = mix(M.base.xyz, M.base2.xyz, vein);
        sf.trans = mix(M.trans, 0.95, vein);
        sf.tcol = M.p2.xyz * (0.92 + 0.08 * noised(float3(u * 30.0, v * 12.0, 1.0)).x);
        break; }
    case 9u: { // ladybird elytron: uv = (|x|, z) in mm on the closed beetle; p2 = pale patch colour
        float2 q = uv; float nz = noised(float3(q * 3.0, irnd * 5.0)).x * 0.05;
        float d = length(q - float2(0.0, 1.38)) - 0.50;
        d = min(d, length(q - float2(1.78, 0.42)) - 0.46);
        d = min(d, length((q - float2(0.92, -0.62)) * float2(1.0, 0.88)) - 0.60);
        d = min(d, length(q - float2(1.42, -2.02)) - 0.50);
        float spot = smoothstep(0.05, -0.03, d + nz);
        float pale = smoothstep(0.05, -0.03, length((q - float2(0.60, 1.93)) * float2(1.0, 1.7)) - 0.30 + nz);
        float3 c = M.base.xyz * (0.92 + 0.16 * noised(float3(q * 1.3, 2.0)).x);
        c = mix(c, M.p2.xyz, pale);
        c = mix(c, M.base2.xyz, spot);
        if (!front) { c = mix(c, float3(0.20, 0.07, 0.02), 0.75); sf.coat *= 0.3; sf.rough = 0.4; }
        sf.diff = c; sf.tcol = c * float3(1.4, 0.9, 0.6);
        float4 n = noised(Po * M.scale); g3 = n.yzw * M.scale * M.p0.x;      // fine punctures
        break; }
    case 10u: { // black cuticle with one pale patch (mirrored in x): p0 = (cx, cz, radius, aspect), uv = (|x|, z) mm
        float2 q = uv; float nz = noised(float3(q * 4.0, 3.0)).x * 0.04;
        float pale = smoothstep(0.05, -0.03, length((q - M.p0.xy) * float2(1.0, M.p0.w)) - M.p0.z + nz);
        sf.diff = mix(M.base.xyz, M.base2.xyz, pale);
        float4 n = noised(Po * M.scale); g3 = n.yzw * M.scale * M.p1.x;
        break; }
    case 11u: { // rain streak: adds a little light, otherwise invisible
        float e = sin(PI * clamp(uv.x, 0.0, 1.0));
        sf.emit = M.base.xyz * (0.25 + 0.75 * e) * (0.4 + 0.6 * prnd);
        sf.tcol = float3(1.0); sf.trans = 0.0; sf.coat = 0.0;
        break; }
    case 12u: { // snail shell: uv = (along the spiral 0..1, around the whorl 0..1); bands + growth lines
        float u = uv.x, v = uv.y;
        float gl = sin(u * M.p0.x + 3.0 * noised(float3(u * 60.0, v * 4.0, 1.0)).x);
        guv.x = cos(u * M.p0.x) * M.p0.x * M.p0.y;
        float wob = 0.012 * noised(float3(u * 40.0, 2.0, 3.0)).x;
        float band = 0.0;
        band = max(band, smoothstep(0.035, 0.02, fabs(v - 0.06 + wob)));
        band = max(band, smoothstep(0.022, 0.012, fabs(v - 0.16 + wob)));
        band = max(band, smoothstep(0.030, 0.018, fabs(v - 0.90 + wob)));
        band = max(band, smoothstep(0.018, 0.010, fabs(v - 0.80 - wob)));
        band *= smoothstep(0.12, 0.3, u) * (0.92 + 0.08 * gl);
        float3 c = M.base.xyz * (0.97 + 0.03 * gl) * (0.85 + 0.3 * noised(float3(u * 25.0, v * 6.0, 5.0)).x);
        c = mix(c, M.p2.xyz, smoothstep(0.45, 0.0, u) * 0.6);          // older whorls are paler, worn
        c = mix(c, M.base2.xyz, band * 0.92);
        sf.diff = c; sf.tcol = c * float3(1.3, 1.0, 0.6);
        break; }
    case 13u: { // snail skin: uv = (along body in mm, around 0..1); tubercles in rows, wet
        float3 wv = worley(float3(uv.x * M.p0.x, uv.y * M.p0.y, irnd * 3.0));
        float t = smoothstep(0.05, 0.5, wv.y - wv.x);
        float3 c = mix(M.base2.xyz, M.base.xyz, t) * (0.85 + 0.3 * noised(float3(uv.x * 0.6, uv.y * 5.0, 2.0)).x);
        c = mix(c, M.p2.xyz, smoothstep(0.42, 0.5, fabs(uv.y - 0.5)) * M.p2.w);   // paler foot fringe
        sf.diff = c * tint; sf.tcol = c * float3(1.3, 1.05, 0.8) * tint;
        float3 e1 = worley(float3((uv.x + 0.02) * M.p0.x, uv.y * M.p0.y, irnd * 3.0)), e2 = worley(float3(uv.x * M.p0.x, (uv.y + 0.004) * M.p0.y, irnd * 3.0));
        guv = float2(((e1.y - e1.x) - (wv.y - wv.x)) / 0.02 * 0.001, ((e2.y - e2.x) - (wv.y - wv.x)) / 0.004) * M.p0.z;
        sf.rough = M.rough + 0.1 * (1.0 - t);
        break; }
    case 14u: { // brook surface: dark glossy water with flowing ripples; p0 = (ripple scale, bump, flow speed, ice amount); p1.xy = flow dir
        float2 fl = M.p1.xy * time * M.p0.z;
        float3 q = float3((Pw.x - fl.x) * M.p0.x, time * 0.35, (Pw.z - fl.y) * M.p0.x);
        float4 n = fbmd(q, 3); float4 n2 = fbmd(q * 3.1 + 4.0, 2);
        g3 = (float3(n.y, 0.0, n.w) + 0.5 * float3(n2.y, 0.0, n2.w) * 3.1) * M.p0.x * M.p0.y;
        float ice = smoothstep(0.0, 0.25, fbm(float3(Pw.x * 0.7, 1.0, Pw.z * 0.7), 3) + M.p0.w * 1.4 - 0.7);
        sf.diff = mix(M.base.xyz, M.base2.xyz, ice); sf.rough = mix(M.rough, 0.6, ice); sf.coat = mix(M.coat, 0.4, ice);
        g3 *= (1.0 - ice);
        break; }
    case 15u: { // bark / needles with snow lying on upward faces: p0 = (snow lo, snow hi, snow amount, noise scale)
        float nz = noised(Po * M.p0.w).x;
        float sn = smoothstep(M.p0.x, M.p0.y, No.y + 0.35 * nz) * M.p0.z;
        float3 c = M.base.xyz * (0.75 + 0.5 * noised(Po * M.scale + irnd * 9.0).x) * tint;
        sf.diff = mix(c, float3(0.86, 0.90, 0.95), sn); sf.rough = 0.8; sf.coat = M.coat * (1.0 - sn);
        break; }
    case 16u: { // meadow floor: mottled greens/browns from world position; p0 = (scale, patch contrast, 0, 0)
        float n = fbm(Pw * M.p0.x, 4), n2 = fbm(Pw * M.p0.x * 7.0 + 3.0, 3);
        sf.diff = mix(M.base.xyz, M.base2.xyz, smoothstep(-0.3, 0.3, n + 0.4 * n2)) * (0.8 + 0.4 * n2) * tint;
        break; }
    case 17u: { sf.diff = M.base.xyz; sf.tcol = M.base2.xyz; break; }   // plain, ignores the instance tint
    default: break;
    }
}
