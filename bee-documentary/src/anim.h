// anim.h — 动画小工具：平滑噪声、关键帧插值
#pragma once
// 平滑的一维值噪声（时间上的随机摆动），约 [-1,1]
static float wob(double t, float seed) {
    double f = std::floor(t); float x = (float)(t - f); int i = (int)f;
    auto h = [&](int k) { return hashF((uint32_t)(k * 7919 + (int)(seed * 1000) * 104729 + 12345)) * 2 - 1; };
    float a = h(i - 1), b = h(i), c = h(i + 1), d = h(i + 2);
    float x2 = x * x, x3 = x2 * x;
    return 0.5f * (2 * b + (c - a) * x + (2 * a - 5 * b + 4 * c - d) * x2 + (3 * b - a - 3 * c + d) * x3);
}
struct KF { double t; float v; };
static float kf(double t, std::initializer_list<KF> ks) {
    const KF* k = ks.begin(); int n = (int)ks.size();
    if (t <= k[0].t) return k[0].v;
    for (int i = 0; i + 1 < n; i++) if (t < k[i + 1].t) { float s = (float)((t - k[i].t) / (k[i + 1].t - k[i].t)); s = s * s * (3 - 2 * s); return mix(k[i].v, k[i + 1].v, s); }
    return k[n - 1].v;
}

