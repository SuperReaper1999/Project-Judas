#include "AIReservations.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
unsigned checks=0,failures=0;
void Check(bool value,const std::string& label) {
    ++checks;if(!value){++failures;std::cerr<<"FAIL "<<label<<'\n';}
}
void Reject(const std::function<void()>& operation,const std::string& label) {
    bool rejected=false;
    try {operation();}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,label);
}
}
int main() {
    AIReservationBoard board;
    Check(board.Size(0)==0,"empty board owns no targets");
    Check(board.Available("bed",1,0),"unclaimed bed is available");
    Check(board.Acquire("bed",1,0,1),"first claimant acquires bed");
    Check(board.Available("bed",1,0),"claimant may keep using its bed");
    Check(!board.Available("bed",2,0),"second claimant sees occupied bed");
    Check(!board.Acquire("bed",2,0,.1),"competitor cannot overwrite live exclusive claim");
    Check(!board.Release("bed",2),"competitor cannot release another claimant's bed");
    Check(board.Size(0)==1&&!board.Available("bed",2,0),"failed steal/release leave occupancy intact");
    Check(board.Acquire("meal",2,0,1),"independent target can be acquired concurrently");
    Check(board.Acquire("bed",1,.5,2),"owner renewal succeeds without changing claimant");
    Check(!board.Available("bed",2,1.1),"renewal extends the original lease");
    Check(board.Available("meal",1,1.1),"expired other target becomes available lazily");
    Check(board.Size(1.1)==1,"lazy expiry removes only expired target");
    Check(board.Available("bed",2,2.5),"lease is expired at exact deadline");
    Check(board.Acquire("bed",2,2.5,.001),"expired claim may be acquired by competitor");
    Check(!board.Available("bed",1,2.5),"new claimant actually owns the reacquired bed");
    Check(board.Size(2.501)==0,"minimum lease expires without background ticking");
    Check(!board.Release("bed",2),"release of absent claim is harmless and false");
    Check(board.Acquire("bed",2,2.501,60),"maximum supported lease succeeds");
    Check(board.Acquire("meal",2,2.501,60),"same claimant may own multiple targets");
    Check(board.Acquire("gate",3,2.501,60),"other claimant remains independent");
    board.ReleaseAll(2);
    Check(board.Size(2.501)==1&&board.Available("bed",3,2.501)&&!board.Available("gate",2,2.501),
          "retiring one claimant releases all and only its targets");
    board.ReleaseAll(2);
    Check(board.Release("gate",3)&&board.Size(2.501)==0,"release and repeated retirement are idempotent");

    AIReservationBoard transaction;
    Check(transaction.Acquire("retained",1,0,1),"transaction fixture starts genuinely occupied");
    const auto bad=[&](const std::function<void()>& action,const std::string& label) {
        Reject(action,label);
        Check(transaction.Size(.25)==1&&!transaction.Available("retained",2,.25),
              label+" preserves published ownership and prior clock");
    };
    bad([&]{transaction.Acquire("new",1,10,0);},"zero lease rejected before pruning or clock advancement");
    bad([&]{transaction.Acquire("new",1,10,-1);},"negative lease rejected transactionally");
    bad([&]{transaction.Acquire("new",1,10,60.01);},"overlong lease rejected transactionally");
    bad([&]{transaction.Acquire("new",0,10,1);},"zero claimant rejected transactionally");
    bad([&]{transaction.Acquire("",1,10,1);},"empty target rejected transactionally");
    bad([&]{transaction.Available(std::string(257,'x'),1,10);},"overlong target query rejected transactionally");
    bad([&]{transaction.Acquire(std::string("retained\0alias",14),1,10,1);},"embedded NUL target cannot alias another key");
    bad([&]{transaction.Acquire("new",1,std::numeric_limits<double>::infinity(),1);},"infinite time rejected transactionally");
    bad([&]{transaction.Acquire("new",1,std::numeric_limits<double>::quiet_NaN(),1);},"NaN time rejected transactionally");
    bad([&]{transaction.Acquire("new",1,10,std::numeric_limits<double>::quiet_NaN());},"NaN lease rejected transactionally");
    bad([&]{transaction.Acquire("new",1,std::numeric_limits<double>::max(),.001);},"unrepresentable positive duration is rejected before clock mutation");
    bad([&]{transaction.Size(-1);},"negative inspection time rejected transactionally");
    bad([&]{transaction.Available("retained",2,.1);},"backwards clock rejected transactionally");
    Check(!transaction.Available("retained",2,.25),"clock rejection cannot make occupied target available");

    AIReservationBoard claimantLimit;
    bool filled=true;
    for(unsigned i=0;i<256;++i)filled&=claimantLimit.Acquire("target-"+std::to_string(i),1,0,.5);
    Check(filled&&claimantLimit.Size(0)==256,"one claimant reaches exactly authored target bound");
    Check(!claimantLimit.Acquire("overflow",1,0,.5),"per-claimant capacity explicitly refuses acquisition");
    Check(claimantLimit.Size(0)==256,"overflow never evicts a live target");
    Check(claimantLimit.Acquire("target-0",1,0,.5),"renewal at claimant capacity still succeeds");
    Check(claimantLimit.Acquire("other",2,0,.5),"other claimant unaffected by first claimant limit");
    Check(claimantLimit.Acquire("new",1,.5,1)&&claimantLimit.Size(.5)==1,
          "expiry frees capacity before new acquisition");

    AIReservationBoard boardLimit;
    filled=true;
    for(unsigned claimant=1;claimant<=16;++claimant)
        for(unsigned i=0;i<256;++i)
            filled&=boardLimit.Acquire(std::to_string(claimant)+"-"+std::to_string(i),claimant,0,.5);
    Check(filled&&boardLimit.Size(0)==4096,"shared board reaches exact global bound");
    Check(!boardLimit.Acquire("overflow",17,0,.5),"global capacity refuses acquisition instead of evicting someone else");
    Check(boardLimit.Size(0)==4096&&!boardLimit.Available("1-0",17,0),"global overflow preserves original owner");
    Check(boardLimit.Acquire("1-0",1,0,.5),"renewal at full global board needs no new capacity");
    Check(boardLimit.Acquire("reused",17,.5,1)&&boardLimit.Size(.5)==1,
          "global expired capacity is reusable without a cleanup loop");

    // Distinct scopes in one script use distinct claimant identities. Sharing
    // an entity/slot must not make two NPC brains silently share one resource.
    AIReservationBoard scopes;
    Check(scopes.Acquire("last meal",101,0,1)&&!scopes.Acquire("last meal",102,0,1),
          "two scopes cannot self-share the same exclusive meal");
    scopes.ReleaseAll(101);
    Check(scopes.Acquire("last meal",102,0,1),"retired scope relinquishes resource to the other scope");
    AIReservationBoard restored;
    Check(restored.Available("last meal",101,0)&&restored.Acquire("last meal",103,0,1),
          "fresh session starts with no revived ownership token");

    std::cout<<"AI reservations: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
