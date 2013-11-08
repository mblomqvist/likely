// Created 24-Apr-2012 by David Kirkby (University of California, Irvine) <dkirkby@uci.edu>

#include "likely/BinnedData.h"
#include "likely/RuntimeError.h"
#include "likely/AbsBinning.h"
#include "likely/CovarianceMatrix.h"

#include "boost/foreach.hpp"
#include "boost/format.hpp"
#include "boost/lexical_cast.hpp"

#include <iostream>

namespace local = likely;

local::BinnedData::BinnedData(BinnedGrid const &grid)
: _grid(grid)
{
    _offset.resize(_grid.getNBinsTotal(),EMPTY_BIN);
    _weight = 1;
    _weighted = false;
    _finalized = false;
}

local::BinnedData::~BinnedData() { }

local::BinnedData *local::BinnedData::clone(bool binningOnly) const {
    return binningOnly ? new BinnedData(_grid) : new BinnedData(*this);
}

void local::BinnedData::cloneCovariance() {
    if(hasCovariance()) {
        // Release our reference to the original covariance matrix and reset our
        // smart pointer to a copy of the original covariance matrix.
        _covariance.reset(new CovarianceMatrix(*_covariance));
    }
}

void local::BinnedData::dropCovariance(double weight) {
    if(isFinalized()) throw RuntimeError("BinnedData::dropCovariance: object is finalized.");
    unweightData();
    _covariance.reset();
    _weight = weight;
}

local::BinnedData& local::BinnedData::add(BinnedData const& other, double weight) {
    // All done if the requested weight is zero.
    if(0 == weight) return *this;
    // Do we have any data yet?
    if(0 == getNBinsWithData()) {
        // If we are empty, then we only require that the other dataset have the same binning.
        if(!isCongruent(other,true)) {
            throw RuntimeError("BinnedData::add: datasets have different binning.");
        }
        // Initialize each occupied bin of the other dataset to zero contents in our dataset.
        for(IndexIterator iter = other.begin(); iter != other.end(); ++iter) {
            setData(*iter,0);
        }
        // If the other dataset has a covariance matrix, initialize ours now.
        if(other.hasCovariance()) {
            _covariance.reset(new CovarianceMatrix(getNBinsWithData()));
        }
        else {
            // The scalar _weight plays the role of Cinv in the absence of any _covariance.
            // Set it to zero here since we will be adding the other data's weight below.
            _weight = 0;
        }
        // Our zero data vector should be interpreted as Cinv.data for the
        // purposes of adding to the other dataset, below. We don't call _setWeighted here
        // because we don't actually want to transform the existing _data.
        _weighted = true;
    }
    else {
        // We already have data, so we will try to add the other data to ours.
        if(!isCongruent(other)) {
            throw RuntimeError("BinnedData::add: datasets are not congruent.");
        }
        // If we have a covariance, it must be modifiable.
        if(hasCovariance() && !isCovarianceModifiable()) {
            throw RuntimeError("BinnedData::add: cannot modify shared covariance.");
        }
    }
    // Add the weighted _data vectors, element by element, and save the result in our _data.
    _setWeighted(true,true); // flushes any cached data
    other._setWeighted(true);
    for(int offset = 0; offset < _data.size(); ++offset) {
        _data[offset] += weight*other._data[offset];
    }
    if(hasCovariance()) {
        // Add Cinv matrices and save the result as our new Cinv matrix.
        _covariance->addInverse(*other._covariance,weight);        
    }
    else {
        _weight += other._weight*weight;
    }
    return *this;
}

void local::BinnedData::unweightData() {
    // Note that both the methods below are const but we still declare this public method
    // as non-const since there is never any need to call it unless it will be followed
    // by a non-const method call that modifies our covariance.
    _setWeighted(false,true); // flushes any cached data
}

void local::BinnedData::_setWeighted(bool weighted, bool flushCache) const {
    // Are we already in the desired state?
    if(weighted != _weighted) {
        // Do we have a cached result we can use?
        if(_dataCache.size() > 0) {
            // Enable argument-dependent lookup (ADL)
            using std::swap;
            swap(_data,_dataCache);
        }
        else {
#ifdef PARANOID_DATA_CACHE
            // Save the original state of our cache.
            std::vector<double> saveCache = _dataCache;
#endif
            // Copy the original data to our cache (unless we are going to flush it below)
            if(!flushCache) _dataCache = _data;
        
            // Do the appropriate transformation of our data vector.
            if(weighted) {
                if(hasCovariance() && getNBinsWithData() > 0) {
                    // Change data to Cinv.data
                    _covariance->multiplyByInverseCovariance(_data);
                }
                else if(_weight != 1) {
                    // Scale data by _weight, which plays the role of Cinv.
                    for(int offset = 0; offset < _data.size(); ++offset) _data[offset] *= _weight;
                }
            }
            else {
                if(hasCovariance() && getNBinsWithData() > 0) {
                    // Change Cinv.data to data = C.Cinv.data
                    _covariance->multiplyByCovariance(_data);
                }
                else if(_weight != 1) {
                    // Scale data by 1/_weight, which plays the role of C.
                    for(int offset = 0; offset < _data.size(); ++offset) _data[offset] /= _weight;
                }
            }
#ifdef PARANOID_DATA_CACHE
            // If we have a saved cache, was it actually valid?
            if(saveCache.size() > 0) {
                assert(saveCache.size() == _data.size());
                for(int offset = 0; offset < _data.size(); ++offset) {
                    double eps = std::fabs(_data[offset] - saveCache[offset]);
                    if(eps > 1e-8) {
                        std::cerr << "Invalid BinnedData cache: " << offset << ' '
                            << _data[offset] << " != " << saveCache[offset] << " (eps = "
                            << eps << ")" << std::endl;
                        assert(false);
                        break;
                    }
                }
            }
#endif        
        }
        // Record our new state.
        _weighted = weighted;
    }
    // Flush the cache now, if requested. We use resize instead of swapping with an empty vector
    // since we are likely to need at least as much capacity in future, and the overhead of
    // this cache is small compared with a covariance matrix.
    if(flushCache) _dataCache.resize(0);
}

bool local::BinnedData::isCongruent(BinnedData const& other, bool onlyBinning, bool ignoreCovariance) const {
    if(!_grid.isCongruent(other.getGrid())) return false;
    if(!onlyBinning) {
        // [2] List (not set, i.e., order matters) of bins with data must be the same.
        if(other.getNBinsWithData() != getNBinsWithData()) return false;
        for(int offset = 0; offset < _index.size(); ++offset) {
            if(other._index[offset] != _index[offset]) return false;
        }
        if(!ignoreCovariance) {
            // [3] Both must have or not have an associated covariance matrix.
            if(other.hasCovariance() && !hasCovariance()) return false;
            if(!other.hasCovariance() && hasCovariance()) return false;
        }
    }
    return true;
}

int local::BinnedData::getIndexAtOffset(int offset) const {
    if(offset < 0 || offset >= _index.size()) {
        throw RuntimeError("BinnedData::getIndexAtOffset: invalid offset.");
    }
    return _index[offset];
}

int local::BinnedData::getOffsetForIndex(int index) const {
    if(!hasData(index)) {
        throw RuntimeError("BinnedData::getOffsetForIndex: no data at index.");
    }
    return _offset[index];
}

bool local::BinnedData::hasData(int index) const {
    _grid.checkIndex(index);
    return !(_offset[index] == EMPTY_BIN);
}

double local::BinnedData::getData(int index, bool weighted) const {
    if(!hasData(index)) {
        throw RuntimeError("BinnedData::getData: bin is empty.");
    }
    _setWeighted(weighted);
    return _data[_offset[index]];
}

void local::BinnedData::setData(int index, double value, bool weighted) {
    _setWeighted(weighted,true); // flushes any cached data
    if(hasData(index)) {
        _data[_offset[index]] = value;
    }
    else {
        if(hasCovariance()) {
            throw RuntimeError("BinnedData::setData: cannot add data after covariance.");
        }
        if(isFinalized()) {
            throw RuntimeError("BinnedData::setData: object is finalized.");
        }
        _offset[index] = _index.size();
        _index.push_back(index);
        _data.push_back(value);
    }
}

void local::BinnedData::addData(int index, double offset, bool weighted) {
    if(!hasData(index)) {
        throw RuntimeError("BinnedData::addData: bin is empty.");        
    }
    _setWeighted(weighted,true); // flushes any cached data
    _data[_offset[index]] += offset;
}

double local::BinnedData::getCovariance(int index1, int index2) const {
    if(!hasCovariance()) {
        throw RuntimeError("BinnedData::getCovariance: has no covariance specified.");
    }
    if(!hasData(index1) || !hasData(index2)) {
        throw RuntimeError("BinnedData::getCovariance: bin is empty.");
    }
    return _covariance->getCovariance(_offset[index1],_offset[index2]);
}

double local::BinnedData::getInverseCovariance(int index1, int index2) const {
    if(!hasCovariance()) {
        throw RuntimeError("BinnedData::getInverseCovariance: has no covariance specified.");
    }
    if(!hasData(index1) || !hasData(index2)) {
        throw RuntimeError("BinnedData::getInverseCovariance: bin is empty.");
    }
    return _covariance->getInverseCovariance(_offset[index1],_offset[index2]);
}

void local::BinnedData::setCovariance(int index1, int index2, double value) {
    if(!hasData(index1) || !hasData(index2)) {
        throw RuntimeError("BinnedData::setCovariance: bin is empty.");
    }
    if(!hasCovariance()) {
        if(isFinalized()) {
            throw RuntimeError("BinnedData::setCovariance: object is finalized.");
        }
        // Create a new covariance matrix sized to the number of bins with data.
        _covariance.reset(new CovarianceMatrix(getNBinsWithData()));
    }
    if(!isCovarianceModifiable()) {
        throw RuntimeError("BinnedData::setCovariance: cannot modify shared covariance.");
    }
    // Note that we do not call _setWeighted here, so we are changing the meaning
    // of _data in a way that depends on the current value of _weighted.
    _covariance->setCovariance(_offset[index1],_offset[index2],value);
}

void local::BinnedData::setInverseCovariance(int index1, int index2, double value) {
    if(!hasData(index1) || !hasData(index2)) {
        throw RuntimeError("BinnedData::setInverseCovariance: bin is empty.");
    }
    if(!hasCovariance()) {
        if(isFinalized()) {
            throw RuntimeError("BinnedData::setInverseCovariance: object is finalized.");
        }
        // Create a new covariance matrix sized to the number of bins with data.
        _covariance.reset(new CovarianceMatrix(getNBinsWithData()));
    }
    if(!isCovarianceModifiable()) {
        throw RuntimeError("BinnedData::setInverseCovariance: cannot modify shared covariance.");
    }
    // Note that we do not call _setWeighted here, so we are changing the meaning
    // of _data in a way that depends on the current value of _weighted.
    _covariance->setInverseCovariance(_offset[index1],_offset[index2],value);
}

void local::BinnedData::transformCovariance(CovarianceMatrixPtr D) {
    if(!hasCovariance()) {
        throw RuntimeError("BinnedData::transformCovariance: no covariance to transform.");
    }
    // Make sure that our _data vector is independent of our _covariance before it changes.
    unweightData();
    // Replace D with C.Dinv.C where C is our original covariance matrix.
    D->replaceWithTripleProduct(*_covariance);
    // Swap C with D
    swap(*D,*_covariance);
}

void local::BinnedData::rescaleEigenvalues(std::vector<double> modeScales) {
    if(!hasCovariance()) {
        throw RuntimeError("BinnedData::rescaleEigenvalues: no covariance to transform.");
    }
    // Check that the scale vector has the expected size.
    if(modeScales.size() != getNBinsWithData()) {
        throw RuntimeError("BinnedData::rescaleEigenvalues: unexpected number of mode scales.");
    }
    // Make sure that our _data vector is independent of our _covariance before it changes.
    unweightData();
    // Rescale our covariance matrix in place.
    _covariance->rescaleEigenvalues(modeScales);
}

int local::BinnedData::projectOntoModes(int nkeep) {
    if(isFinalized()) {
        throw RuntimeError("BinnedData::projectOntoModes: object is finalized.");
    }
    if(!hasCovariance()) {
        throw RuntimeError("BinnedData::projectOntoModes: no covariance to define modes.");
    }
    int size(getNBinsWithData());
    if(0 == nkeep || nkeep >= size || nkeep <= -size) {
        throw RuntimeError("BinnedData::projectOntoModes: invalid value of nkeep.");
    }
    // Do the eigenmode analysis.
    std::vector<double> eigenvalues,eigenvectors;
    _covariance->getEigenModes(eigenvalues,eigenvectors);
    // What range of modes are we projecting onto?
    int index1,index2,ndrop;
    if(nkeep > 0) {
        index1 = 0;
        index2 = nkeep;
        ndrop = size - nkeep;
    }
    else {
        index1 = size + nkeep;
        index2 = size;
        ndrop = size + nkeep;
    }
    // Prepare to change our data vector.
    unweightData();
    std::vector<double> projected(size,0);
    // Loop over modes
    for(int index = index1; index < index2; ++index) {
        // Calculate the dot product of this mode with our data vector.
        double dotprod(0);
        for(int bin = 0; bin < size; ++bin) {
            dotprod += _data[bin]*eigenvectors[index*size+bin];
        }
        // Update our projected vector.
        for(int bin = 0; bin < size; ++bin) {
            projected[bin] += dotprod*eigenvectors[index*size+bin];
        }
    }
    _data = projected;
    return ndrop;
}

void local::BinnedData::setCovarianceMatrix(CovarianceMatrixPtr covariance) {
    if(isFinalized()) {
        throw RuntimeError("BinnedData::setCovarianceMatrix: object is finalized.");
    }
    if(covariance->getSize() != getNBinsWithData()) {
        throw RuntimeError("BinnedData::setCovarianceMatrix: new covariance has the wrong size.");
    }
    _covariance = covariance;
}

void local::BinnedData::shareCovarianceMatrix(BinnedData const &other) {
    if(isFinalized()) {
        throw RuntimeError("BinnedData::shareCovarianceMatrix: object is finalized.");
    }
    if(!other.hasCovariance()) {
        throw RuntimeError("BinnedData::shareCovarianceMatrix: no other covariance to share.");
    }
    // Ignore covariance when we test for congruence.
    if(!isCongruent(other,false,true)) {
        throw RuntimeError("BinnedData::shareCovarianceMatrix: datasets are not congruent.");
    }
    _covariance = other._covariance;
}

bool local::BinnedData::compress(bool weighted) const {
    // Get our data vector into the requested format (weighted/unweighted)
    _setWeighted(weighted);
    // Drop any storage used by our cache of the alternate format.
    std::vector<double>().swap(_dataCache);
    // Compress our covariance matrix, if any.
    return _covariance.get() ? _covariance->compress() : false;
}

void local::BinnedData::finalize() {
    _finalized = true;
}

bool local::BinnedData::isCompressed() const {
    return _covariance.get() ? _covariance->isCompressed() : false;
}

std::size_t local::BinnedData::getMemoryUsage(bool includeCovariance) const {
    std::size_t size = sizeof(*this) +
        sizeof(int)*(_offset.capacity() + _index.capacity()) +
        sizeof(double)*(_data.capacity() + _dataCache.capacity());
    if(hasCovariance() && includeCovariance) size += _covariance->getMemoryUsage();
    return size;
}

void local::BinnedData::prune(std::set<int> const &keep) {
    if(isFinalized()) {
        throw RuntimeError("BinnedData::prune: object is finalized.");
    }
    // Create a parallel set of internal offsets for each global index, checking that
    // all indices are valid.
    std::set<int> offsets;
    BOOST_FOREACH(int index, keep) {
        _grid.checkIndex(index);
        offsets.insert(_offset[index]);
    }
    // Are we actually removing anything?
    int newSize(offsets.size());
    if(newSize == getNBinsWithData()) return;
    // Reset our vector of offset for each index with data.
    _offset.assign(_grid.getNBinsTotal(),EMPTY_BIN);
    // Shift our (unweighted) data vector elements down to compress out any elements
    // we are not keeping. We are using the fact that std::set guarantees that iteration
    // follows sort order, from smallest to largest key value.
    unweightData();
    int newOffset(0);
    BOOST_FOREACH(int oldOffset, offsets) {
        // oldOffset >= newOffset so we will never clobber an element that we still need
        assert(oldOffset >= newOffset);
        int index = _index[oldOffset];
        _offset[index] = newOffset;
        _index[newOffset] = index;
        _data[newOffset] = _data[oldOffset];
        newOffset++;
    }
    _index.resize(newSize);
    _data.resize(newSize);
    // Prune our covariance matrix, if any.
    if(hasCovariance()) {
        if(!isCovarianceModifiable()) cloneCovariance();
        _covariance->prune(offsets);
    }
}

double local::BinnedData::chiSquare(std::vector<double> pred) const {
    if(pred.size() != getNBinsWithData()) {
        throw RuntimeError("BinnedData::chiSquare: prediction vector has wrong size.");
    }
    // Subtract our data vector from the prediction.
    IndexIterator nextIndex(begin());
    std::vector<double>::iterator nextPred(pred.begin());
    double residual, unweighted(0);
    while(nextIndex != end()) {
        residual = (*nextPred++ -= getData(*nextIndex++));
        unweighted += residual*residual;
    }
    // Our input vector now holds deltas. Our covariance does the rest of the work.
    return hasCovariance() ? _covariance->chiSquare(pred) : unweighted*_weight;
}

void local::BinnedData::getDecorrelatedWeights(std::vector<double> const &pred,
std::vector<double> &dweights) const {
    int nbins(getNBinsWithData());
    if(pred.size() != nbins) {
        throw RuntimeError("BinnedData::getDecorrelatedErrors: prediction vector has wrong size.");
    }
    dweights.reserve(nbins);
    dweights.resize(0);
    // Subtract the prediction from our data vector.
    std::vector<double> delta;
    delta.reserve(nbins);
    std::vector<double>::const_iterator nextPred(pred.begin());
    for(IndexIterator iter = begin(); iter != end(); ++iter) {
        delta.push_back(getData(*iter) - *nextPred++);
    }
    // Loop over bins
    for(int j = 0; j < nbins; ++j) {
        double dweight(0);
        if(hasCovariance()) {
            double deltaj(delta[j]);
            if(0 == deltaj) {
                dweight = _covariance->getInverseCovariance(j,j);
            }
            else {
                for(int k = 0; k < nbins; ++k) {
                    dweight += _covariance->getInverseCovariance(j,k)*delta[k]/deltaj;
                }
            }
        }
        else {
            dweight = _weight;
        }
        dweights.push_back(dweight);
    }
}

void local::BinnedData::printToStream(std::ostream &out, std::string format) const {
    boost::format indexFormat("[%4d] "),valueFormat(format);
    for(IndexIterator iter = begin(); iter != end(); ++iter) {
        int index(*iter);
        out << (indexFormat % index) << (valueFormat % getData(index)) << std::endl;
    }
}

void local::BinnedData::saveData(std::ostream &os, bool weighted) const {    
    for(IndexIterator iter = begin(); iter != end(); ++iter) {
        double value = getData(*iter,weighted);
        // Use lexical_cast to ensure that the full double precision is saved.
        os << *iter << ' ' << boost::lexical_cast<std::string>(value) << std::endl;
    }
}

void local::BinnedData::saveInverseCovariance(std::ostream &os, double scale) const {
    if(!getCovarianceMatrix()->isPositiveDefinite()) {
        throw RuntimeError("BinnedData::saveInverseCovariance: matrix is not positive definite.");
    }
    for(IndexIterator iter1 = begin(); iter1 != end(); ++iter1) {
        int index1(*iter1);
        // Save all diagonal elements.
        double value = scale*getInverseCovariance(index1,index1);
        os << index1 << ' ' << index1 << ' '
            << boost::lexical_cast<std::string>(value) << std::endl;
        // Loop over pairs with index2 > index1
        for(IndexIterator iter2 = iter1; ++iter2 != end();) {
            int index2(*iter2);
            value = scale*getInverseCovariance(index1,index2);
            // Only save non-zero off-diagonal elements.
            if(0 == value) continue;
            // Use lexical_cast to ensure that the full double precision is saved.
            os << index1 << ' ' << index2 << ' ' << boost::lexical_cast<std::string>(value) << std::endl;
        }
    }
}

local::BinnedDataPtr local::BinnedData::sample(RandomPtr random) const {
    // Create a new dataset with the same binning.
    bool binningOnly(true);
    BinnedDataPtr sampled(this->clone(binningOnly));
    // Fill the new dataset with noise sampled from our covariance.
    _covariance->sample(sampled->_data,random);
    // Copy our data vector book-keeping arrays to the sampled dataset.
    sampled->_offset = _offset;
    sampled->_index = _index;
    // Add our (unweighted) data vector to the sampled noise.
    _setWeighted(false);
    // sampled was constructed with _weighted = false and empty _dataCache so
    // the next line shouldn't actually do anything
    sampled->unweightData();
    for(int offset = 0; offset < _data.size(); ++offset) {
        sampled->_data[offset] += _data[offset];
    }
    // Copy our covariance matrix to the sampled data.
    sampled->setCovarianceMatrix(_covariance);
    return sampled;
}

double local::BinnedData::getScalarWeight() const {
    return hasCovariance() ? std::exp(-_covariance->getLogDeterminant()/getNBinsWithData()) : _weight;
}

std::string local::BinnedData::getMemoryState() const {
    std::string state = boost::str(boost::format("%6d %s%c ")
        % getMemoryUsage(false) % (_weighted ? "CinvD" : "    D")
        % (_dataCache.size() > 0 ? '+':'-')); // +/- indicates if complement to data is cached
    if(hasCovariance()) {
        state += boost::str(boost::format("refcount %2d ") % _covariance.use_count());
        state += _covariance->getMemoryState();
    }
    else {
        state += "no covariance";
    }
    return state;
}
