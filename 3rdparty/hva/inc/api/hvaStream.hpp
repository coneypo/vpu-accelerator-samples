#ifndef HVA_HVASTREAM_HPP
#define HVA_HVASTREAM_HPP

namespace hva{

class hvaStream_t{

public:
    hvaStream_t();

    hvaStream_t duplicate() const;

    void runOnce() const;

    void wait() const;
};

}

#endif //HVA_HVASTREAM_HPP