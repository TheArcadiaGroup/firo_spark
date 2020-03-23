#include "coin.h"
#include "primitives/zerocoin.h"

namespace lelantus {

//class PublicCoin
PublicCoin::PublicCoin() {
}

PublicCoin::PublicCoin(const GroupElement& coin):
    value(coin) {
}

const GroupElement& PublicCoin::getValue() const {
    return this->value;
}

uint256 PublicCoin::getValueHash() const {
    return primitives::GetPubCoinValueHash(value);
}

bool PublicCoin::operator==(const PublicCoin& other) const {
    return (*this).value == other.value;
}

bool PublicCoin::operator!=(const PublicCoin& other) const {
    return (*this).value != other.value;
}

bool PublicCoin::validate() const {
    return this->value.isMember() && !this->value.isInfinity();
}

size_t PublicCoin::GetSerializeSize() const {
    return value.memoryRequired();
}

//class PrivateCoin
PrivateCoin::PrivateCoin(const Params* p, const uint64_t& v):
    params(p) {
        this->mintCoin(v);
}

PrivateCoin::PrivateCoin(const Params* p,const Scalar& serial, const uint64_t& v, const Scalar& random, int version_) :
        params(p),
        serialNumber(serial),
        value(v),
        randomness(random),
        version(version_) {
    publicCoin = LelantusPrimitives<Scalar, GroupElement>::double_commit(
            params->get_g(), serialNumber, params->get_h1(), getVScalar(), params->get_h0(), randomness);
}

const Params* PrivateCoin::getParams() const {
    return this->params;
}

const PublicCoin& PrivateCoin::getPublicCoin() const {
    return this->publicCoin;
}

const Scalar& PrivateCoin::getSerialNumber() const {
    return this->serialNumber;
}

const Scalar& PrivateCoin::getRandomness() const {
    return this->randomness;
}

uint64_t PrivateCoin::getV() const {
    return this->value;
}

Scalar PrivateCoin::getVScalar() const {
    return Scalar(this->value);
}

unsigned int PrivateCoin::getVersion() const {
    return this->version;
}

void PrivateCoin::setPublicCoin(const PublicCoin& p){
    publicCoin = p;
}

void PrivateCoin::setRandomness(const Scalar& n){
    randomness = n;
}

const unsigned char* PrivateCoin::getEcdsaSeckey() const {
    return this->ecdsaSeckey;
}

void PrivateCoin::setEcdsaSeckey(const std::vector<unsigned char> &seckey) {
    if (seckey.size() == sizeof(ecdsaSeckey))
        std::copy(seckey.cbegin(), seckey.cend(), &ecdsaSeckey[0]);
    else
        throw std::invalid_argument("EcdsaSeckey size does not match.");
}

void PrivateCoin::setEcdsaSeckey(uint256 &seckey) {
    if (seckey.size() == sizeof(ecdsaSeckey))
        std::copy(seckey.begin(), seckey.end(), &ecdsaSeckey[0]);
    else
        throw std::invalid_argument("EcdsaSeckey size does not match.");
}

void PrivateCoin::setSerialNumber(const Scalar& n){
    serialNumber = n;
}

void PrivateCoin::setV(const uint64_t& n){
    value = n;
}

void PrivateCoin::setVersion(unsigned int nVersion){
    version = nVersion;
}

void PrivateCoin::mintCoin(const uint64_t& v){
    serialNumber.randomize();
    randomness.randomize();
    value = v;
    GroupElement commit = LelantusPrimitives<Scalar, GroupElement>::double_commit(
            params->get_g(), serialNumber, params->get_h1(), getVScalar(), params->get_h0(), randomness);
    publicCoin = PublicCoin(commit);
}

Scalar PrivateCoin::serialNumberFromSerializedPublicKey(
        const secp256k1_context *context,
        secp256k1_pubkey *pubkey) {
    std::vector<unsigned char> pubkey_hash(32, 0);

    static const unsigned char one[32] = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
    };

    // We use secp256k1_ecdh instead of secp256k1_serialize_pubkey to avoid a timing channel.
    if (1 != secp256k1_ecdh(context, pubkey_hash.data(), pubkey, &one[0])) {
        throw ZerocoinException("Unable to compute public key hash with secp256k1_ecdh.");
    }

    std::string zpts(ZEROCOIN_PUBLICKEY_TO_SERIALNUMBER);
    std::vector<unsigned char> pre(zpts.begin(), zpts.end());
    std::copy(pubkey_hash.begin(), pubkey_hash.end(), std::back_inserter(pre));

    unsigned char hash[CSHA256::OUTPUT_SIZE];
    CSHA256().Write(pre.data(), pre.size()).Finalize(hash);

    // Use 32 bytes of hash as coin serial.
    return Scalar(hash);
}

}//namespace lelantus