#include "physics/ContactGroupUpdates.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>

namespace {
struct Key {
    std::array<unsigned,4> group{};
    std::array<unsigned,2> generation{};
    bool operator<(const Key& other) const { return group < other.group; }
    bool SameBodies(const Key& other) const { return group==other.group && generation==other.generation; }
};
struct Row { Key key; std::uint64_t identity; float normal; std::array<float,3> tangent; };
unsigned checks=0,failures=0;
bool negative=false;
void Check(bool value,const char* label){++checks;if(!value){if(failures<12)std::printf("FAIL %s\n",label);++failures;}}
bool Equal(const Row& a,const Row& b){return a.key.SameBodies(b.key)&&a.identity==b.identity&&std::memcmp(&a.normal,&b.normal,sizeof(float))==0&&std::memcmp(a.tangent.data(),b.tangent.data(),sizeof(a.tangent))==0;}
bool Equal(const std::vector<Row>& a,const std::vector<Row>& b){return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),[](const Row& x,const Row& y){return Equal(x,y);});}
void Sort(std::vector<Row>& rows){std::stable_sort(rows.begin(),rows.end(),[](const Row& a,const Row& b){return a.key<b.key;});}
std::vector<Row> Read(const std::vector<Row>& rows,const Key& key){std::vector<Row> found;for(const auto& row:rows)if(!(row.key<key)&&!(key<row.key))found.push_back(row);return found;}
void Corpus(){
    std::mt19937 random(0x78cd45u);std::uint64_t next=0;
    for(unsigned trial=0;trial<48;++trial){
        std::vector<Row> actual;
        for(unsigned i=0;i<400;++i){Key key{{unsigned(random()%24),unsigned(random()%5),unsigned(random()%3),unsigned(random()%2)},{1,1}};actual.push_back({key,next++,float(random()%40)*.125f,{0,-0.f,.25f}});}
        Sort(actual);auto expected=actual;
        {
            JudasPhysicsDetail::ContactGroupUpdates<Row> updates(actual);
            Check(updates.Empty()&&updates.copiedRows==0,"untouched store allocates/copies no row groups");
            for(unsigned event=0;event<240;++event){
                Key key{{unsigned(random()%32),unsigned(random()%5),unsigned(random()%3),unsigned(random()%2)},{1+unsigned(random()%3),1}};
                if(event%3==0){
                    expected.erase(std::remove_if(expected.begin(),expected.end(),[&](const Row& row){return row.key.SameBodies(key);}),expected.end());
                    auto& group=updates.Edit(key);
                    group.erase(std::remove_if(group.begin(),group.end(),[&](const Row& row){return negative||row.key.SameBodies(key);}),group.end());
                    for(unsigned i=0,n=random()%5;i<n;++i){Row row{key,next++,float(random()%40)*.125f,{-.5f,0,-0.f}};expected.push_back(row);group.push_back(row);}
                }else{
                    bool retained=false;
                    expected.erase(std::remove_if(expected.begin(),expected.end(),[&](const Row& row){
                        if(row.key<key||key<row.key)return false;
                        if(!row.key.SameBodies(key))return true;
                        retained=true;return false;
                    }),expected.end());
                    auto range=updates.Read(key);bool actualRetained=false,stale=false;
                    for(auto row=range.first;row!=range.second;++row){if(row->key.SameBodies(key))actualRetained=true;else stale=true;}
                    Check(retained==actualRetained,"retention tests only the current generation");
                    if(stale){auto& group=updates.Edit(key);group.erase(std::remove_if(group.begin(),group.end(),[&](const Row& row){return !row.key.SameBodies(key);}),group.end());}
                    if(!retained)for(unsigned i=0;i<4;++i){Row row{key,next++,0,{0,0,0}};expected.push_back(row);updates.Edit(key).push_back(row);}
                }
                Sort(expected);
                const auto range=updates.Read(key);std::vector<Row> observed(range.first,range.second);
                Check(Equal(Read(expected,key),observed),"staged generation/group rows match independent erase/append/stable-sort");
                if(event%17==0){updates.Publish();Check(Equal(actual,expected),"ordered publication preserves every row and warm-guess bit");}
                if(event%71==0){
                    updates.Publish();
                    const auto remove=[](const Row& row){return row.key.group[0]%7==0;};
                    expected.erase(std::remove_if(expected.begin(),expected.end(),remove),expected.end());
                    actual.erase(std::remove_if(actual.begin(),actual.end(),remove),actual.end());
                    Check(Equal(actual,expected),"global fallback safely rebases subsequent group updates");
                }
            }
        }
        Check(Equal(actual,expected),"scope exit publishes final staged groups in canonical order");
    }
}
void Unwind(){
    const Key first{{1,2,0,0},{1,1}},second{{1,2,0,0},{2,1}},missing{{0,0,0,0},{1,1}};
    std::vector<Row> rows{{first,1,.75f,{0,-0.f,.25f}},{second,2,.5f,{0,0,0}}};
    try{
        JudasPhysicsDetail::ContactGroupUpdates<Row> updates(rows);
        auto& group=updates.Edit(first);group.erase(group.begin());group.push_back({first,3,1,{0,0,0}});
        updates.Edit(missing); // Explicitly empty insertion must not discard adjacent groups.
        throw std::runtime_error("physical query failure after one completed update");
    }catch(const std::runtime_error&){}
    Check(rows.size()==2&&rows[0].identity==2&&rows[1].identity==3,"failure publication retains other generations and stable intra-group order");
}
}
int main(int argc,char** argv){
    negative=argc==2&&std::string(argv[1])=="--drop-generation";
    if(argc>2||(argc==2&&!negative))return 2;
    Corpus();Unwind();
    std::printf("Contact group updates: checks=%u failures=%u negative=%d\n",checks,failures,int(negative));
    return failures?1:0;
}
