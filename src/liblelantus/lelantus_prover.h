#ifndef ZCOIN_LIBLELANTUS_LELANTUSPROVER_H
#define ZCOIN_LIBLELANTUS_LELANTUSPROVER_H

#include "schnorr_prover.h"
#include "sigmaplus_prover.h"
#include "range_prover.h"
#include "coin.h"

namespace lelantus {

class LelantusProver {
public:
    LelantusProver(const Params* p);
    void proof(
            const std::vector <PublicCoin>& anonymity_set,
            const Scalar& Vin,
            const std::vector <PrivateCoin>& Cin,
            const std::vector <uint64_t>& indexes,
            const Scalar& Vout,
            const std::vector <PrivateCoin>& Cout,
            const Scalar& f,
            LelantusProof& proof_out);

private:
    void generate_sigma_proofs(
            const std::vector<PublicCoin>& c,
            const std::vector<PrivateCoin>& Cin,
            const std::vector<uint64_t>& indexes,
            Scalar& x,
            std::vector<Scalar>& Yk_sum,
            std::vector<SigmaPlusProof<Scalar, GroupElement>>& sigma_proofs);

    void generate_bulletproofs(
            const std::vector <PrivateCoin>& Cout,
            RangeProof<Scalar, GroupElement>& bulletproofs);

private:
    const Params* params;
};
}// namespace lelantus

#endif //ZCOIN_LIBLELANTUS_LELANTUSPROVER_H
