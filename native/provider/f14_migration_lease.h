#pragma once
#include <stdint.h>
// Finite per-vote ownership, for trials and the manual retained bridge.
// IO provides read(node,pair), write(node,pair), game_off().
// Node 0 is upmigrate, node 1 downmigrate. Never touch boost/core/thermal nodes.
template<class IO> class F14MigrationLease {
 IO& io;
 bool held=false,disabled=false;
 int64_t expires=0;
 static bool same(const int*a,const int*b){return a[0]==b[0]&&a[1]==b[1];}
 bool reads(int*u,int*d){return io.read(0,u)&&io.read(1,d);}
public:
 const int base_up[2]={95,95},base_down[2]={85,85};
 const int vote_up[2]={30,40},vote_down[2]={20,30};
 explicit F14MigrationLease(IO& access):io(access){}
 bool active()const{return held;}
 bool at_baseline(){int u[2],d[2];return reads(u,d)&&same(u,base_up)&&same(d,base_down);}
 bool available(){return !disabled&&io.game_off()&&at_baseline();}
 bool release(){
  if(!held)return true;
  int u[2],d[2];bool ok=reads(u,d);
  // Restore up first: both intermediate states keep down <= up. An external
  // value is never overwritten; restore only the still-owned component.
  if(ok){
   bool known_u=same(u,base_up)||same(u,vote_up);
   if(same(u,vote_up)){ok=io.write(0,base_up);known_u=ok;}
   if(known_u&&same(d,vote_down))ok=io.write(1,base_down)&&ok;
   else if(!same(d,base_down))ok=false;
   if(!known_u)ok=false;
  }
  held=false;expires=0;
  int after_u[2],after_d[2];
  ok=reads(after_u,after_d)&&same(after_u,base_up)&&same(after_d,base_down)&&ok;
  if(!ok)disabled=true;
  return ok;
 }
 bool tick(int64_t now){return !held||now<expires||release();}
 bool renew(int64_t now){
  if(disabled||now<=0)return false;
  int u[2],d[2];
  if(!io.game_off()||!reads(u,d)){release();disabled=true;return false;}
  if(held){
   if(!same(u,vote_up)||!same(d,vote_down)){release();disabled=true;return false;}
  }else{
   if(!same(u,base_up)||!same(d,base_down)){disabled=true;return false;}
   // Down first on entry. Mark ownership before either write so partial
   // failures can restore the successfully changed component.
   held=true;
   if(!io.write(1,vote_down)||!io.write(0,vote_up)||!reads(u,d)||
      !same(u,vote_up)||!same(d,vote_down)){
    release();disabled=true;return false;
   }
  }
  expires=now+2000;return true;
 }
};
