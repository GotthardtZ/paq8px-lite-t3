#ifndef PAQ8PX_NORMALMODEL_HPP
#define PAQ8PX_NORMALMODEL_HPP

#include "../ContextMap2.hpp"

/**
 * Model for order 0-14 contexts
 * Contexts are hashes of previous 0..14 bytes.
 * Order 0..6, 8 and 14 are used for prediction.
 * Note: order 7+ contexts are modeled by matchModel as well.
 */
class NormalModel {
private:
    static constexpr int nCM = ContextMap2::C; // 8
    static constexpr int nSM = 8;
    Shared * const shared;
    uint64_t wordhash{};
    uint8_t type{};
    uint8_t lasttokentype{};
    uint32_t ctx = 0;
    
    //for recordmodel
    Array<uint32_t> cPos1{ 256 }, cPos2{ 256 }, cPos3{ 256 }, cPos4{ 256 };
    uint32_t mismatchCount = 0; // mismatch count for the current record length
    uint32_t rLength = 4; // current record length (initially as a default: 4)
    bool isValidRecord = false;
    uint32_t rLengthArchive[3] = { 0,0,0 }; // record length archive for quick re-detection
public:
    static constexpr int MIXERINPUTS = nCM * (ContextMap2::MIXERINPUTS) + nSM; // 35
    static constexpr int MIXERCONTEXTS =
      (ContextMap2::C + 1) * 2 * 8 * 7 +//504
      255 * 8 * 7 + //14280
      (ContextMap2::C + 1) * 2 * 256 + //4608
      19683 + //3^9
      4 * 256 //1024
    ; // 41227
    static constexpr int MIXERCONTEXTSETS = 5;
    NormalModel(Shared* const sh, const uint64_t cmSize);

    ContextMap2 cm;
    StateMap smOrder0;
    StateMap smOrder1;
    StateMap smOrder2;

    void mix(Mixer &m);
};

#endif //PAQ8PX_NORMALMODEL_HPP
