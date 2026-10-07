#include <nativeui/canvas.hpp>

#include <stdexcept>

namespace ui {

CanvasComponent::CanvasComponent(Size size, DrawCallback draw,
                                 InputCallback input, bool focusable)
    : size_(size), draw_(std::move(draw)), input_(std::move(input)),
      focusable_(focusable) {
  if (std::isinf(size_.w) && size_.w > 0.0f)
    throw std::invalid_argument("Canvas width must not be positive infinity");
  if (std::isinf(size_.h) && size_.h > 0.0f)
    throw std::invalid_argument("Canvas height must not be positive infinity");
  size_.w = std::max(1.0f, size_.w);
  size_.h = std::max(1.0f, size_.h);
}

bool CanvasComponent::focusable() const noexcept { return focusable_; }

Size CanvasComponent::measure(const std::vector<ChildMetrics> &) const {
  return size_;
}

EventResult CanvasComponent::input(const InputEvent &event,
                                   InputContext &context) {
  auto callback = input_;
  if (!callback)
    return EventResult::Ignored;

  InputEvent local = event;
  if (event.type == InputType::PointerDown ||
      event.type == InputType::PointerMove ||
      event.type == InputType::PointerUp ||
      event.type == InputType::PointerWheel ||
      event.type == InputType::DropOffer || event.type == InputType::DropData) {
    const auto bounds = context.bounds();
    local.position.x -= bounds.x;
    local.position.y -= bounds.y;
  }

  CanvasInputContext canvas_context{context};
  return callback(local, canvas_context);
}

void CanvasComponent::paint(PaintContext &context) const {
  auto callback = draw_;
  if (!callback)
    return;

  auto &painter = context.painter();
  auto clip_scope = painter.scoped_clip(context.bounds());
  {
    auto local_scope = painter.scoped_state();
    painter.translate(context.bounds().x, context.bounds().y);
    CanvasContext2D canvas_context{
        painter, Rect{0.0f, 0.0f, context.bounds().w, context.bounds().h},
        context.focused()};
    callback(canvas_context);
  }
}

Canvas::Canvas(Size size, DrawCallback draw)
    : size_(size), draw_(std::move(draw)) {}

Canvas::Canvas(float width, float height, DrawCallback draw)
    : Canvas(Size{width, height}, std::move(draw)) {}

Canvas &&Canvas::focusable(bool value) && {
  focusable_ = value;
  return std::move(*this);
}

Spec Canvas::spec() && {
  const auto size = size_;
  auto draw = std::move(draw_);
  auto input = std::move(input_);
  const bool focusable = focusable_;

  return Spec{[size, draw = std::move(draw), input = std::move(input),
               focusable]() mutable {
                return std::make_unique<CanvasComponent>(
                    size, std::move(draw), std::move(input), focusable);
              },
              {}};
}

} // namespace ui
