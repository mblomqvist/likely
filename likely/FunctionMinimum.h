// Created 30-May-2011 by David Kirkby (University of California, Irvine) <dkirkby@uci.edu>

#ifndef LIKELY_FUNCTION_MINIMUM
#define LIKELY_FUNCTION_MINIMUM

#include "likely/types.h"
#include "likely/FitParameter.h"

#include <string>
#include <iosfwd>

namespace likely {
    // Represents the information known about an approximate function minimum.
	class FunctionMinimum {
	public:
	    // Creates a function minimum object for the specified function minimum value and
	    // estimated location of the minimum in parameter space.
		FunctionMinimum(double minValue, FitParameters const &parameters);
		// Creates a function minimum object that also specifies an estimated covariance matrix
		// for the subset of floating parameters. Agreement between parameter errors and covariance
		// matrix diagonal elements is not checked or required.
        FunctionMinimum(double minValue, FitParameters const &parameters, CovarianceMatrixCPtr covariance);
		virtual ~FunctionMinimum();
		// Returns the function value at the minimum.
        double getMinValue() const;
        // Returns a copy of our FitParameters.
        FitParameters getFitParameters() const;
        // Returns the number of fit parameters. If onlyFloating is true, only the number
        // of floating parameters is returned.
        int getNParameters(bool onlyFloating = false) const;
		// Returns a vector of parameter values at this minimum. If onlyFloating is true, only
		// the values of floating parameters are included in the returned vector.
        Parameters getParameters(bool onlyFloating = false) const;
		// Filters the input parameters to only include floating parameters.
        void filterParameterValues(Parameters const &allValues, Parameters &floatingValues) const;
		// Returns a vector of parameter errors at this minimum. If onlyFloating is true, only
		// the errors of floating parameters are included in the returned vector.
        Parameters getErrors(bool onlyFloating = false) const;
        // Returns a vector of parameters names. If onlyFloating is true, on the names of
        // floating parameters are included in the returned vector.
        std::vector<std::string> getNames(bool onlyFloating = false) const;
        // Returns the index of the parameter with the specified name, or throws a RuntimeError.
        int findName(std::string const &name) const;
        // Updates the fit parameters and function value at the minimum.
        void updateParameters(double minValue, FitParameters const &parameters);
        // Updates the location of the minimum and the function value at that point. If a covariance
        // matrix is available, its diagonal elements will be used to update fit parameter errors.
        void updateParameterValues(double minValue, Parameters const &values);
        // Sets a single named parameter to the specified value, or throws a RuntimeError if
        // no such named parameter exists.
        void setParameterValue(std::string const &name, double value);
        // Returns true if a covariance matrix is available.
        bool hasCovariance() const;
        // Returns a pointer to the estimated covariance matrix of floating parameters at this minimum.
        CovarianceMatrixCPtr getCovariance() const;
        // Updates the covariance matrix associated with this minimum, if possible.
        // This method can be used to add a covariance matrix to a minimum that did not
        // originally have one. This method does not update the errors associated with our
        // parameters, but calling it before updateParameterValues() will have this effect.
        void updateCovariance(CovarianceMatrixCPtr covariance);
        // Fills toParams by adding a vector sampled from our covariance matrix to the
        // input fromParams vector. Returns the -log(liklihood) associated with the random
        // offset vector (see CovarianceMatrix::sample for details)
        double setRandomParameters(const Parameters &fromParams, Parameters &toParams) const;
        // Sets the number of times the function and its gradient have been evaluated to
        // obtain this estimate of the minimum.
        void setCounts(long nEvalCount, long nGradCount);
        long getNEvalCount() const;
        long getNGradCount() const;
        // Sets the status of this function minimum estimate. A newly created object has
        // status of OK.
        enum Status { OK, WARNING, ERROR };
        void setStatus(Status status, std::string const &message = std::string());
        Status getStatus() const;
        std::string getStatusMessage() const;
        // Ouptuts a multiline description of this minimum to the specified stream using
        // the specified printf format for floating point values.
        void printToStream(std::ostream &os, std::string const &formatSpec = "%12.6f") const;
        // Saves our parameter values and diagonal errors in plain text to the specified stream,
        // using full double precision. The format is a list of "index value error" lines, where
        // index values start at zero and include floating and fixed parameters, and error = 0
        // for fixed parameters.
        void saveParameters(std::ostream &os, bool onlyFloating = false) const;
        // Saves our floating-parameter covariance matrix in plain text to the specified stream,
        // use full double precision. The format is a list of "index1 index2 value" lines,
        // where value is the scaled covariance matrix element for (index1,index2), with indexing
        // that matches saveParameterValues(). Lines with value==0 or index2 < index1 are not
        // written to the file. Throws a RuntimeError if the covariance is not positive-definite.
        void saveFloatingParameterCovariance(std::ostream &os, double scale = 1) const;
	private:
        double _minValue;
        int _nFloating;
        FitParameters _parameters;
        CovarianceMatrixCPtr _covar;
        long _nEvalCount, _nGradCount;
        Status _status;
        std::string _statusMessage;
	}; // FunctionMinimum
	
    inline double FunctionMinimum::getMinValue() const { return _minValue; }
    inline FitParameters FunctionMinimum::getFitParameters() const { return _parameters; }
    inline bool FunctionMinimum::hasCovariance() const { return bool(_covar); }
    inline CovarianceMatrixCPtr FunctionMinimum::getCovariance() const { return _covar; }
    inline void FunctionMinimum::setCounts(long nEvalCount, long nGradCount) {
        _nEvalCount = nEvalCount;
        _nGradCount = nGradCount;
    }
    inline long FunctionMinimum::getNEvalCount() const { return _nEvalCount; }
    inline long FunctionMinimum::getNGradCount() const { return _nGradCount; }
    inline void FunctionMinimum::setStatus(Status status, std::string const &message) {
        _status = status;
        _statusMessage = message;
    }
    inline FunctionMinimum::Status FunctionMinimum::getStatus() const { return _status; }
    inline std::string FunctionMinimum::getStatusMessage() const { return _statusMessage; }
    inline int FunctionMinimum::getNParameters(bool onlyFloating) const {
        return onlyFloating ? _nFloating : _parameters.size();
    }
	
} // likely

#endif // LIKELY_FUNCTION_MINIMUM
