#include <assert.h>
#include <vector>
#include "f14_migration_lease.h"
struct IO {
 int up[2]={95,95},down[2]={85,85};bool game=true;int fails=-1,calls=0;
 std::vector<int> order;
 bool game_off(){return game;}
 bool read(int n,int*v){int*p=n?down:up;v[0]=p[0];v[1]=p[1];return true;}
 bool write(int n,const int*v){
  order.push_back(n);if(calls++==fails)return false;
  // Simulate a kernel rejecting transient down > up.
  for(int i=0;i<2;i++)if(n?(v[i]>up[i]):(v[i]<down[i]))return false;
  int*p=n?down:up;p[0]=v[0];p[1]=v[1];return true;
 }
};
int main(){
 {IO io;F14MigrationLease<IO> l(io);assert(l.available());assert(l.renew(1));
  for(int t=2;t<10000;t+=100)assert(l.renew(t));assert(io.calls==2);
  assert(l.tick(11802));assert(l.active());assert(l.tick(11903));assert(!l.active());
  assert((io.order==std::vector<int>{1,0,0,1}));assert(l.available());}
 for(int fail=0;fail<2;fail++){IO io;io.fails=fail;F14MigrationLease<IO> l(io);
  assert(!l.renew(10));assert(io.up[0]==95&&io.down[0]==85);assert(!l.active());assert(!l.renew(20));}
 {IO io;io.game=false;F14MigrationLease<IO> l(io);assert(!l.available());assert(!l.renew(1));assert(io.calls==0);}
 {IO io;F14MigrationLease<IO> l(io);assert(l.renew(1));io.game=false;assert(!l.renew(100));assert(!l.active());assert(io.up[0]==95&&io.down[0]==85);}
 {IO io;F14MigrationLease<IO> l(io);assert(l.renew(1));io.up[0]=70;assert(!l.renew(100));assert(io.up[0]==70);assert(!l.active());}
 {IO io;F14MigrationLease<IO> l(io);assert(l.renew(1));io.down[0]=25;assert(!l.renew(100));assert(io.down[0]==25);assert(io.up[0]==95);}
 {IO io;io.up[0]=90;F14MigrationLease<IO> l(io);assert(!l.renew(1));assert(io.calls==0);}
 {IO io;F14MigrationLease<IO> l(io);assert(l.renew(1));assert(l.release());assert(l.release());assert(io.calls==4);}
 for(int fail=2;fail<4;fail++){IO io;F14MigrationLease<IO> l(io);assert(l.renew(1));io.fails=fail;
  assert(!l.release());assert(!l.at_baseline());assert(!l.renew(100));}
 return 0;
}
