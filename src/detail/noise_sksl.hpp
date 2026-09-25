#pragma once

#include <string_view>

namespace ui::detail {

// Skia chrome/m153 exposes ES2 runtime effects. Integer hash operations are
// implemented as exact little-endian byte lanes: each intermediate fits the
// 24-bit precision of float, including multiplication carry accumulation.
inline constexpr std::string_view kNoiseHashSkSL = R"(
float low_byte(float n) { return n - 256.0 * floor(n / 256.0); }
float4 bytes_of_abs(float n) {
    float b3 = floor(n / 16777216.0);
    n -= b3 * 16777216.0;
    float b2 = floor(n / 65536.0);
    n -= b2 * 65536.0;
    float b1 = floor(n / 256.0);
    return float4(n - b1 * 256.0, b1, b2, b3);
}
float4 bytes_of_int(float n) {
    float4 v = bytes_of_abs(abs(n));
    if (n >= 0.0) return v;
    v = 255.0 - v;
    v.x += 1.0;
    float carry = floor(v.x / 256.0);
    v.x -= carry * 256.0;
    v.y += carry;
    carry = floor(v.y / 256.0);
    v.y -= carry * 256.0;
    v.z += carry;
    carry = floor(v.z / 256.0);
    v.z -= carry * 256.0;
    v.w = low_byte(v.w + carry);
    return v;
}
float4 add_one(float4 v) {
    v.x += 1.0;
    float carry = floor(v.x / 256.0);
    v.x -= carry * 256.0;
    v.y += carry;
    carry = floor(v.y / 256.0);
    v.y -= carry * 256.0;
    v.z += carry;
    carry = floor(v.z / 256.0);
    v.z -= carry * 256.0;
    v.w = low_byte(v.w + carry);
    return v;
}
float4 xor32(float4 a, float4 b) {
    float4 result = float4(0.0);
    float place = 1.0;
    for (int i = 0; i < 8; ++i) {
        result += mod(mod(a, 2.0) + mod(b, 2.0), 2.0) * place;
        a = floor(a * 0.5);
        b = floor(b * 0.5);
        place *= 2.0;
    }
    return result;
}
float4 mul32(float4 a, float4 b) {
    float t0 = a.x * b.x;
    float r0 = low_byte(t0);
    float t1 = a.y * b.x + a.x * b.y + floor(t0 / 256.0);
    float r1 = low_byte(t1);
    float t2 = a.z * b.x + a.y * b.y + a.x * b.z + floor(t1 / 256.0);
    float r2 = low_byte(t2);
    float t3 = a.w * b.x + a.z * b.y + a.y * b.z + a.x * b.w + floor(t2 / 256.0);
    return float4(r0, r1, r2, low_byte(t3));
}
float4 rotl17(float4 a) {
    return float4(low_byte(2.0 * a.z + floor(a.y / 128.0)),
                  low_byte(2.0 * a.w + floor(a.z / 128.0)),
                  low_byte(2.0 * a.x + floor(a.w / 128.0)),
                  low_byte(2.0 * a.y + floor(a.x / 128.0)));
}
float4 shr16(float4 a) { return float4(a.z, a.w, 0.0, 0.0); }
float4 shr15(float4 a) {
    return float4(floor(a.y / 128.0) + 2.0 * mod(a.z, 128.0),
                  floor(a.z / 128.0) + 2.0 * mod(a.w, 128.0),
                  floor(a.w / 128.0), 0.0);
}
float4 hash2(float4 seed, float4 x, float4 y) {
    float4 h = xor32(seed, mul32(x, float4(185.0, 121.0, 55.0, 158.0)));
    h = rotl17(h);
    h = xor32(h, mul32(y, float4(107.0, 202.0, 235.0, 133.0)));
    h = xor32(h, shr16(h));
    h = mul32(h, float4(45.0, 53.0, 235.0, 127.0));
    h = xor32(h, shr15(h));
    h = mul32(h, float4(139.0, 166.0, 108.0, 132.0));
    return xor32(h, shr16(h));
}
float u24(float4 h) {
    return (h.y + h.z * 256.0 + h.w * 65536.0) / 16777216.0;
}
)";

inline constexpr std::string_view kValueNoiseKernelSkSL = R"(
uniform float feature_size;
uniform float4 seed_bytes;
layout(color) uniform float4 low_color;
layout(color) uniform float4 high_color;
float fade5(float t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}
float value_noise(float2 p) {
    float x = p.x / feature_size;
    float y = p.y / feature_size;
    if (!(x >= -2147483648.0 && x < 2147483648.0 &&
          y >= -2147483648.0 && y < 2147483648.0)) return 0.5;
    float ix = floor(x);
    float iy = floor(y);
    float4 xb = bytes_of_int(ix);
    float4 yb = bytes_of_int(iy);
    float4 xb1 = add_one(xb);
    float4 yb1 = add_one(yb);
    float v00 = u24(hash2(seed_bytes, xb, yb));
    float v10 = u24(hash2(seed_bytes, xb1, yb));
    float v01 = u24(hash2(seed_bytes, xb, yb1));
    float v11 = u24(hash2(seed_bytes, xb1, yb1));
    float ux = fade5(x - ix);
    float uy = fade5(y - iy);
    float a = v00 + (v10 - v00) * ux;
    float b = v01 + (v11 - v01) * ux;
    return clamp(a + (b - a) * uy, 0.0, 1.0);
}
)";

inline constexpr std::string_view kValueNoiseMainSkSL = R"(
half4 main(float2 p) {
    float v = value_noise(p);
    float4 color = mix(low_color, high_color, v);
    return half4(color.rgb * color.a, color.a);
}
)";

// Frozen T089 Perlin kernel: hash & 7 selects one of eight gradients (S is the
// mathematical 1/sqrt(2)), T088 quintic fade interpolates corner dot products
// and the public scalar is clamp(0.5 + raw / (2*sqrt(2)), 0, 1). ES2-safe: the
// gradient index is selected with if/else, never an array or integer type.
inline constexpr std::string_view kPerlinNoiseKernelSkSL = R"(
uniform float feature_size;
uniform float4 seed_bytes;
layout(color) uniform float4 low_color;
layout(color) uniform float4 high_color;
float fade5(float t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}
float2 gradient_of(float4 h) {
    float gi = mod(h.x, 8.0);
    if (gi < 0.5) return float2(1.0, 0.0);
    if (gi < 1.5) return float2(-1.0, 0.0);
    if (gi < 2.5) return float2(0.0, 1.0);
    if (gi < 3.5) return float2(0.0, -1.0);
    if (gi < 4.5) return float2(0.707106781186547524400844362104849,
                               0.707106781186547524400844362104849);
    if (gi < 5.5) return float2(-0.707106781186547524400844362104849,
                                0.707106781186547524400844362104849);
    if (gi < 6.5) return float2(0.707106781186547524400844362104849,
                                -0.707106781186547524400844362104849);
    return float2(-0.707106781186547524400844362104849,
                  -0.707106781186547524400844362104849);
}
float perlin_noise(float2 p) {
    float x = p.x / feature_size;
    float y = p.y / feature_size;
    if (!(x >= -2147483648.0 && x < 2147483648.0 &&
          y >= -2147483648.0 && y < 2147483648.0)) return 0.5;
    float ix = floor(x);
    float iy = floor(y);
    float fx = x - ix;
    float fy = y - iy;
    float4 xb = bytes_of_int(ix);
    float4 yb = bytes_of_int(iy);
    float4 xb1 = add_one(xb);
    float4 yb1 = add_one(yb);
    float d00 = dot(gradient_of(hash2(seed_bytes, xb, yb)), float2(fx, fy));
    float d10 = dot(gradient_of(hash2(seed_bytes, xb1, yb)),
                    float2(fx - 1.0, fy));
    float d01 = dot(gradient_of(hash2(seed_bytes, xb, yb1)),
                    float2(fx, fy - 1.0));
    float d11 = dot(gradient_of(hash2(seed_bytes, xb1, yb1)),
                    float2(fx - 1.0, fy - 1.0));
    float ux = fade5(fx);
    float uy = fade5(fy);
    float r0 = d00 + (d10 - d00) * ux;
    float r1 = d01 + (d11 - d01) * ux;
    float raw = r0 + (r1 - r0) * uy;
    return clamp(0.5 + raw / 2.8284271247461900976033774484194, 0.0, 1.0);
}
)";

inline constexpr std::string_view kPerlinNoiseMainSkSL = R"(
half4 main(float2 p) {
    float v = perlin_noise(p);
    float4 color = mix(low_color, high_color, v);
    return half4(color.rgb * color.a, color.a);
}
)";

} // namespace ui::detail
