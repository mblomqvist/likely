// Created 16-Apr-2012 by David Kirkby (University of California, Irvine) <dkirkby@uci.edu>

#include "likely/NonUniformSampling.h"
#include "likely/BinningError.h"

#include <cmath>
#include <ostream>

namespace local = likely;

local::NonUniformSampling::NonUniformSampling(std::vector<double> const &samplePoints, double ftol)
: _samplePoints(samplePoints), _ftol(ftol)
{
    // Check that we have some samples.
    int nSamples = _samplePoints.size();
    if(nSamples < 3) {
        throw BinningError("NonUniformSampling: need at least 3 sample points.");
    }
    // Check that sample points are increasing.
    for(int index = 1; index < nSamples; ++index) {
        if(_samplePoints[index-1] > _samplePoints[index]) {
            throw BinningError("NonUniformSampling: sample points are not in increasing order.");
        }
    }
    // A value of ftol < 0 means that getBinIndex will always throw a BinningError.
    if(ftol < 0) {
        throw BinningError("NonUniformSampling: expected ftol >= 0.");
    }
}

local::NonUniformSampling::~NonUniformSampling() { }

int local::NonUniformSampling::getBinIndex(double value) const {
    if(value < (_samplePoints[0] - _ftol*(_samplePoints[1]-_samplePoints[0]))) {
        throw BinningError("getBinIndex: value is below binning interval.");
    }
    for(int sample = 0; sample < _samplePoints.size(); ++sample) {
        int prev = (sample > 0) ? sample-1 : 0;
        int next = (sample < _samplePoints.size()-1) ? sample+1 : _samplePoints.size()-1;
        double scale = (next > prev) ? (_samplePoints[next] - _samplePoints[prev])/(next-prev) : 0;
        if(std::fabs(getBinCenter(sample) - value) <= _ftol*scale) return sample;
        if(value < getBinCenter(sample)) {
            throw BinningError("getBinIndex: value is not one of our samples.");
        }
    }
    throw BinningError("getBinIndex: value is above binning interval.");
}

int local::NonUniformSampling::getNBins() const {
    return _samplePoints.size();
}

double local::NonUniformSampling::getBinLowEdge(int index) const {
    return getBinCenter(index);
}

double local::NonUniformSampling::getBinHighEdge(int index) const {
    return getBinCenter(index);
}

double local::NonUniformSampling::getBinWidth(int index) const {
    isValidBinIndex(index,"getBinWidth: invalid bin index %d.");
    return 0;
}

double local::NonUniformSampling::getBinCenter(int index) const {
    isValidBinIndex(index,"getBinCenter: invalid bin index %d.");
    return _samplePoints[index];
}

void local::NonUniformSampling::printToStream(std::ostream &os) const {
    os << '{';
    for(int bin = 0; bin < _samplePoints.size(); ++bin) {
        if(bin) os << ',';
        os << _samplePoints[bin];
    }
    os << '}';
}
