#include "example_support.hpp"
#include <nativeui/detail/dispatcher_owner.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace std::chrono_literals;
constexpr ui::Color kAccent{0.27f, 0.76f, 0.72f, 1.0f};
constexpr ui::Color kMuted{0.58f, 0.65f, 0.74f, 1.0f};
constexpr ui::Color kSurface{0.085f, 0.11f, 0.16f, 1.0f};
constexpr std::array<std::string_view, 8> kPages{
    "Contrôles & actions", "Saisie & formulaires",
    "Dates & couleurs",    "Collections",
    "Conteneurs",          "Composition",
    "Texte & dessin",      "Messages & indicateurs"};

// A small example-only adapter for public factories which already return Spec.
// The retained tree owns the resulting components; this adapter owns no runtime
// state.
struct View {
  ui::Spec value;
  ui::Spec spec() && { return std::move(value); }
};

template <class Child> auto frame(float height, Child &&child) {
  return ui::Grid{
      ui::GridTracks{{ui::Track::flex()}, {ui::Track::fixed(height)}},
      std::forward<Child>(child)};
}
template <class Child> auto card(std::string title, Child &&child) {
  ui::FieldsetStyle style;
  style.padding = 18.0f;
  style.row_gap = 12.0f;
  style.legend_gap = 14.0f;
  style.background = kSurface;
  style.border_color = ui::Color{0.17f, 0.22f, 0.30f, 1.0f};
  style.corner_radius = 12.0f;
  return ui::Fieldset{std::move(title), std::forward<Child>(child)}.style(
      style);
}
template <class Left, class Right> auto pair(Left &&left, Right &&right) {
  return ui::Grid{ui::GridTracks{{ui::Track::flex(), ui::Track::flex()},
                                 {ui::Track::auto_size()}},
                  std::forward<Left>(left), std::forward<Right>(right)}
      .gap(18.0f);
}
auto hint(std::string text) {
  return ui::Label{std::move(text)}.size(12.0f).color(kMuted);
}

ui::SvgIcon make_icon() {
  constexpr std::string_view source =
      R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="120" height="120" viewBox="0 0 120 120"><rect width="120" height="120" rx="28" fill="#152b3d"/><path d="M20 77 L20 43 L43 77 L43 43 M57 43 L57 69 Q57 79 68 79 Q79 79 79 69 L79 43" fill="none" stroke="#45c2b8" stroke-width="7" stroke-linecap="round"/><circle cx="96" cy="47" r="5" fill="#a8d9ef"/><path d="M96 60 L96 78" stroke="#a8d9ef" stroke-width="7" stroke-linecap="round"/></svg>)svg";
  const auto *data = reinterpret_cast<const std::byte *>(source.data());
  return ui::SvgIcon::parse(std::span<const std::byte>{data, source.size()});
}
ui::SvgIcon make_symbol() {
  constexpr std::string_view svg =
      R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32"><path d="M6 8h8v8H6z M18 8h8v8h-8z M6 20h8v6H6z M18 20h8v6h-8z" fill="#45c2b8"/></svg>)svg";
  const auto *bytes = reinterpret_cast<const std::byte *>(svg.data());
  return ui::SvgIcon::parse(std::span<const std::byte>{bytes, svg.size()});
}
std::vector<ui::CollectionItem<int>> make_items() {
  std::vector<ui::CollectionItem<int>> rows;
  for (int i = 1; i <= 120; ++i)
    rows.push_back({i, "Composant " + std::to_string(i), true, false});
  return rows;
}
std::vector<ui::TreeNode<int>> make_nodes() {
  return {{1, {}, "Projet NativeUI", true, true},
          {2, 1, "include/nativeui", true, true},
          {3, 2, "button.hpp"},
          {4, 2, "slider.hpp"},
          {5, 1, "src", true, true},
          {6, 5, "button.cpp"},
          {7, 5, "slider.cpp"},
          {8, 1, "docs", true, true},
          {9, 8, "widgets.md"},
          {10, 8, "widgets_demo.md"}};
}
struct Model {
  ui::State<int> page{0};
  ui::State<std::optional<int>> navigation{0};
  ui::State<std::string> status{"Prêt — explorez les composants"};
  ui::State<bool> checked{true}, notify{true}, export_audio{true},
      export_midi{false};
  ui::State<bool> bold{true}, italic{false}, underline{false}, bypass{false};
  ui::State<int> radio{1};
  ui::RadioGroup<int> radio_group{radio};
  ui::State<int> segment{0}, combo{1}, collection_tab{0};
  ui::State<float> value{0.62f}, knob{0.42f};
  ui::State<ui::RangeValue> range{ui::RangeValue{0.2f, 0.8f}};
  ui::State<double> number{12.5}, rating{3.5};
  ui::State<std::string> name{"Mon premier projet"},
      notes{"NativeUI : une interface native, construite en C++.\nNativeUI "
            "conserve les composants et leurs états.\nSélectionnez le texte, "
            "ajoutez des lignes, essayez les raccourcis."};
  ui::State<std::string> query{"NativeUI"}, font{"Inter"}, city{""},
      filename{"Projet.nativeui"};
  ui::State<std::vector<std::string>> tokens{
      std::vector<std::string>{"C++", "Interface"}};
  ui::State<bool> find_open{true};
  ui::State<std::size_t> matches{0};
  ui::State<std::optional<std::size_t>> match{std::size_t{0}};
  std::shared_ptr<ui::EditableTextController> rename{
      std::make_shared<ui::EditableTextController>()};
  ui::State<ui::Calendar::Value> date{
      std::chrono::sys_days{std::chrono::year{2026} / 10 / 5}};
  ui::State<ui::TimeInput::Value> time{
      std::chrono::seconds{9 * 3600 + 30 * 60}};
  ui::State<ui::Color> color{kAccent};
  ui::State<std::vector<ui::CollectionItem<int>>> items{make_items()};
  ui::State<std::vector<ui::TreeNode<int>>> nodes{make_nodes()};
  ui::State<ui::SelectionSnapshot<int>> table_selection{}, grid_selection{},
      tree_selection{}, outline_selection{}, outline_table_selection{};
  ui::Selection<int> table_selected{table_selection},
      grid_selected{grid_selection}, tree_selected{tree_selection},
      outline_selected{outline_selection},
      outline_table_selected{outline_table_selection};
  ui::State<std::vector<int>> tree_expanded{std::vector<int>{1, 2, 5, 8}},
      outline_expanded{std::vector<int>{1, 2}},
      outline_table_expanded{std::vector<int>{1, 5}};
  ui::State<std::optional<ui::SortOrder>> sort{std::nullopt};
  ui::State<ui::TableLayout> table_layout{};
  ui::OutlineState<int> outline_state;
  ui::GridViewState<int> grid_state;
  ui::State<std::optional<int>> list_selection{1};
  ui::State<std::vector<ui::BreadcrumbItem>> path{
      std::vector<ui::BreadcrumbItem>{{"home", "Galerie"},
                                      {"collections", "Collections"},
                                      {"project", "Projet"}}};
  ui::State<bool> can_back{true};
  ui::State<std::vector<ui::HistoryEntry>> history{
      std::vector<ui::HistoryEntry>{{"controls", "Contrôles"},
                                    {"input", "Saisie"},
                                    {"colors", "Couleurs"}}};
  ui::State<double> split{250.0};
  ui::State<bool> disclosure{true};
  ui::State<std::vector<std::string>> accordion{
      std::vector<std::string>{"first"}};
  ui::State<bool> visible{true}, enabled{true}, read_only{false},
      conditional{true}, focus_scope{false};
  ui::State<int> branch{0};
  ui::State<std::vector<int>> chips{std::vector<int>{1, 2, 3}};
  ui::State<bool> popover{false}, spinning{true};
  std::array<std::unique_ptr<ui::ScrollState>, 8> scroll;
  ui::ScrollState horizontal{ui::ScrollAxis::Horizontal},
      plain_scroll{ui::ScrollAxis::Vertical};
  ui::SvgIcon icon{make_icon()}, symbol{make_symbol()};
  ui::Dialog *dialogs{};
  ui::Toast *toasts{};
  int clicks{}, strokes{}, dialog_completions{};
  int next_row_id{121};
  ui::Binding<std::string>::Subscription query_subscription, notes_subscription;
  Model() {
    for (auto &state : scroll)
      state = std::make_unique<ui::ScrollState>(ui::ScrollAxis::Both);
    query_subscription =
        query.observe([this](const auto &) { update_matches(); });
    notes_subscription =
        notes.observe([this](const auto &) { update_matches(); });
    update_matches();
  }
  void update_matches() {
    const auto needle = query.get();
    const auto text = notes.get();
    std::size_t count{};
    if (!needle.empty())
      for (auto at = text.find(needle); at != std::string::npos;
           at = text.find(needle, at + needle.size()))
        ++count;
    matches.set(count);
    if (count == 0)
      match.set(std::nullopt);
    else if (!match.get() || *match.get() >= count)
      match.set(0);
  }
  void add_row() {
    auto rows = items.get();
    const auto id = next_row_id;
    rows.push_back({id, "Nouvel élément " + std::to_string(id)});
    items.set(std::move(rows));
    ++next_row_id;
  }
  void sort_rows(const ui::SortOrder &order) {
    auto rows = items.get();
    std::sort(rows.begin(), rows.end(), [&order](const auto &a, const auto &b) {
      const bool ascending = order.direction == ui::SortDirection::Ascending;
      if (order.column == "id")
        return ascending ? a.key < b.key : a.key > b.key;
      return ascending ? a.label < b.label : a.label > b.label;
    });
    items.set(std::move(rows));
    sort.set(order);
  }
  void report(std::string message) { status.set(std::move(message)); }
  void select_page(int selected) {
    if (selected < 0 || selected >= static_cast<int>(kPages.size()))
      return;
    page.set(selected);
    navigation.set(selected);
    report(std::string{kPages[static_cast<std::size_t>(selected)]});
  }
  void add_chip() {
    auto next = chips.get();
    next.push_back(next.empty() ? 1 : next.back() + 1);
    chips.set(std::move(next));
  }
  void open_dialog() {
    if (!dialogs)
      return;
    const auto shown = dialogs->show_alert(
        {.title = "Enregistrer le projet ?",
         .message = "Cette démonstration conserve vos modifications dans la "
                    "fenêtre courante.",
         .actions = {{"save", "Enregistrer", true,
                      ui::DialogActionRole::Default},
                     {"cancel", "Annuler", true,
                      ui::DialogActionRole::Cancel}}},
        [this](ui::DialogResult result) {
          ++dialog_completions;
          report(result.action_id == "save" ? "Projet enregistré"
                                            : "Dialogue fermé");
        });
    if (shown != ui::DialogShowResult::Shown)
      report("Un dialogue est déjà ouvert");
  }
  void notify_toast() {
    if (toasts)
      (void)toasts->show(
          {.message = "Votre projet est enregistré",
           .action_label = "Compris",
           .action = [this] { report("Notification confirmée"); }});
  }
};

std::vector<ui::PopupMenuItem> menu(Model &m) {
  return {ui::PopupMenuItem::action("Enregistrer", [&m] { m.notify_toast(); }),
          ui::PopupMenuItem::action("Modifier le projet",
                                    [&m] { m.select_page(1); }),
          ui::PopupMenuItem::separator(),
          ui::PopupMenuItem::action("Explorer les couleurs",
                                    [&m] { m.select_page(2); })};
}

ui::Spec controls(Model &m) {
  return ui::Column{
      pair(card("Button · Link · PopupMenu · ContextMenu",
                ui::Column{
                    ui::Row{ui::Button{"Action principale",
                                       [&m] {
                                         ++m.clicks;
                                         m.report("Activation " +
                                                  std::to_string(m.clicks));
                                       }},
                            ui::PopupMenu{"Plus d’actions", menu(m)}}
                        .gap(10.0f),
                    ui::Link{"Explorer les formulaires →", "input",
                             [&m](const auto &) { m.select_page(1); }},
                    View{ui::ContextMenu{
                        ui::Padding{
                            14.0f,
                            ui::Label{"Clic droit ici pour ouvrir le menu"}}
                            .spec(),
                        menu(m)}
                             .spec()}}
                    .padding(0)
                    .gap(14)
                    .align(ui::Align::Stretch)),
           card("ToggleButton · ToggleGroup · SegmentedControl",
                ui::Column{
                    View{ui::ToggleGroup{
                        "Mise en forme",
                        {ui::ToggleButton{"Gras", m.bold}.spec(),
                         ui::ToggleButton{"Italique", m.italic}.spec(),
                         ui::ToggleButton{"Souligné", m.underline}.spec()}}
                             .spec()},
                    ui::SegmentedControl<int>{
                        "Affichage",
                        m.segment,
                        {{0, "Liste"}, {1, "Grille"}, {2, "Détails"}}},
                    hint("Sélection par clic ou flèches du clavier")}
                    .padding(0)
                    .gap(14)
                    .align(ui::Align::Stretch))),
      pair(
          card("Checkbox · CheckboxGroup · RadioButton · Toggle",
               ui::Column{ui::Checkbox{m.checked, "Recevoir les notifications"},
                          ui::CheckboxGroup{
                              "Formats à exporter",
                              {{"audio", "Audio", m.export_audio.binding()},
                               {"midi", "MIDI", m.export_midi.binding()}}},
                          ui::Row{ui::RadioButton{m.radio_group, 1, "Standard"},
                                  ui::RadioButton{m.radio_group, 2, "Avancé"}}
                              .gap(14),
                          ui::Toggle{"Bypass", m.bypass}}
                   .padding(0)
                   .gap(14)),
          card(
              "ComboBox · Slider · RangeSlider · Knob",
              ui::Column{ui::ComboBox<int>{
                             m.combo,
                             {{1, "Équilibré"}, {2, "Doux"}, {3, "Intense"}}},
                         ui::Slider{m.value}.step(0.01f),
                         ui::RangeSlider{m.range}.step(0.01f),
                         ui::Row{ui::Knob{"Intensité", m.knob},
                                 ui::Column{hint("Curseur et progression liés"),
                                            ui::ProgressBar{m.value}}
                                     .padding(0)
                                     .gap(12)}
                             .gap(18)}
                  .padding(0)
                  .gap(16)
                  .align(ui::Align::Stretch))),
      card("Stepper · NumberInput · Rating",
           ui::Row{
               ui::Column{hint("Réglage numérique partagé"),
                          ui::NumberInput{"Quantité", m.number}
                              .range(0, 100)
                              .step(0.5)
                              .precision(1)}
                   .padding(0)
                   .gap(8),
               ui::Stepper{m.number}.label("Quantité").range(0, 100).step(0.5),
               ui::Column{hint("Votre évaluation"),
                          ui::Rating{"Évaluation", m.rating}.step(0.5)}
                   .padding(0)
                   .gap(8)}
               .gap(24)
               .align(ui::Align::Center))}
      .padding(0)
      .gap(18)
      .align(ui::Align::Stretch)
      .spec();
}

ui::Spec inputs(Model &m) {
  return ui::Column{
      pair(card("Form · Field · Fieldset · TextInput · NumberInput",
                ui::Form{
                    ui::Field{"Nom du projet", ui::TextInput{"", m.name}
                                                   .placeholder("Votre projet")
                                                   .max_length(120)}
                        .required()
                        .description("Une valeur partagée avec l’avatar"),
                    ui::Field{
                        "Quantité",
                        ui::NumberInput{"", m.number}.range(0, 100).step(0.5)},
                    ui::Field{"Police",
                              ui::EditableComboBox{
                                  "", m.font,
                                  std::vector<std::string>{"Inter", "Georgia",
                                                           "Menlo", "Arial"}}},
                    ui::Field{"Ville",
                              ui::Autocomplete{"", m.city,
                                               std::vector<std::string>{
                                                   "Paris", "Lyon", "Nantes",
                                                   "Bordeaux", "Toulouse"}}
                                  .placeholder("Commencez à écrire")}}
                    .layout(ui::FormLayout::Responsive)
                    .stacked_below(370)
                    .on_submit([&m] { m.report("Formulaire validé"); })),
           card("SearchField · TokenField · EditableText",
                ui::Column{ui::SearchField{"Recherche", m.query}
                               .placeholder("Rechercher un composant")
                               .on_submit([&m](const auto &text) {
                                 m.report("Recherche : " + text);
                               }),
                           hint("Tags : Entrée ajoute, Retour arrière retire"),
                           ui::TokenField{"Tags",
                                          m.tokens,
                                          {"C++", "Interface", "Audio",
                                           "Graphisme", "Natif"}}
                               .maximum_tokens(8)
                               .placeholder("Ajouter un tag"),
                           ui::EditableText{"Nom de fichier", m.filename}
                               .controller(m.rename)
                               .select_stem()
                               .validator([](std::string_view value)
                                              -> std::optional<std::string> {
                                 if (value.empty())
                                   return "Un nom est requis";
                                 return {};
                               }),
                           ui::Button{"Renommer le fichier",
                                      [&m] { m.rename->begin(); }}}
                    .padding(0)
                    .gap(14)
                    .align(ui::Align::Stretch))),
      card(
          "TextArea · FindBar",
          ui::Column{
              ui::Row{hint("Notes multiligne · sélection · copier/coller"),
                      ui::Button{"Afficher la recherche",
                                 [&m] { m.find_open.set(true); }}}
                  .gap(20),
              ui::FindBar{m.find_open, m.query, m.matches, m.match}.on_navigate(
                  [&m](std::size_t index) {
                    m.report("Résultat " + std::to_string(index + 1));
                  }),
              frame(185, ui::TextArea{"Notes", m.notes})}
              .padding(0)
              .gap(12)
              .align(ui::Align::Stretch))}
      .padding(0)
      .gap(18)
      .align(ui::Align::Stretch)
      .spec();
}

ui::Spec dates(Model &m) {
  const auto reference =
      std::chrono::sys_days{std::chrono::year{2026} / 10 / 5};
  const std::vector<ui::ColorSwatch> swatches{
      {"mint", "Menthe", kAccent},
      {"sky", "Ciel", {0.3f, 0.6f, 0.95f, 1}},
      {"orange", "Abricot", {0.98f, 0.6f, 0.32f, 1}},
      {"purple", "Lavande", {0.68f, 0.48f, 0.95f, 1}}};
  return pair(card("Calendar · DateInput · TimeInput",
                   ui::Column{hint("Le calendrier et le champ de date "
                                   "partagent leur sélection"),
                              ui::Calendar{"Calendrier du projet", m.date}
                                  .reference_day(reference)
                                  .today(reference),
                              ui::DateInput{"Date du projet", m.date}
                                  .reference_day(reference)
                                  .clearable(),
                              ui::TimeInput{"Heure du projet", m.time}
                                  .show_seconds()
                                  .clearable()}
                       .padding(0)
                       .gap(18)
                       .align(ui::Align::Stretch)),
              card("ColorPicker · ColorWell",
                   ui::Column{
                       hint("Canaux, transparence, hexadécimal et nuancier"),
                       ui::Grid{ui::GridTracks{{ui::Track::fixed(280)},
                                               {ui::Track::auto_size()}},
                                ui::ColorPicker{"Couleur d’accent", m.color}
                                    .alpha_enabled()
                                    .swatches(swatches)},
                       ui::Row{hint("Ouvrir le sélecteur :"),
                               ui::ColorWell{"Couleur du projet", m.color}
                                   .swatches(swatches)}
                           .gap(12)
                           .align(ui::Align::Center)}
                       .padding(0)
                       .gap(18)
                       .align(ui::Align::Stretch)))
      .spec();
}

std::vector<ui::TableColumn> columns() {
  return {{"name", "Nom", 245, 100, {}, ui::Align::Start, true, false},
          {"id", "Identifiant", 125, 70}};
}
ui::Spec collections(Model &m) {
  auto activated = [&m](const int &key) {
    m.report("Élément activé : " + std::to_string(key));
  };
  return ui::Column{
      ui::Row{ui::HistoryButton{ui::HistoryDirection::Backward, m.can_back,
                                [&m](int steps) {
                                  m.report("Retour de " +
                                           std::to_string(-steps) +
                                           " étape(s)");
                                }}
                  .entries(m.history),
              ui::Breadcrumbs{m.path}
                  .label("Chemin du projet")
                  .on_navigate([&m](const auto &key) {
                    m.report("Destination : " + key);
                  }),
              ui::Button{"Ajouter une ligne", [&m] { m.add_row(); }}}
          .gap(12)
          .align(ui::Align::Center),
      hint("120 éléments · sélection, navigation clavier, défilement et tri "
           "des colonnes"),
      frame(
          370,
          ui::Tabs<int>{m.collection_tab}
              .tab(0, "TableView",
                   ui::TableView<int>{m.items, m.table_selected}
                       .columns(columns())
                       .layout(m.table_layout)
                       .sort(m.sort)
                       .on_sort_request([&m](const ui::SortOrder &order) {
                         m.sort_rows(order);
                       })
                       .cell([](const auto &row, const auto &col) {
                         return ui::Label{col == "id" ? std::to_string(row.key)
                                                      : row.label}
                             .spec();
                       })
                       .on_activate(activated))
              .tab(1, "GridView",
                   ui::GridView<int>{m.items, m.grid_selected}
                       .state(m.grid_state)
                       .minimum_cell_width(160)
                       .cell_height(76)
                       .gap(10)
                       .cell([icon = m.symbol](const auto &row) {
                         return ui::Row{
                             ui::IconView{icon}.size(28).color(kAccent),
                             ui::Label{row.label}}
                             .gap(10)
                             .align(ui::Align::Center)
                             .spec();
                       })
                       .on_activate(activated))
              .tab(2, "OutlineTableView",
                   ui::OutlineTableView<int>{m.nodes, m.outline_table_selected,
                                             m.outline_table_expanded}
                       .columns(columns())
                       .tree_column("name")
                       .cell([](const auto &node, const auto &col) {
                         return ui::Label{col == "id" ? std::to_string(node.key)
                                                      : node.label}
                             .spec();
                       })
                       .on_activate(activated))),
      pair(card("TreeView",
                frame(250, ui::TreeView<int>{m.nodes, m.tree_selected,
                                             m.tree_expanded}
                               .row([](const auto &node) {
                                 return ui::Label{node.label}.spec();
                               })
                               .on_activate(activated))),
           card("OutlineView",
                frame(250, ui::OutlineView<int>{m.nodes, m.outline_selected,
                                                m.outline_expanded}
                               .state(m.outline_state)
                               .row_heights({30, false})
                               .row([](const auto &node) {
                                 return ui::Label{node.label}.spec();
                               })
                               .on_activate(activated)))),
      card("ListView",
           ui::ListView<int>{m.list_selection}
               .item(1, ui::Row{ui::Avatar{"Alice Martin"},
                                ui::Label{"Alice Martin — Design"}}
                            .gap(12)
                            .align(ui::Align::Center))
               .item(2, ui::Row{ui::Avatar{"Benoît Leroy"},
                                ui::Label{"Benoît Leroy — Développement"}}
                            .gap(12)
                            .align(ui::Align::Center))
               .item(3, ui::Row{ui::Avatar{"Camille Durand"},
                                ui::Label{"Camille Durand — Tests"}}
                            .gap(12)
                            .align(ui::Align::Center))
               .on_activate(activated))}
      .padding(0)
      .gap(18)
      .align(ui::Align::Stretch)
      .spec();
}

ui::Canvas tile(std::string text, ui::Color color, float width = 180,
                float height = 80) {
  return ui::Canvas{
      width, height, [text = std::move(text), color](ui::CanvasContext2D &g) {
        g.fill_rounded_rect({0, 0, g.width(), g.height()}, 10, color);
        g.text({g.width() / 2, g.height() / 2}, text, 14, ui::colors::text,
               ui::TextAlign::Center);
      }};
}
ui::Spec containers(Model &m) {
  return ui::Column{
      card("SplitView",
           frame(
               160,
               ui::SplitView{
                   m.split,
                   ui::Padding{
                       18, ui::Column{ui::Label{"Panneau de navigation"}.bold(),
                                      hint("Glissez le séparateur →")}
                               .padding(0)
                               .gap(12)},
                   ui::Padding{
                       18,
                       ui::Column{
                           ui::Label{"Espace de travail"}.bold(),
                           hint("Redimensionnement à la souris et au clavier")}
                           .padding(0)
                           .gap(12)}}
                   .minimum_panes(170, 210))),
      pair(
          card("Collapsible · Accordion",
               ui::Column{
                   ui::Collapsible{"Options du projet", m.disclosure,
                                   ui::Row{ui::Checkbox{m.notify, "Activer"},
                                           ui::Toggle{"Bypass", m.bypass}}
                                       .gap(18)},
                   ui::Accordion{m.accordion}
                       .mode(ui::AccordionMode::Single)
                       .section(
                           "first", "À propos de NativeUI",
                           ui::Label{
                               "Composants retenus, rendu Skia, fenêtres Pugl"})
                       .section(
                           "second", "Interactions",
                           ui::Label{
                               "Pointeur, clavier, focus et états observables"})
                       .section("third", "Ressources",
                                ui::Label{"Images, SVG et typographie"})}
                   .padding(0)
                   .gap(14)
                   .align(ui::Align::Stretch)),
          card("Row · Column · Flex · Spacer · Padding",
               ui::Column{
                   ui::Row{tile("Fixe", {0.16f, 0.28f, 0.36f, 1}, 90),
                           ui::Flex{tile("Flex : espace restant",
                                         {0.11f, 0.36f, 0.35f, 1})}
                               .grow(1)}
                       .gap(10),
                   ui::Spacer{8},
                   ui::Padding{14,
                               ui::Label{"14 points de marge autour du texte"}},
                   hint("Les espaces et les proportions suivent la taille du "
                        "panneau")}
                   .padding(0)
                   .gap(10)
                   .align(ui::Align::Stretch))),
      pair(
          card("Grid · Stack · Clip",
               ui::Grid{ui::GridTracks{
                            {ui::Track::flex(), ui::Track::flex()},
                            {ui::Track::fixed(100), ui::Track::fixed(80)}},
                        tile("Cellule A", {0.2f, 0.25f, 0.4f, 1}),
                        tile("Cellule B", {0.35f, 0.23f, 0.4f, 1}),
                        ui::Clip{ui::Stack{
                            tile("Fond empilé", {0.1f, 0.35f, 0.3f, 1}),
                            ui::Padding{16, ui::Badge{"SUPERPOSITION"}}}},
                        tile("Cellule D", {0.2f, 0.3f, 0.4f, 1})}
                   .gap(10)),
          card("Scroll · ScrollView",
               ui::Column{
                   frame(100,
                         ui::Scroll{m.horizontal,
                                    ui::Row{tile("1", {0.17f, 0.27f, 0.4f, 1}),
                                            tile("2", {0.2f, 0.36f, 0.3f, 1}),
                                            tile("3", {0.36f, 0.25f, 0.4f, 1}),
                                            tile("4", {0.37f, 0.28f, 0.16f, 1})}
                                        .gap(10)}),
                   ui::Row{
                       ui::Button{"←",
                                  [&m] { m.horizontal.scroll_by({-160, 0}); }},
                       ui::Button{"→",
                                  [&m] { m.horizontal.scroll_by({160, 0}); }},
                       hint("Scroll piloté par l’application")}
                       .gap(12),
                   frame(100,
                         ui::ScrollView{
                             m.plain_scroll,
                             ui::Column{
                                 ui::Label{"ScrollView : molette et barre"},
                                 ui::Label{"Ligne 2"}, ui::Label{"Ligne 3"},
                                 ui::Label{"Ligne 4"}, ui::Label{"Ligne 5"},
                                 ui::Label{"Ligne 6"}}
                                 .padding(8)
                                 .gap(18)})}
                   .padding(0)
                   .gap(12)
                   .align(ui::Align::Stretch)))}
      .padding(0)
      .gap(18)
      .align(ui::Align::Stretch)
      .spec();
}

ui::Spec composition(Model &m) {
  ui::StyleScopeOverrides scope;
  scope.palette.accent = ui::Color{0.72f, 0.5f, 0.95f, 1};
  scope.palette.focus = ui::Color{0.72f, 0.5f, 0.95f, 1};
  return ui::Column{
      pair(
          card("Visibility · Enabled · ReadOnly",
               ui::Column{
                   ui::Row{ui::Checkbox{m.visible, "Visible"},
                           ui::Checkbox{m.enabled, "Actif"},
                           ui::Checkbox{m.read_only, "Lecture seule"}}
                       .gap(12),
                   ui::Visibility{
                       m.visible,
                       ui::Enabled{
                           m.enabled,
                           ui::ReadOnly{
                               m.read_only,
                               ui::Column{
                                   ui::TextInput{"Champ contrôlé", m.name},
                                   ui::Slider{m.value},
                                   ui::Button{
                                       "Action contrôlée",
                                       [&m] { m.report("Action autorisée"); }}}
                                   .padding(0)
                                   .gap(12)}}}
                       .mode(ui::VisibilityMode::Collapsed),
                   hint("Chaque option agit sur le sous-arbre ci-dessus")}
                   .padding(0)
                   .gap(14)
                   .align(ui::Align::Stretch)),
          card("If · Switch",
               ui::Column{ui::Checkbox{m.conditional,
                                       "Afficher la branche conditionnelle"},
                          ui::If{m.conditional, ui::Badge{"Branche If montée"}},
                          ui::SegmentedControl<int>{
                              "Branche",
                              m.branch,
                              {{0, "Repos"}, {1, "Actif"}, {2, "Terminé"}}},
                          ui::Switch<int>{m.branch}
                              .when(0, ui::Label{"En attente d’une action"})
                              .when(1, ui::ProgressBar{m.value})
                              .when(2, ui::Badge{"Terminé"})}
                   .padding(0)
                   .gap(14)
                   .align(ui::Align::Stretch))),
      pair(
          card("ForEach",
               ui::Column{
                   ui::Row{ui::Button{"Ajouter", [&m] { m.add_chip(); }},
                           ui::Button{
                               "Retirer",
                               [&m] {
                                 auto values = m.chips.get();
                                 if (!values.empty())
                                   values.pop_back();
                                 m.chips.set(std::move(values));
                               }}}
                       .gap(12),
                   ui::ForEach<int>{
                       m.chips, [](int id) { return id; },
                       [](int id) {
                         // ForEach preserves keys and superposes its children.
                         // The application gives each child a distinct logical
                         // position.
                         return ui::Column{
                             ui::Spacer{0, static_cast<float>(id - 1) * 30},
                             ui::Badge{"Élément " + std::to_string(id)}}
                             .padding(0)
                             .gap(0);
                       }},
                   hint("L’identité stable préserve les composants conservés")}
                   .padding(0)
                   .gap(14)
                   .align(ui::Align::Stretch)),
          card("FocusScope · CommandScope · StyleScope",
               ui::Column{
                   ui::Checkbox{m.focus_scope, "Limiter le focus au groupe"},
                   ui::FocusScope{
                       m.focus_scope,
                       ui::Row{ui::Button{"Premier",
                                          [&m] { m.report("Premier bouton"); }},
                               ui::Button{"Second",
                                          [&m] { m.report("Second bouton"); }}}
                           .gap(12)},
                   ui::CommandScope{
                       [&m](ui::Command command) {
                         m.report("Commande reçue : " +
                                  std::to_string(static_cast<int>(command)));
                         return ui::EventResult::Ignored;
                       },
                       ui::TextInput{"Commandes d’édition", m.filename}},
                   ui::StyleScope{
                       scope, ui::Column{ui::Label{"Accent local lavande"},
                                         ui::Slider{m.knob},
                                         ui::Toggle{"Style local", m.notify}}
                                  .padding(0)
                                  .gap(10)}}
                   .padding(0)
                   .gap(14)
                   .align(ui::Align::Stretch)))}
      .padding(0)
      .gap(18)
      .align(ui::Align::Stretch)
      .spec();
}

ui::Spec drawing(Model &m) {
  ui::TextStyle accent;
  accent.size = 17;
  accent.color = kAccent;
  return ui::Column{
      pair(card("Label · Header · RichText",
                ui::Column{
                    ui::Header{"Une interface expressive"}.subtitle(
                        "Hiérarchie, typographie et contenu enrichi"),
                    ui::Label{"Titre en gras"}.size(20).bold(),
                    ui::Label{"Texte secondaire en italique"}.italic().color(
                        kMuted),
                    ui::RichText{std::vector<ui::RichTextSpan>{
                        {.text = "NativeUI associe texte courant, "},
                        {.id = "interactive",
                         .text = "lien interactif",
                         .style = accent,
                         .underline = true,
                         .on_activate =
                             [&m] { m.report("Lien RichText activé"); }},
                        {.text = " et contenu multilingue.\nFrançais · "
                                 "Ελληνικά · العربية · 日本語\nLe paragraphe "
                                 "s’adapte à la largeur disponible."}}}}
                    .padding(0)
                    .gap(14)
                    .align(ui::Align::Stretch)),
           card("ImageView · IconView · Avatar",
                ui::Column{
                    ui::Row{ui::ImageView{m.icon}.size({130, 130}),
                            ui::Column{
                                ui::IconView{m.symbol}.size(50).color(kAccent),
                                ui::Badge{"SVG VECTORIEL"}}
                                .padding(0)
                                .gap(16)}
                        .gap(30)
                        .align(ui::Align::Center),
                    ui::Row{ui::Avatar{m.name}.size(52),
                            ui::Avatar{"Alice Martin"}.size(42),
                            ui::Avatar{"Benoît Leroy"}.size(36),
                            ui::Avatar{"Camille"}.size(30)}
                        .gap(16)
                        .align(ui::Align::Center),
                    hint("L’avatar du projet suit le nom saisi dans le "
                         "formulaire")}
                    .padding(0)
                    .gap(18)
                    .align(ui::Align::Stretch))),
      card("Canvas · Divider", ui::Column{hint("Cliquez sur la toile : le "
                                               "motif et le compteur changent"),
                                          ui::Canvas{880, 200,
                                                     [&m](ui::CanvasContext2D
                                                              &g) {
                                                       g.fill_rounded_rect(
                                                           {0, 0, g.width(),
                                                            g.height()},
                                                           12,
                                                           {0.06f, 0.15f, 0.2f,
                                                            1});
                                                       for (int i = 0; i < 90;
                                                            ++i) {
                                                         const auto x =
                                                             20.0f +
                                                             static_cast<float>(
                                                                 i) *
                                                                 (g.width() -
                                                                  40) /
                                                                 90;
                                                         const auto wave =
                                                             std::sin(
                                                                 static_cast<
                                                                     float>(i) *
                                                                     0.23f +
                                                                 static_cast<
                                                                     float>(
                                                                     m.strokes) *
                                                                     0.45f);
                                                         const auto extent =
                                                             18.0f +
                                                             std::abs(wave) *
                                                                 55;
                                                         g.line({x, g.height() /
                                                                            2 -
                                                                        extent},
                                                                {x, g.height() /
                                                                            2 +
                                                                        extent},
                                                                4, kAccent);
                                                       }
                                                       g.text(
                                                           {20, 24},
                                                           "CANVAS / " +
                                                               std::to_string(
                                                                   m.strokes) +
                                                               " INTERACTIONS",
                                                           12, kMuted);
                                                     }}
                                              .on_input(
                                                  [&m](const ui::
                                                           InputEvent &event,
                                                       ui::CanvasInputContext
                                                           &
                                                               context) {
                                                    if (event.type !=
                                                        ui::InputType::
                                                            PointerDown)
                                                      return ui::EventResult::
                                                          Ignored;
                                                    ++m.strokes;
                                                    context.invalidate();
                                                    m.report("Toile : " +
                                                             std::to_string(
                                                                 m.strokes));
                                                    return ui::EventResult::
                                                        Handled;
                                                  }),
                                          ui::Divider{},
                                          hint(
                                              "Dessin en coordonnées logiques, "
                                              "sans accès au backend")}
                                   .padding(0)
                                   .gap(14)
                                   .align(ui::Align::Stretch))}
      .padding(0)
      .gap(18)
      .align(ui::Align::Stretch)
      .spec();
}

ui::Spec messages(Model &m) {
  return ui::Column{
      pair(card("Dialog · Toast",
                ui::Column{hint("Ouvrez un dialogue modal ou une notification "
                                "temporaire"),
                           ui::Row{ui::Button{"Ouvrir le dialogue",
                                              [&m] { m.open_dialog(); }},
                                   ui::Button{"Afficher le toast",
                                              [&m] { m.notify_toast(); }}}
                               .gap(12),
                           hint("Échap ferme le dialogue · le toast disparaît "
                                "après son délai")}
                    .padding(0)
                    .gap(18)
                    .align(ui::Align::Stretch)),
           card("Popover · Tooltip",
                ui::Column{
                    ui::Popover{
                        m.popover,
                        ui::Button{"Ouvrir le popover",
                                   [&m] { m.popover.set(!m.popover.get()); }},
                        ui::Column{ui::Label{"Réglages rapides"}.bold(),
                                   ui::Toggle{"Notifications", m.notify},
                                   ui::Slider{m.value},
                                   ui::Button{"Fermer",
                                              [&m] { m.popover.set(false); }}}
                            .padding(18)
                            .gap(12)}
                        .match_anchor_width(),
                    ui::Tooltip{
                        "Une aide s’affiche au survol ou au focus après 400 ms",
                        ui::Button{"Survolez pour obtenir de l’aide",
                                   [&m] { m.report("Bouton avec infobulle"); }}}
                        .delay(400ms)}
                    .padding(0)
                    .gap(18)
                    .align(ui::Align::Stretch))),
      pair(card("ProgressBar · Meter",
                ui::Column{ui::Slider{m.value}.step(0.01f),
                           ui::ProgressBar{m.value},
                           ui::Meter{m.value}.levels({0.65f, 0.85f}),
                           hint("Le curseur pilote les deux indicateurs")}
                    .padding(0)
                    .gap(18)
                    .align(ui::Align::Stretch)),
           card("Spinner · Badge",
                ui::Column{
                    ui::Toggle{"Animation active", m.spinning},
                    ui::Row{
                        ui::Spinner{"Chargement"}.active(m.spinning).size(42),
                        ui::Badge{"83 COMPOSANTS"}, ui::Badge{"NATIVE / C++20"}}
                        .gap(20)
                        .align(ui::Align::Center),
                    hint("Animation liée au dispatcher de la fenêtre")}
                    .padding(0)
                    .gap(18)
                    .align(ui::Align::Stretch)))}
      .padding(0)
      .gap(18)
      .align(ui::Align::Stretch)
      .spec();
}

ui::Spec page(Model &m, int index, ui::Spec content) {
  return ui::ScrollView{
      *m.scroll[static_cast<std::size_t>(index)],
      ui::Grid{
          ui::GridTracks{{ui::Track::fixed(1000)}, {ui::Track::auto_size()}},
          ui::Column{
              ui::Row{ui::Label{
                          std::string{kPages[static_cast<std::size_t>(index)]}}
                          .size(27)
                          .bold(),
                      ui::Flex{ui::Spacer{0}}.grow(1),
                      ui::Badge{"0" + std::to_string(index + 1) + " / 08"}}
                  .align(ui::Align::Center)
                  .gap(16),
              hint("Cliquez, saisissez et naviguez au clavier pour explorer "
                   "les composants."),
              View{std::move(content)}}
              .padding(24)
              .gap(18)
              .align(ui::Align::Stretch)}}
      .spec();
}
ui::UI gallery(Model &m) {
  auto pages = ui::Switch<int>{m.page}
                   .when(0, View{page(m, 0, controls(m))})
                   .when(1, View{page(m, 1, inputs(m))})
                   .when(2, View{page(m, 2, dates(m))})
                   .when(3, View{page(m, 3, collections(m))})
                   .when(4, View{page(m, 4, containers(m))})
                   .when(5, View{page(m, 5, composition(m))})
                   .when(6, View{page(m, 6, drawing(m))})
                   .when(7, View{page(m, 7, messages(m))});
  auto sidebar =
      ui::Sidebar<int>{m.navigation}
          .section("library", "EXPLORER")
          .item(0, std::string{kPages[0]})
          .item(1, std::string{kPages[1]})
          .item(2, std::string{kPages[2]})
          .item(3, std::string{kPages[3]})
          .item(4, std::string{kPages[4]})
          .item(5, std::string{kPages[5]})
          .item(6, std::string{kPages[6]})
          .item(7, std::string{kPages[7]})
          .on_navigate([&m](const int &value) { m.select_page(value); });
  std::vector<ui::ToolbarItem> tools{
      {"Enregistrer",
       ui::Button{"Enregistrer", [&m] { m.notify_toast(); }}.spec(),
       ui::PopupMenuItem::action("Enregistrer", [&m] { m.notify_toast(); })},
      {"À propos", ui::Button{"À propos", [&m] { m.select_page(7); }}.spec(),
       ui::PopupMenuItem::action("À propos", [&m] { m.select_page(7); })}};
  ui::Theme theme = ui::default_theme();
  theme.palette.background = {0.055f, 0.075f, 0.11f, 1};
  theme.palette.surface = kSurface;
  theme.palette.accent = kAccent;
  theme.palette.focus = kAccent;
  theme.palette.muted_text = kMuted;
  theme.palette.selection = {0.12f, 0.28f, 0.29f, 1};
  theme.palette.control_background = {0.10f, 0.14f, 0.2f, 1};
  theme.controls.control_height = 36;
  return ui::UI{
      ui::Stack{
          ui::Canvas{1, 1,
                     [](ui::CanvasContext2D &g) {
                       g.fill_rect({0, 0, g.width(), g.height()},
                                   {0.055f, 0.075f, 0.11f, 1});
                     }},
          ui::Grid{
              ui::GridTracks{{ui::Track::flex()},
                             {ui::Track::fixed(94), ui::Track::flex(),
                              ui::Track::fixed(42)}},
              ui::Padding{
                  18, ui::Row{ui::IconView{m.icon}.size(48).monochrome(false),
                              ui::Column{ui::Label{"NativeUI"}.size(25).bold(),
                                         hint("LE LABORATOIRE DES COMPOSANTS")}
                                  .padding(0)
                                  .gap(4),
                              ui::Flex{ui::Spacer{0}}.grow(1),
                              ui::Badge{"83 COMPOSANTS"},
                              ui::Toolbar{"Actions du laboratoire",
                                          std::move(tools)}}
                          .gap(20)
                          .align(ui::Align::Center)},
              ui::Grid{
                  ui::GridTracks{{ui::Track::fixed(232), ui::Track::flex()},
                                 {ui::Track::flex()}},
                  ui::Padding{12, ui::Column{std::move(sidebar), ui::Spacer{16},
                                             hint("C++20 · Skia · Pugl"),
                                             hint("8 écrans interactifs")}
                                      .padding(0)
                                      .gap(8)
                                      .align(ui::Align::Stretch)},
                  std::move(pages)},
              ui::Padding{10,
                          ui::Row{ui::Badge{m.status},
                                  ui::Flex{ui::Spacer{0}}.grow(1),
                                  hint("Tab : focus   ·   Échap : fermer   ·   "
                                       "Molette : défiler")}
                              .gap(12)
                              .align(ui::Align::Center)}},
      },
      theme};
}

void snapshot(const ui::HeadlessRenderer &renderer,
              const std::filesystem::path &file) {
  std::ofstream output{file, std::ios::binary};
  output.exceptions(std::ios::badbit | std::ios::failbit);
  output << "P6\n"
         << renderer.pixel_width() << ' ' << renderer.pixel_height()
         << "\n255\n";
  const auto &rgba = renderer.rgba_pixels();
  std::vector<char> rgb;
  rgb.reserve(rgba.size() / 4 * 3);
  for (std::size_t i = 0; i < rgba.size(); i += 4)
    for (std::size_t channel = 0; channel < 3; ++channel)
      rgb.push_back(static_cast<char>(rgba[i + channel]));
  output.write(rgb.data(), static_cast<std::streamsize>(rgb.size()));
}
int self_test(const std::filesystem::path &snapshots = {}) {
  if (!snapshots.empty())
    std::filesystem::create_directories(snapshots);
  Model model;
  auto tree = gallery(model);
  example::Platform platform;
  auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
  ui::detail::DispatcherOwner dispatcher{{}, clock};
  ui::Dialog dialogs{tree};
  ui::Toast notices{tree, dispatcher.dispatcher()};
  model.dialogs = &dialogs;
  model.toasts = &notices;
  tree.resize({1280, 900});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{1280, 900}};
  for (int index = 0; index < static_cast<int>(kPages.size()); ++index) {
    model.select_page(index);
    if (!renderer.render(tree) || !tree.structural_diagnostic().empty())
      return example::fail("gallery page failed: " + std::to_string(index) +
                           " / " + tree.structural_diagnostic());
    if (!snapshots.empty())
      snapshot(renderer,
               snapshots / ("page_" + std::to_string(index + 1) + ".ppm"));
    tree.dispatch(example::key(ui::Key::Tab), platform);
    tree.dispatch(example::key(ui::Key::Tab, true), platform);
    renderer.resize({780, 600}, 1.25f);
    tree.resize({780, 600});
    if (!renderer.render(tree))
      return example::fail("compact gallery page failed");
    renderer.resize({1280, 900});
    tree.resize({1280, 900});
  }
  model.select_page(3);
  for (int tab = 0; tab < 3; ++tab) {
    model.collection_tab.set(tab);
    if (!renderer.render(tree))
      return example::fail("collection tab did not render");
  }
  auto rows = model.items.get();
  rows.clear();
  model.items.set(rows);
  for (int tab = 0; tab < 2; ++tab) {
    model.collection_tab.set(tab);
    if (!renderer.render(tree))
      return example::fail("empty collection failed");
  }
  model.items.set(make_items());
  model.collection_tab.set(0);
  model.sort_rows({"name", ui::SortDirection::Descending});
  model.add_row();
  auto updated = model.items.get();
  std::vector<int> keys;
  for (const auto &row : updated)
    keys.push_back(row.key);
  std::sort(keys.begin(), keys.end());
  if (keys.size() != 121 ||
      std::adjacent_find(keys.begin(), keys.end()) != keys.end() ||
      !renderer.render(tree))
    return example::fail("sorted collection insertion reused an identity");
  model.select_page(1);
  model.query.set("NativeUI");
  if (model.matches.get() != 2)
    return example::fail("find bar count does not follow the notes");
  model.notes.set("Sans correspondance");
  if (model.matches.get() != 0 || model.match.get())
    return example::fail("find bar retained a missing result");
  model.select_page(5);
  model.visible.set(false);
  model.enabled.set(false);
  model.read_only.set(true);
  model.conditional.set(false);
  model.branch.set(2);
  model.add_chip();
  if (!renderer.render(tree))
    return example::fail("composition transitions failed");
  model.select_page(7);
  model.popover.set(true);
  if (!renderer.render(tree) || tree.overlay_entries().empty())
    return example::fail("popover did not open");
  model.popover.set(false);
  if (!renderer.render(tree))
    return example::fail("popover did not close");
  model.open_dialog();
  if (!dialogs.active() || !renderer.render(tree))
    return example::fail("dialog did not open");
  tree.dispatch(example::key(ui::Key::Escape), platform);
  if (dialogs.active() || model.dialog_completions != 1)
    return example::fail("dialog completion failed");
  model.notify_toast();
  if (tree.overlay_entries().empty() || !renderer.render(tree))
    return example::fail("toast did not open");
  clock->advance(8001ms);
  (void)dispatcher.checkpoint();
  if (!renderer.render(tree) || !tree.overlay_entries().empty())
    return example::fail("toast did not expire");
  Model second;
  auto other = gallery(second);
  other.resize({1280, 900});
  other.activate(platform);
  if (!renderer.render(other) || second.page.get() != 0 ||
      second.chips.get().size() != 3)
    return example::fail("gallery instances share state");
  other.deactivate(platform);
  tree.deactivate(platform);
  model.dialogs = nullptr;
  model.toasts = nullptr;
  std::cout << "NativeUI gallery: 8 pages, 3 collection tabs, compact/HiDPI "
               "rendering, overlays and instance isolation passed.\n";
  return 0;
}
} // namespace

int main(int argc, char **argv) {
  try {
    std::filesystem::path snapshots;
    for (int i = 1; i < argc; ++i)
      if (std::string_view{argv[i]} == "--snapshot-dir") {
        if (i + 1 >= argc)
          return example::fail("--snapshot-dir requires a directory");
        snapshots = argv[++i];
      }
    if (example::self_test_requested(argc, argv) || !snapshots.empty())
      return self_test(snapshots);
#ifdef NATIVEUI_EXAMPLE_SELF_TEST_ONLY
    return example::fail(
        "window mode is disabled in self-test-only validation builds");
#else
    Model model;
    auto tree = gallery(model);
    ui::Application application;
    if (!application.valid())
      return example::fail("application initialization failed");
    ui::StandaloneWindow window{
        application, tree,
        ui::WindowDesc{.title = "NativeUI — Le laboratoire des composants",
                       .size = {1380, 940},
                       .resizable = true,
                       .min_size = ui::Size{780, 600},
                       .max_size = std::nullopt}};
    if (!window.valid())
      return example::fail("window initialization failed");
    ui::Dialog dialogs{tree};
    ui::Toast notices{tree, window.dispatcher()};
    model.dialogs = &dialogs;
    model.toasts = &notices;
    bool window_self_test = false;
    for (int i = 1; i < argc; ++i)
      window_self_test |= std::string_view{argv[i]} == "--window-self-test";
    if (window_self_test) {
      // Pump the real platform until its initial frame has been committed.
      for (int i = 0; i < 100 && tree.dirty(); ++i)
        if (!application.poll(0.01))
          break;
      const bool rendered = !tree.dirty();
      window.request_close();
      for (int i = 0; i < 100 && !window.is_closed(); ++i)
        if (!application.poll(0.01))
          break;
      model.dialogs = nullptr;
      model.toasts = nullptr;
      if (!rendered || !window.is_closed())
        return example::fail("native window did not render and close");
      std::cout << "NativeUI gallery: native window rendered and closed.\n";
      return 0;
    }
    const auto result = application.run();
    model.dialogs = nullptr;
    model.toasts = nullptr;
    return result;
#endif
  } catch (const std::exception &error) {
    return example::fail(error.what());
  }
}
