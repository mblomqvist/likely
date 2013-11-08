// Created 14-Apr-2012 by David Kirkby (University of California, Irvine) <dkirkby@uci.edu>

#ifndef LIKELY_UNIFORM_BINNING
#define LIKELY_UNIFORM_BINNING

#include "likely/AbsBinning.h"

namespace likely {
	// Represents a uniform 1D binning of a finite interval.
	class UniformBinning : public AbsBinning {
	public:
	    // Creates a new uniform binning for the interval [minValue,maxValue] using the specified
	    // number of bins. Throws a BinningError if maxValue <= minValue or nBins <= 0.
		UniformBinning(double minValue, double maxValue, int nBins);
		virtual ~UniformBinning();
        // Returns the bin index [0,nBins-1] corresponding to the specified value, or throws a
        // BinningError if value does not fall in any bin.
        virtual int getBinIndex(double value) const;
        // Returns the total number of bins.
        virtual int getNBins() const;
        // Returns the lower bound of the specified bin. Throws a BinningError if index is out of range.
        virtual double getBinLowEdge(int index) const;
        // Returns the upper bound of the specified bin. Throws a BinningError if index is out of range.
        virtual double getBinHighEdge(int index) const;
        // Returns the full width (hi-lo) of the specified bin, which might be zero if this bin represents
        // a point sample rather than an integral over some interval. Throws a BinningError if index
        // is out of range.
        virtual double getBinWidth(int index) const;
        // Returns the midpoint value (lo+hi)/2 of the specified bin. Throws a BinningError if index is
        // out of range.
        virtual double getBinCenter(int index) const;
        // Prints this binning to the specified output stream in a format compatible with createBinning.
        virtual void printToStream(std::ostream &os) const;
	private:
        double _minValue, _maxValue, _binWidth;
        int _nBins;
	}; // UniformBinning
} // likely

#endif // LIKELY_UNIFORM_BINNING
