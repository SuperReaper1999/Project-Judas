from pathlib import Path
p=Path(__file__).parent/'sources/CollisionGeometry.cpp';s=p.read_text();needle='''  for(auto i:candidates(b,lo,hi,margin)){
''';replacement='''  const auto selected=candidates(b,lo,hi,margin);
  std::vector<uint32_t> connected;
  if(episode&&!episode->fronts.empty()){
   auto overlaps=[&](uint32_t feature){triangleInto(pb,asset,feature,rotation,placed);return !faceAxesSeparate(pa,pb,0)&&sat(pa,pb).gap<=0;};
   for(auto feature:episode->fronts)if(std::binary_search(selected.begin(),selected.end(),feature)&&overlaps(feature))connected.push_back(feature);
   auto edgeInside=[&](V x,V y){double enter=0,leave=1;for(unsigned face=0;face<pa.faces.size();++face){const V n=pa.normals[face];const auto& indices=pa.faces[face];const double from=glm::dot(n,x-pa.points[indices[0]]),rate=glm::dot(n,y-x);if(rate==0){if(from>eps)return false;}else{const double at=-from/rate;if(rate<0)enter=std::max(enter,at);else leave=std::min(leave,at);if(enter>leave)return false;}}return true;};
   for(std::size_t at=0;at<connected.size();++at){const auto& face=asset.faces[connected[at]];for(unsigned edge=0;edge<3;++edge){const auto neighbor=face.adjacent[edge];if(neighbor<0||std::find(connected.begin(),connected.end(),uint32_t(neighbor))!=connected.end()||!std::binary_search(selected.begin(),selected.end(),uint32_t(neighbor)))continue;
    const V x=placed+rotation*asset.vertices[face.vertices[edge]],y=placed+rotation*asset.vertices[face.vertices[(edge+1)%3]];
    if(edgeInside(x,y)&&overlaps(uint32_t(neighbor)))connected.push_back(uint32_t(neighbor));}}
   std::sort(connected.begin(),connected.end());
  }
  for(auto i:selected){
''';assert needle in s;s=s.replace(needle,replacement,1);s=s.replace('const bool owned=episode&&std::binary_search(episode->fronts.begin(),episode->fronts.end(),i);','const bool owned=std::binary_search(connected.begin(),connected.end(),i);');p.write_text(s)
