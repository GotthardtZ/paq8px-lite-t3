#include "NormalModel.hpp"

NormalModel::NormalModel(Shared* const sh, const uint64_t cmSize) :
  shared(sh), 
  cm(sh, cmSize),
  smOrder0(sh, 255, 4),
  smOrder1(sh, 255 * 256, 32),
  smOrder2(sh, 1<<24, 1023)
{
  assert(isPowerOf2(cmSize));
}

void NormalModel::mix(Mixer &m) {
  INJECT_SHARED_bpos
  INJECT_SHARED_c1
  INJECT_SHARED_c4
  INJECT_SHARED_pos
  if( bpos == 0 ) {

    INJECT_SHARED_buf

    //detect record length
    uint32_t r = pos - cPos1[c1];
    if (r > 1 && r <= 256 && (!isValidRecord || r < rLength || r % rLength != 0)) { // new length and not a multiple of the current length
      bool match = r == cPos1[c1] - cPos2[c1] && r == cPos2[c1] - cPos3[c1];
      if (match) {
        bool archived = r == rLengthArchive[0] || r == rLengthArchive[1] || r == rLengthArchive[2];
        match = archived || (r >= 32 || r == cPos3[c1] - cPos4[c1]) && (r >= 12 || (c1 == buf(r * 5 + 1) && c1 == buf(r * 6 + 1)));
        if(match) {
            // record detected
            rLength = r;
            isValidRecord = true;
            mismatchCount = 0;
            if (rLengthArchive[0] == r) {}
            else if (rLengthArchive[1] == r) {
              rLengthArchive[1] = rLengthArchive[0];
              rLengthArchive[0] = r;
            }
            else {
              rLengthArchive[2] = rLengthArchive[1];
              rLengthArchive[1] = rLengthArchive[0];
              rLengthArchive[0] = r;
            }
            //printf("\nRecordModel: detected rLength: %d (%d)\n", r, pos); // for debugging
        }
      }
    }
      

    if (isValidRecord) {
      if (buf(rLength + 1) == c1) {
        // reinforcement: we are still good
        // we don't zero the mismatch count completely so that more matches are needed to keep the count low
        mismatchCount >>= 1;
      }
      else {
        mismatchCount++;
        if (mismatchCount >= rLength) { // no matches for a whole row - we lost it
          //rLength = 0; //it's better to keep the record length and still use it to predict as we may have actually not lost it
          isValidRecord = false;
          mismatchCount = 0;
          //printf("\nRecordModel: reset (%d)\n", pos); // for debugging
        }
      }
    }

    // update last context positions
    cPos4[c1] = cPos3[c1];
    cPos3[c1] = cPos2[c1];
    cPos2[c1] = cPos1[c1];
    cPos1[c1] = pos;

    uint64_t i = 0;

    //recordmodel
    const uint8_t N = buf(rLength);
    const uint8_t NN = buf(rLength * 2);
    const uint8_t NNN = buf(rLength * 3);
    cm.set(i, hash(i, N, NN, NNN, rLength << 1 | isValidRecord));
    i++;

    cm.set(i, hash(i, buf(2), pos & 3));
    i++;
    cm.set(i, hash(i, buf(8), buf(4), pos & 3));
    i++;
    cm.set(i, hash(i, c4 & 0xf0f0f0f0, pos & 3));
    i++;
    cm.set(i, hash(i, c4 >> 8));
    i++;
    cm.set(i, hash(i, c4 & 0x00ffffff));
    i++;
    cm.set(i, hash(i, c4, buf(5), buf(6), buf(7)));
    i++;

    //context for 4-byte structures and 
    //positions of zeroes in the last 16 bytes
    ctx <<= 1;
    ctx |= (c4 & 0xff) == buf(5); //column matches in a 4-byte fixed structure
    ctx <<= 1;
    ctx |= (c4 & 0xff) == 0; //zeroes

    cm.set(i, hash(i, ctx));
    i++;

    type =
      c1 >= '0' && c1 <= '9' ? 0 :
      (c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') ? 1 :
      (c1 < 128) ? 2 : 3;

    if (type != lasttokentype)
      wordhash = MUL64_1;
    wordhash = hash(wordhash, c1);
    cm.set(i, wordhash);
    lasttokentype = type;
  }
  cm.mix(m);
  
  INJECT_SHARED_c0
  int p1, st;
  p1 = smOrder0.p1(c0 - 1);
  m.add((p1 - 2048) >> 2);
  st = stretch(p1);
  m.add(st >> 1);

  p1 = smOrder1.p1((c0 - 1) << 8 | c1); 
  m.add((p1 - 2048) >> 2);
  st = stretch(p1);
  m.add(st >> 1);
    
  p1 = smOrder2.p1(finalize64(c4 & 0xffffff, 24) ^ c0);
  m.add((p1 - 2048) >> 2);
  st = stretch(p1);
  m.add(st >> 1);

  uint32_t misses = shared->State.misses << ((8 - bpos) & 7); //byte-aligned
  misses = (misses & 0xffffff00) | (misses & 0xff) >> ((8 - bpos) & 7);

  uint32_t misses3 =
    ((misses & 0x1) != 0) |
    ((misses & 0xfe) != 0) << 1 |
    ((misses & 0xff00) != 0) << 2;
   
  const int order = cm.order; //0..6 (C)
  m.set((order * 7 + type) << 4 | isValidRecord << 3 | bpos, (ContextMap2::C + 1) * 2 * 8 * 7);
  m.set(((misses3) * 7 + type) * 255 + (c0 - 1), 255 * 8 * 7);
  m.set(order << 9 | (misses != 0) << 8 | c1, (ContextMap2::C + 1) * 2 * 256);
  m.set(cm.confidence, 19683); // 3^9
  m.set((pos & 3) << 8 | (ctx & 0xff), 4 * 256);

}

