// Created 10-Jun-2012 by David Kirkby (University of California, Irvine) <dkirkby@uci.edu>

#include "likely/FitParameterStatistics.h"
#include "likely/RuntimeError.h"
#include "likely/FunctionMinimum.h"
#include "likely/CovarianceAccumulator.h"
#include "likely/CovarianceMatrix.h"
#include "likely/WeightedAccumulator.h"
#include "likely/ExactQuantileAccumulator.h"

#include "boost/format.hpp"

#include <iostream>

namespace local = likely;

local::FitParameterStatistics::FitParameterStatistics(FitParameters const &params)
: _nupdates(0)
{
    // Remember the values of each free parameter, as a baseline.
    getFitParameterValues(params,_baseline,true);
    _nfree = _baseline.size();
    if(0 == _nfree) {
        throw RuntimeError("FitParameterStatistics: no free parameters.");
    }
    // Allocate our accumulators, with extra space (+1) for chisq statistics.
    _stats.reset(new WeightedAccumulator[_nfree+1]);
    _quantiles.reset(new ExactQuantileAccumulator[_nfree+1]);
    _accumulator.reset(new CovarianceAccumulator(_nfree+1));
    // Save labels to use in printToStream.
    getFitParameterNames(params,_labels,true);
    _labels.push_back("chiSquare");
}

local::FitParameterStatistics::~FitParameterStatistics() { }

void local::FitParameterStatistics::update(Parameters pvalues, double fval) {
    if(pvalues.size() != _nfree) {
        throw RuntimeError("FitParameterStatistics::update: unexpected number of parameter values.");
    }
    for(int par = 0; par < _nfree; ++par) {
        // Accumulate statistics for this parameter.
        _stats[par].accumulate(pvalues[par]);
        _quantiles[par].accumulate(pvalues[par]);
        // Calculate differences from the baseline fit result (to minimize
        // roundoff error when accumulating covariance statistics).
        pvalues[par] -= _baseline[par];
    }
    // Include the fit chiSquare = 2*fval in our statistics.
    double chisq(2*fval);
    _stats[_nfree].accumulate(chisq);
    _quantiles[_nfree].accumulate(chisq);
    pvalues.push_back(chisq);
    _accumulator->accumulate(pvalues);
    _nupdates++;
}

void local::FitParameterStatistics::printToStream(std::ostream &out, std::string const &formatSpec) const {
    std::string resultSpec("%20s = ");
    resultSpec += formatSpec + " +/- " + formatSpec + " <<< " + formatSpec + " << " + formatSpec + " < " +
        formatSpec + " | " + formatSpec + " | " + formatSpec + " > " + formatSpec + " >> " + formatSpec + " >>>\n";
    boost::format resultFormat(resultSpec.c_str());
    out << std::endl << "Fit Parameter Value Statistics:" << std::endl;
    for(int stat = 0; stat <= _nfree; ++stat) {
        double median = _quantiles[stat].getQuantile(0.5);
        out << resultFormat % _labels[stat] % _stats[stat].mean() % _stats[stat].error()
            % (median - _quantiles[stat].getQuantile(0.5 - 0.9973/2))  // -3sig
            % (median - _quantiles[stat].getQuantile(0.5 - 0.9545/2))  // -2sig
            % (median - _quantiles[stat].getQuantile(0.5 - 0.6827/2))  // -1sig
            % (median                                               )  // median
            % (_quantiles[stat].getQuantile(0.5 + 0.6827/2) - median)  // +1sig
            % (_quantiles[stat].getQuantile(0.5 + 0.9545/2) - median)  // +2sig
            % (_quantiles[stat].getQuantile(0.5 + 0.9973/2) - median); // +3sig
    }
    out << std::endl << "Fit Parameter Value RMS & Correlations:" << std::endl;
    try {
        _accumulator->getCovariance()->printToStream(out,true,formatSpec,_labels);
    }
    catch(likely::RuntimeError const &e) {
        out << "!!! failed to estimate full covariance matrix !!!" << std::endl;
    }
}
