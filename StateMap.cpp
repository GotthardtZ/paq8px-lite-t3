#include "StateMap.hpp"
#include "Utils.hpp"

StateMap::StateMap(const Shared* const sh, const int n, const int lim) :
  shared(sh), numContextsPerSet(n), t(n), limit(lim), cxt(0) {
  assert(limit > 0 && limit < 1024);
  dt = DivisionTable::getDT();
    for (uint32_t i = 0; i < numContextsPerSet; ++i) {
      t[i] = 2048 << 20 | 0; //initial p=0.5, initial count=0
    }
  
}

void StateMap::update() {
  uint32_t* const p = &t[cxt];
  uint32_t p0 = p[0];
  const int n = p0 & 1023U; //count
  const int pr = p0 >> 10U; //prediction (22-bit fractional part)
  if (n < limit) {
    ++p0;
  }
  else {
    p0 = (p0 & 0xfffffc00U) | limit;
  }
  INJECT_SHARED_y
  const int target = y << 22U; //(22-bit fractional part)
  const int delta = ((target - pr) >> 3U) * dt[n]; //the larger the count (n) the less it should adapt pr+=(target-pr)/(n+1.5)
  p0 += delta & 0xfffffc00U;
  p[0] = p0;
}

auto StateMap::p1(const uint32_t cx) -> int {
  assert(cx >= 0 && cx < numContextsPerSet);
  cxt = cx;
  return t[cx] >> 20;
}


void StateMap::print() const {
  for( uint32_t i = 0; i < t.size(); i++ ) {
    uint32_t p0 = t[i] >> 10;
    uint32_t n = t[i] & 1023;
    printf("%d\t%d\t%d\n", i, p0, n);
  }
}
