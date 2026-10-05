# Advanced rendering foundations

This guide documents the NativeUI 1.1 rendering foundations already exposed by the public API. Skia and Pugl remain implementation details; normal users should integrate through NativeUI types only.

Public declarations used here are in [paint styles](../include/nativeui/paint_style.hpp), [painting](../include/nativeui/paint.hpp), and [runtime shaders](../include/nativeui/shader.hpp).

## 1. Rendering model and scope

NativeUI is retained-mode and paints in two logical dimensions. Components render through `ui::Painter`; drawing coordinates, gradients and runtime-shader coordinates are Painter-local and follow the active transform.

The supported rendering contract documented here is backend-neutral. `Color`, `Brush`, `Effect`, `PaintOptions`, `ShaderProgram` and `ShaderInstance` are NativeUI API. Some current low-level Painter declarations still expose backend C++ escape hatches; those are not a portable NativeUI integration contract and are intentionally not taught in this guide. Backend compiler objects remain implementation details.

This layer is not a general 3D scene API. Perspective, depth-buffer behavior and physically based material/lighting contracts are outside this documentation slice.

## 2. Brush, colors and gradients

A `ui::Brush` can hold a `ui::Color`, `ui::LinearGradient`, `ui::RadialGradient`, `ui::ImageTexture`, or a snapshot of a `ui::ShaderInstance`. This slice covers colors, gradients and runtime-shader snapshots; the detailed ImageTexture tiling, sampling and interpretation contract belongs to the later texture documentation slice. `ui::PaintOptions` supplies opacity and a public blend mode: `SourceOver`, `Multiply`, `Screen`, or `Plus`.

Gradient stops use `ui::GradientStop`. Linear gradients use two Painter-local points. Radial gradients use a Painter-local center and radius. The same Brush surface is shared by fills and strokes; a multi-segment Path does not remap a gradient independently for each segment.

Canonical gradient fill:

~~~cpp
void paint_header(ui::Painter& painter)
{
    ui::LinearGradient gradient{
        {0.0f, 0.0f},
        {240.0f, 0.0f},
        {
            {0.0f, {0.10f, 0.14f, 0.22f, 1.0f}},
            {1.0f, {0.42f, 0.58f, 0.90f, 1.0f}},
        }};

    painter.fill_rounded_rect(
        {0.0f, 0.0f, 240.0f, 48.0f},
        8.0f,
        ui::Brush{gradient});
}
~~~

Brushes are also accepted by the public stroke overloads, including Path, line, arc and rounded-rectangle strokes.

## 3. Clips, layers and effects

Scoped Painter state is owned by `ui::Painter::StateGuard`. Keep the returned guard alive for exactly the lexical region that should own the state.

~~~cpp
auto clip = painter.scoped_clip(bounds);
auto rounded = painter.scoped_clip(bounds, 12.0f);
auto path_clip = painter.scoped_clip(path);
~~~

Clips intersect with the existing clip chain. Invalid or non-finite clip geometry fails closed to an empty effective clip.

A bounded compositing layer uses `scoped_layer`:

~~~cpp
auto layer = painter.scoped_layer(
    bounds,
    ui::PaintOptions{.opacity = 0.8f, .blend = ui::BlendMode::SourceOver});
~~~

NativeUI enforces a hard drawing boundary for the layer. Opacity and blend apply once to the composed group.

Effects use the same layer scope:

~~~cpp
const auto blur = ui::Effect::gaussian_blur(6.0f, 6.0f);
auto blurred = painter.scoped_layer(bounds, blur);

const auto shadow = ui::Effect::drop_shadow(
    {3.0f, 5.0f},
    8.0f,
    {0.0f, 0.0f, 0.0f, 0.45f});
auto shadowed = painter.scoped_layer(bounds, shadow);
~~~

`ui::Effect::drop_shadow_only` keeps only the shadow contribution. `Effect::visual_outset()` exposes the conservative logical expansion required by an effect, so retained invalidation can include output beyond the source bounds.

Scopes restore in strict lexical LIFO order, including exception unwinding. `StateGuard` is intentionally non-copyable and non-movable.

## 4. SkSL runtime shaders

SkSL is a public source format in NativeUI. Skia C++ API/ABI is not a supported portable NativeUI integration contract; use NativeUI shader types for normal application code.

Shader compilation is explicit preparation work through `ui::ShaderProgram::compile()`. It may allocate and is not audio-real-time work. Rendering a shader Brush does not implicitly compile its source.

`ui::ShaderInstance` is mutable preparation state, not a synchronized shared object. Numeric uniform setters are allocation-free/noexcept; `set_child()` snapshots a Brush and may allocate. Do not mutate one instance concurrently from multiple threads. Publish a `ui::Brush{instance}` snapshot before sharing retained rendering state.

Canonical compile, uniform binding and Brush snapshot:

~~~cpp
ui::Brush make_meter_brush()
{
    constexpr std::string_view source = R"(
        uniform float gain;
        layout(color) uniform half4 tint;

        half4 main(float2 p) {
            return half4(tint.rgb * gain, tint.a);
        }
    )";

    auto compiled = ui::ShaderProgram::compile(source);
    if (!compiled.ok())
        return ui::Brush{ui::Color{0.0f, 0.0f, 0.0f, 0.0f}};

    ui::ShaderInstance instance{compiled.program};
    if (instance.set_float("gain", 0.75f) != ui::ShaderSetResult::Ok)
        return ui::Brush{ui::Color{0.0f, 0.0f, 0.0f, 0.0f}};
    if (instance.set_color("tint", {0.20f, 0.75f, 1.0f, 1.0f}) !=
        ui::ShaderSetResult::Ok)
        return ui::Brush{ui::Color{0.0f, 0.0f, 0.0f, 0.0f}};

    return ui::Brush{instance};
}
~~~

A successful compile returns an immutable shared program and no diagnostics. Failures return no program and diagnostic records.

The supported reflected uniform profile contains scalar/vector float or half values, scalar/vector int values, and four-component `layout(color)` uniforms. Matrices and uniform arrays are outside this profile.

Shader-child composition uses `uniform shader` plus `ShaderInstance::set_child()`. Child binding snapshots the supplied Brush. Creating `ui::Brush{instance}` snapshots the instance's logical bindings; later mutation or destruction of the source instance does not mutate the existing Brush. Nested shader children are bounded by `ShaderInstance::kMaxChildDepth`.

### Performance boundary

NativeUI documents where compilation and logical binding work occurs, but does **not** claim that arbitrary user-supplied SkSL will meet a hard 60 FPS target. Cost depends on source complexity, coverage, backend, device and composition depth. Compile and prepare custom shaders outside steady-state painting and measure representative workloads.
