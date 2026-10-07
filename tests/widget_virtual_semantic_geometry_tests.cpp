#include <nativeui/semantics.hpp>
#include <stdexcept>

void check(bool condition){if(!condition)throw std::runtime_error("virtual semantic geometry contract");}
class Geometry final : public ui::detail::VirtualSemanticGeometry {
public:
 explicit Geometry(std::vector<ui::Rect> rects):rects_(std::move(rects)){}
 std::size_t size() const noexcept override{return rects_.size();}
 ui::Rect bounds_at(std::size_t index) const noexcept override{return index<rects_.size()?rects_[index]:ui::Rect{};}
private:std::vector<ui::Rect> rects_;
};
int main(){
 using V=ui::VirtualSemanticChildren;
 auto data=std::make_shared<V::Metadata>();
 for(std::uint64_t n=1;n<=4;++n){ui::VirtualSemanticItemMetadata m;m.token=n;m.name="item";data->push_back(m);}
 (*data)[1].role=ui::SemanticRole::Group;(*data)[1].expanded=ui::SemanticExpandedState::Expanded;(*data)[1].parent_token=1;(*data)[1].level=2;
 auto geo=std::make_shared<const Geometry>(std::vector<ui::Rect>{{10,20,80,5},{10,25,80,30},{10,55,80,10},{10,65,80,15}});
 auto selected=std::make_shared<const std::vector<ui::VirtualSemanticItemToken>>(std::vector<ui::VirtualSemanticItemToken>{2,4});
 const auto old=V::from_geometry(7,data,selected,geo);
 check(old.size()==4&&old.dataset_generation()==7);
 auto second=old.item_at(1);check(second&&second->info.selected&&second->info.role==ui::SemanticRole::Group&&second->info.expanded==ui::SemanticExpandedState::Expanded);
 check(second->parent_token==1&&second->level==2&&second->logical_bounds.y==25&&second->logical_bounds.h==30);
 check(!old.item_at(4)&&old.index_of_selected_item()==1);
 auto next=V::from_geometry(8,data,{},std::make_shared<const Geometry>(std::vector<ui::Rect>{{0,0,2,3}}));
 const auto missing=next.item_at(1)->logical_bounds;check(missing.x==0&&missing.y==0&&missing.w==0&&missing.h==0&&!next.item_at(1)->info.selected);
 check(old.item_at(1)->logical_bounds.y==25&&old.item_at(3)->info.selected);
 bool rejected=false;try{(void)V::from_geometry(9,data,std::make_shared<const std::vector<ui::VirtualSemanticItemToken>>(std::vector<ui::VirtualSemanticItemToken>{4,2}),geo);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
 rejected=false;try{(void)V::from_geometry(9,data,std::make_shared<const std::vector<ui::VirtualSemanticItemToken>>(std::vector<ui::VirtualSemanticItemToken>{2,2}),geo);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
 const auto legacy=V::from_metadata(3,data,2,{2,3,40,50},10,4);check(legacy.item_at(1)->logical_bounds.y==9&&legacy.item_at(1)->info.selected);
}
