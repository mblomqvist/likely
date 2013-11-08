// Created 08-Jun-2012 by David Kirkby (University of California, Irvine) <dkirkby@uci.edu>

#include "likely/FitModel.h"
#include "likely/RuntimeError.h"
#include "likely/AbsEngine.h"
#include "likely/FunctionMinimum.h"
#include "likely/CovarianceMatrix.h"

#include <iostream>

namespace local = likely;

local::FitModel::FitModel(std::string const &name)
: _name(name)
{ }

local::FitModel::~FitModel() { }

int local::FitModel::defineParameter(std::string const &name, double value, double error) {
    int newIndex = _parameters.size();
    _nameIndexMap.insert(NameIndexMap::value_type(name,newIndex));
    _parameters.push_back(FitParameter(name,value,error));
    _parameterValue.push_back(value);
    _parameterValueChanged.push_back(true);
    return newIndex;
}

int local::FitModel::getNParameters(bool onlyFloating) const {
    return countFitParameters(_parameters, onlyFloating);
}

bool local::FitModel::updateParameterValues(Parameters const &values) {
    int nValues(_parameterValue.size());
    if(values.size() != nValues) {
        throw RuntimeError("FitModel::updateParameterValues: invalid values size.");
    }
    bool anyChanged(false);
    for(int index = 0; index < nValues; ++index) {
        _setParameterValue(index,values[index]);
        if(_parameterValueChanged[index]) anyChanged = true;
    }
    return anyChanged;
}

void  local::FitModel::printToStream(std::ostream &out, std::string const &formatSpec) const {
    out << "Fit Model \"" << _name << "\" has initial parameters:" << std::endl;
    printFitParametersToStream(_parameters,out,formatSpec);
}

void local::FitModel::configureFitParameters(std::string const &script) {
    modifyFitParameters(_parameters,script);
}

local::FunctionMinimumPtr local::FitModel::findMinimum(FunctionPtr fptr, std::string const &method,
std::string const &oneTimeConfig) {
    if(0 < oneTimeConfig.length()) {
        // Apply the config script to a copy of our parameters.
        FitParameters modified(_parameters);
        modifyFitParameters(modified,oneTimeConfig);
        // Minimize using the modified parameters.
        return local::findMinimum(fptr, modified, method);
    }
    else {
        // Minimize using un-modified parameters.
        return local::findMinimum(fptr, _parameters, method);
    }
}

local::FunctionMinimumPtr local::FitModel::guessMinimum(FunctionPtr fptr) const {
    // Evaluate the function at our configured initial parameter values.
    Parameters pvalues;
    getFitParameterValues(_parameters,pvalues);
    double minValue = (*fptr)(pvalues);
    // Build a diagonal covariance matrix of our configured initial parameter errors.
    bool onlyFloating(true);
    int nFloating = countFitParameters(_parameters,onlyFloating);
    CovarianceMatrixPtr covariance(new CovarianceMatrix(nFloating));
    int index(0);
    for(FitParameters::const_iterator iter = _parameters.begin(); iter != _parameters.end(); ++iter) {
        if(iter->isFloating()) {
            double error(iter->getError());
            covariance->setCovariance(index,index,error*error);
            index++;
        }
    }
    // Return a FunctionMinimum pointer
    FunctionMinimumPtr fmin(new FunctionMinimum(minValue,_parameters,covariance));
    return fmin;
}

int local::FitModel::_checkIndex(int index) const {
    if(index < 0 || index >= _parameters.size()) {
        throw RuntimeError("FitModel: invalid parameter index.");
    }
    return index;
}

int local::FitModel::_getIndex(std::string const &name) const {
    // Could remember the last find result to speed this up if necessary.
    NameIndexMap::const_iterator where = _nameIndexMap.find(name);
    if(where == _nameIndexMap.end()) {
        throw RuntimeError("FitModel: unknown parameter \"" + name + "\"");
    }
    return where->second;
}

double local::FitModel::evaluatePriors() const {
    double penalty(0);
    for(int index = 0; index < _parameters.size(); ++index) {
        FitParameter const &param(_parameters[index]);
        if(!param.isFloating()) continue;
        FitParameter::PriorType priorType = param.getPriorType();
        if(priorType == FitParameter::NoPrior) continue;
        double value = _parameterValue[index];
        double priorMin = param.getPriorMin();
        double priorMax = param.getPriorMax();
        double priorScale = param.getPriorScale();
        double range = priorMax - priorMin;
        if(priorType == FitParameter::BoxPrior) {
            double sigma(priorScale*range);
            if(value < priorMin) {
                double diff((value - priorMin)/sigma);
                penalty += diff*diff/2;
            }
            else if(value > priorMax) {
                double diff((priorMax - value)/sigma);
                penalty += diff*diff/2;
            }
        }
        else if(priorType == FitParameter::GaussPrior) {
            double sigma = 0.5*priorScale*range;
            double diff((value - 0.5*(priorMin+priorMax))/sigma);
            penalty += diff*diff/2;
        }
    }
    return penalty;
}
