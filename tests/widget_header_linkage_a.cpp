#include <nativeui/list_view.hpp>
#include <nativeui/form.hpp>
#include <nativeui/field.hpp>
#include <nativeui/fieldset.hpp>
#include <nativeui/find_bar.hpp>
#include <nativeui/editable_text.hpp>
#include <nativeui/search_field.hpp>
#include <nativeui/number_input.hpp>
#include <nativeui/tabs.hpp>
#include <nativeui/if.hpp>
#include <nativeui/switch.hpp>
#include <nativeui/for_each.hpp>
#include <nativeui/icon_view.hpp>
#include <nativeui/avatar.hpp>
#include <nativeui/link.hpp>
#include <nativeui/accordion.hpp>
#include <nativeui/collapsible.hpp>
#include <nativeui/rating.hpp>
#include <nativeui/stepper.hpp>
#include <nativeui/split_view.hpp>
#include <nativeui/toggle_button.hpp>
#include <nativeui/radio_button.hpp>
#include <nativeui/row.hpp>
#include <nativeui/column.hpp>
#include <nativeui/grid.hpp>
#include <nativeui/scroll.hpp>
#include <nativeui/scroll_view.hpp>
#include <nativeui/clip.hpp>
#include <nativeui/flex.hpp>
#include <nativeui/spacer.hpp>
#include <nativeui/stack.hpp>
#include <nativeui/padding.hpp>
#include <nativeui/visibility.hpp>
#include <nativeui/enabled.hpp>
#include <nativeui/read_only.hpp>
#include <nativeui/focus_scope.hpp>
#include <nativeui/command_scope.hpp>
#include <nativeui/style_scope.hpp>
#include <nativeui/label.hpp>
#include <nativeui/header.hpp>
#include <nativeui/canvas.hpp>
#include <nativeui/knob.hpp>
#include <nativeui/toggle.hpp>
#include <nativeui/button.hpp>
#include <nativeui/checkbox.hpp>
#include <nativeui/slider.hpp>
#include <nativeui/range_slider.hpp>
#include <nativeui/text_input.hpp>
#include <nativeui/text_area.hpp>
#include <nativeui/progress_bar.hpp>
#include <nativeui/meter.hpp>
#include <nativeui/divider.hpp>
#include <nativeui/badge.hpp>
#include <nativeui/image_view.hpp>
#include <nativeui/spinner.hpp>

ui::Spec widget_headers_forward() {
  return ui::make_spec(ui::Row{
    ui::Label{"En-tête"}.bold(),
    ui::Header{"Compatible"},
    ui::Button{"Valider", [] {}},
  });
}
