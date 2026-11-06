#include "CutCondensedPressure.h"
#include "CutExactResidual.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
namespace judas::volume_grid {
namespace {
struct Elimination {std::size_t node;CutCompensated diagonal;std::vector<std::pair<std::size_t,CutCompensated>> edges;};
bool Fail(std::string& e,const char* s){e=s;return false;}
}
bool SolveCutCondensedPressure(const std::vector<double>& storage,
 const std::vector<CutHydroacousticGraphLink>& links,const std::vector<double>& source,
 const std::vector<double>& tolerance,const std::vector<bool>& condense,
 CutCondensedResult& output,std::string& error){
 const auto n=storage.size();if(n==0||source.size()!=n||tolerance.size()!=n||condense.size()!=n)return Fail(error,"invalid condensed pressure dimensions");
 std::vector<std::map<std::size_t,CutCompensated>> adjacency(n);
 std::vector<CutCompensated> ground(n);std::vector<bool> removed(n);
 for(std::size_t i=0;i<n;++i){if(!(storage[i]>=0.&&std::isfinite(storage[i])&&std::isfinite(source[i])&&tolerance[i]>0.&&std::isfinite(tolerance[i])))return Fail(error,"invalid condensed row");ground[i]=storage[i];}
 for(const auto& e:links){if(e.left>=n||e.right>=n||e.left==e.right||!(e.conductance>0.&&std::isfinite(e.conductance)))return Fail(error,"invalid condensed edge");adjacency[e.left][e.right]=adjacency[e.left][e.right]+e.conductance;adjacency[e.right][e.left]=adjacency[e.right][e.left]+e.conductance;}
 std::vector<Elimination> eliminated;
 // Bound fill, not physical participation: high-degree nodes remain in the
 // reduced solve and in the final independent original-row audit.
 for(std::size_t i=0;i<n;++i)if(condense[i]&&adjacency[i].size()<=16){
  Elimination step{i,ground[i],{adjacency[i].begin(),adjacency[i].end()}};
  for(auto& e:step.edges)step.diagonal=step.diagonal+e.second;
  if(!(step.diagonal.Value()>0.))continue;
  for(auto& a:step.edges){ground[a.first]=ground[a.first]+a.second*(ground[i]/step.diagonal);adjacency[a.first].erase(i);}
  for(std::size_t a=0;a<step.edges.size();++a)for(std::size_t b=a+1;b<step.edges.size();++b){auto [j,wj]=step.edges[a];auto [k,wk]=step.edges[b];auto w=wj*(wk/step.diagonal);adjacency[j][k]=adjacency[j][k]+w;adjacency[k][j]=adjacency[k][j]+w;}
  adjacency[i].clear();removed[i]=true;eliminated.push_back(std::move(step));
 }
 std::vector<std::size_t> index(n,n),original;
 for(std::size_t i=0;i<n;++i)if(!removed[i]){index[i]=original.size();original.push_back(i);}
 std::vector<double> reducedGround(original.size());std::vector<CutHydroacousticGraphLink> reducedLinks;
 for(std::size_t i=0;i<original.size();++i){auto node=original[i];reducedGround[i]=ground[node].Value();for(auto& [j,w]:adjacency[node])if(j>node)reducedLinks.push_back({i,index[j],w.Value()});}
 // A gauge anchor belongs only to the preconditioner. The unchanged
 // original, ungrounded equations remain the final acceptance oracle.
 std::vector<std::size_t> component(original.size());std::iota(component.begin(),component.end(),0);
 auto root=[&](std::size_t i){while(component[i]!=i){component[i]=component[component[i]];i=component[i];}return i;};
 for(auto& e:reducedLinks){auto a=root(e.left),b=root(e.right);if(a!=b)component[b]=a;}
 std::vector<bool> anchored(original.size());std::vector<double> diagonal=reducedGround;
 for(auto& e:reducedLinks){diagonal[e.left]+=e.conductance;diagonal[e.right]+=e.conductance;}
 for(std::size_t i=0;i<original.size();++i)if(reducedGround[i]>0.)anchored[root(i)]=true;
 for(std::size_t i=0;i<original.size();++i)if(!anchored[root(i)]){reducedGround[i]=diagonal[i]>0.?diagonal[i]:1.;anchored[root(i)]=true;}
 std::vector<CutDirectedEdge> auditEdges;for(auto& e:links){auditEdges.push_back({e.left,e.right,e.conductance});auditEdges.push_back({e.right,e.left,e.conductance});}
 std::vector<CutCompensated> auditSource(source.begin(),source.end()),auditExterior(n);
 CutCondensedResult next;next.pressure.resize(n);next.eliminated=eliminated.size();next.reduced=original.size();
 for(int iteration=0;iteration<24;++iteration){
  std::vector<CutCompensated> residual(n);for(std::size_t i=0;i<n;++i)residual[i]=CutCompensated(source[i])-CutCompensated(storage[i])*next.pressure[i];
  next.flux.resize(links.size());
  for(std::size_t f=0;f<links.size();++f){auto e=links[f];auto q=CutCompensated(e.conductance)*(next.pressure[e.left]-next.pressure[e.right]);next.flux[f]=q;residual[e.left]=residual[e.left]-q;residual[e.right]=residual[e.right]+q;}
  double maxR=0.,maxAbs=0.;for(std::size_t i=0;i<n;++i){maxR=std::max(maxR,std::abs(residual[i].Value())/tolerance[i]);maxAbs=std::max(maxAbs,std::abs(residual[i].Value()));}
  if(maxR<=1.){std::vector<CutResidualInterval> enclosure;if(!BoundCutDirectedResidual(storage,auditEdges,auditExterior,auditSource,next.pressure,enclosure,error))return false;
   maxR=0.;for(std::size_t i=0;i<n;++i)maxR=std::max(maxR,std::max(std::abs(enclosure[i].lo),std::abs(enclosure[i].hi))/tolerance[i]);
  }
  next.maximumNormalizedResidual=maxR;next.refinements=iteration;
  if(maxR<=1.){output=std::move(next);error.clear();return true;}
  for(const auto& step:eliminated)for(auto& [j,w]:step.edges)residual[j]=residual[j]+(w/step.diagonal)*residual[step.node];
  std::vector<double> rhs(original.size());for(std::size_t i=0;i<original.size();++i)rhs[i]=residual[original[i]].Value();
  CutHydroacousticGraphResult correction;
  if(!rhs.empty()&&!SolveCutHydroacousticGraph(reducedGround,reducedLinks,rhs,std::max(maxAbs*1e-5,1e-300),correction,error))return false;
  std::vector<CutCompensated> dp(n);for(std::size_t i=0;i<original.size();++i)dp[original[i]]=correction.pressure[i];
  for(std::size_t k=eliminated.size();k-->0;){auto& step=eliminated[k];auto value=residual[step.node];for(auto& [j,w]:step.edges)value=value+w*dp[j];dp[step.node]=value/step.diagonal;}
  for(std::size_t i=0;i<n;++i)next.pressure[i]=next.pressure[i]+dp[i];
 }
 return Fail(error,"condensed pressure exhausted original-row refinement");
}
}
