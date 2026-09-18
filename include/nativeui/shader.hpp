#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ui {

enum class ShaderCompileError {
    None,
    CompileError,
    UnsupportedInterface,
};

struct ShaderDiagnostic {
    ShaderCompileError code{};
    int line{};
    int column{};
    std::string message;
};

class ShaderProgram;

struct ShaderCompileResult {
    std::shared_ptr<const ShaderProgram> program;
    std::vector<ShaderDiagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return program != nullptr;
    }
};

namespace detail {
struct ShaderProgramData;
} // namespace detail

/// Immutable compiled runtime-shader program.
///
/// Compilation is explicit resource-preparation work. It may allocate and is
/// not an audio-real-time operation. The returned program owns its backend
/// representation and does not borrow the caller's SkSL source. Rendering never
/// invokes compilation implicitly. Backend/compiler types remain private to
/// NativeUI.
class ShaderProgram final {
public:
    ShaderProgram() = delete;
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) = delete;
    ShaderProgram& operator=(ShaderProgram&&) = delete;
    ~ShaderProgram() noexcept;

    [[nodiscard]] static ShaderCompileResult compile(std::string_view sksl);

private:
    explicit ShaderProgram(std::unique_ptr<const detail::ShaderProgramData> data) noexcept;

    std::unique_ptr<const detail::ShaderProgramData> data_;
};

} // namespace ui
