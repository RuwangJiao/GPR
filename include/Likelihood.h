/*
 * Copyright 2015 Christoph Jud (christoph.jud@unibas.ch)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#pragma once

#include <string>
#include <cmath>
#include <climits>
#include <memory>

//#include "GaussianProcess.h"

#include <Eigen/Dense>

namespace gpr{

/*
 * Kernel interface. Operator () has to be implemented in subclass
 */
template <class TScalarType>
class Likelihood{
public:
    typedef Likelihood Self;
    typedef std::shared_ptr<Self> Pointer;

    typedef GaussianProcess<TScalarType> GaussianProcessType;
    typedef std::shared_ptr<GaussianProcessType> GaussianProcessTypePointer;
    typedef typename GaussianProcessType::VectorType VectorType;
    typedef typename GaussianProcessType::MatrixType MatrixType;
    typedef typename GaussianProcessType::DiagMatrixType DiagMatrixType;
    typedef typename std::pair<VectorType,VectorType> ValueDerivativePair;
    typedef typename std::pair<VectorType,MatrixType> ValueJacobianPair;

    typedef long double HighPrecisionType;

    virtual inline VectorType operator()(const GaussianProcessTypePointer gp) const{
        throw std::string("Likelihood: operator() is not implemented.");
    }

    virtual inline VectorType GetParameterDerivatives(const GaussianProcessTypePointer gp) const{
        throw std::string("Likelihood: GetParameterDerivatives is not implemented.");
    }

    virtual inline ValueDerivativePair GetValueAndParameterDerivatives(const GaussianProcessTypePointer gp) const{
        throw std::string("Likelihood: GetValueAndParameterDerivatives is not implemented.");
    }

    virtual inline ValueJacobianPair GetValueAndJacobian(const GaussianProcessTypePointer gp) const{
        throw std::string("Likelihood: GetValueAndJacobian is not implemented.");
    }

    virtual std::string ToString() const = 0;

    Likelihood() { }
    virtual ~Likelihood() {}

protected:
    // methods using friendship to gaussian process class
    virtual void GetLabelMatrix(const GaussianProcessTypePointer gp, MatrixType& Y) const{
        gp->ComputeLabelMatrix(Y);
    }

    virtual TScalarType GetCoreMatrix(const GaussianProcessTypePointer gp, MatrixType& C) const{
        return gp->ComputeCoreMatrixWithDeterminant(C); // computes core and returns kernel matrix
    }

    virtual void GetDerivativeKernelMatrix(const GaussianProcessTypePointer gp, MatrixType& D) const{
        return gp->ComputeDerivativeKernelMatrix(D); // computes derivative of kernel
    }

    virtual TScalarType GetSigma(const GaussianProcessTypePointer gp) const{
        return gp->GetSigma();
    }

private:
      Likelihood (const Self&); //purposely not implemented
      void operator=(const Self&); //purposely not implemented
};

template <class TScalarType>
class GaussianLikelihood : public Likelihood<TScalarType>{
public:

    typedef Likelihood<TScalarType> Superclass;
    typedef GaussianLikelihood Self;
    typedef std::shared_ptr<Self> SelfPointer;
    typedef typename Superclass::VectorType VectorType;
    typedef typename Superclass::MatrixType MatrixType;
    typedef typename Superclass::GaussianProcessTypePointer GaussianProcessTypePointer;
    typedef typename Superclass::HighPrecisionType HighPrecisionType;

    virtual inline VectorType operator()(const GaussianProcessTypePointer gp) const{
        MatrixType Y; // label matrix
        this->GetLabelMatrix(gp, Y);

        MatrixType C; // core matrix inv(K + sigmaI)
        HighPrecisionType determinant; // determinant of K + sigma I
        determinant = this->GetCoreMatrix(gp, C);

        // data fit
        VectorType df = -0.5 * (Y.adjoint() * C * Y);
        for(unsigned i=0; i<df.rows(); i++){
            df[i] = std::exp(df[i]);
        }

        // complexity penalty
        if(determinant < -std::numeric_limits<HighPrecisionType>::epsilon()){
            std::stringstream ss;
            ss << "GaussianLikelihood: determinant of K is smaller than zero: " << determinant;
            throw ss.str();
        }
        TScalarType cp;

        if(determinant <= 0){
            cp = 1.0/std::sqrt(std::numeric_limits<HighPrecisionType>::min());
        }
        else{
            cp = 1.0/std::sqrt(determinant);
        }

        // constant term
        TScalarType ct = 1.0/std::pow(2*M_PI,C.rows()/2.0);

        return df.array() * cp * ct;
    }


    GaussianLikelihood() : Superclass(){  }
    virtual ~GaussianLikelihood() {}

    virtual std::string ToString() const{ return "GaussianLikelihood"; }

private:
    GaussianLikelihood(const Self&); //purposely not implemented
    void operator=(const Self&); //purposely not implemented
};

template <class TScalarType>
class GaussianLogLikelihood : public Likelihood<TScalarType>{
public:

    typedef Likelihood<TScalarType> Superclass;
    typedef GaussianLogLikelihood Self;
    typedef std::shared_ptr<Self> SelfPointer;
    typedef typename Superclass::VectorType VectorType;
    typedef typename Superclass::MatrixType MatrixType;
    typedef typename Superclass::GaussianProcessTypePointer GaussianProcessTypePointer;
    typedef typename Superclass::ValueDerivativePair ValueDerivativePair;
    typedef typename Superclass::ValueJacobianPair ValueJacobianPair;
    typedef typename Superclass::HighPrecisionType HighPrecisionType;

    virtual inline VectorType operator()(const GaussianProcessTypePointer gp) const{
        MatrixType Y; // label matrix
        this->GetLabelMatrix(gp, Y);

        MatrixType C; // core matrix inv(K + sigmaI)
        HighPrecisionType determinant; // determinant of K + sigma I
        determinant = this->GetCoreMatrix(gp, C);

        // data fit
        VectorType df = -0.5 * (Y.adjoint() * C * Y);

        // complexity penalty
        HighPrecisionType cp;

        if(determinant <= std::numeric_limits<HighPrecisionType>::min()){
            cp = -0.5 * std::log(std::numeric_limits<HighPrecisionType>::min());
        }
        else if(determinant > std::numeric_limits<HighPrecisionType>::max()){
            cp = -0.5 * std::log(std::numeric_limits<HighPrecisionType>::max());
        }
        else{
            cp = -0.5 * std::log(determinant);
        }


        // constant term
        TScalarType ct = -C.rows()/2.0 * std::log(2*M_PI);


        VectorType value = df.array() + (cp + ct);
        if(std::isinf(value.array().sum())){
            std::cout << "df: " << df << ", cp: " << cp << ", ct: " << ct << ", determinant: " << determinant << std::endl;
            throw std::string("GaussianLogLikelihood::GetValueAndParameterDerivatives: likelihood is infinite.");
        }

        return value;
    }

    virtual inline VectorType GetParameterDerivatives(const GaussianProcessTypePointer gp) const{
        MatrixType Y; // label matrix
        this->GetLabelMatrix(gp, Y);

        MatrixType C; // core matrix inv(K + sigmaI)
        this->GetCoreMatrix(gp, C);

        MatrixType alpha = C*Y;

        // D has the dimensions of num_params*C.rows x C.cols
        MatrixType D;
        this->GetDerivativeKernelMatrix(gp, D);

        unsigned num_params = static_cast<unsigned>(D.rows()/D.cols());
        if(static_cast<double>(D.rows())/static_cast<double>(D.cols()) - num_params != 0){
            throw std::string("GaussianLogLikelihood: wrong dimension of derivative kernel matrix.");
        }
        VectorType delta = VectorType::Zero(num_params);


        for(unsigned p=0; p<num_params; p++){
            delta[p] = 0.5 * ((alpha*alpha.adjoint() - C) * D.block(p*D.cols(),0,D.cols(),D.cols())).trace();
        }

        return delta;
    }

    virtual inline ValueDerivativePair GetValueAndParameterDerivatives(const GaussianProcessTypePointer gp) const{
        MatrixType Y; // label matrix
        this->GetLabelMatrix(gp, Y);

        MatrixType C; // core matrix inv(K + sigmaI)
        HighPrecisionType determinant; // determinant of K + sigma I
        determinant = this->GetCoreMatrix(gp, C);

        // data fit
        VectorType df = -0.5 * (Y.adjoint() * C * Y);

        // complexity penalty
        HighPrecisionType cp;

        if(determinant <= std::numeric_limits<HighPrecisionType>::min()){
            cp = -0.5 * std::log(std::numeric_limits<HighPrecisionType>::min());
        }
        else if(determinant > std::numeric_limits<HighPrecisionType>::max()){
            cp = -0.5 * std::log(std::numeric_limits<HighPrecisionType>::max());
        }
        else{
            cp = -0.5 * std::log(determinant);
        }


        // constant term
        TScalarType ct = -C.rows()/2.0 * std::log(2*M_PI);

        VectorType value = df.array() + (cp + ct);

        if(std::isinf(value.array().sum())){
            std::cout << "df: " << df << ", cp: " << cp << ", ct: " << ct << ", determinant: " << determinant << std::endl;
            throw std::string("GaussianLogLikelihood::GetValueAndParameterDerivatives: likelihood is infinite.");
        }

        // DERIVATIVE

        MatrixType alpha = C*Y;

        // D has the dimensions of num_params*C.rows x C.cols
        MatrixType D;
        this->GetDerivativeKernelMatrix(gp, D);

        unsigned num_params = static_cast<unsigned>(D.rows()/D.cols());
        if(static_cast<double>(D.rows())/static_cast<double>(D.cols()) - num_params != 0){
            throw std::string("GaussianLogLikelihood: wrong dimension of derivative kernel matrix.");
        }
        VectorType delta = VectorType::Zero(num_params);

        for(unsigned p=0; p<num_params; p++){
            delta[p] = 0.5 * ((alpha*alpha.adjoint() - C) * D.block(p*D.cols(),0,D.cols(),D.cols())).trace();
        }

        return std::make_pair(value, delta);
    }

    virtual inline ValueJacobianPair GetValueAndJacobian(const GaussianProcessTypePointer gp) const{
        MatrixType Y; // label matrix
        this->GetLabelMatrix(gp, Y);

        MatrixType C; // core matrix inv(K + sigmaI)
        HighPrecisionType determinant; // determinant of K + sigma I
        determinant = this->GetCoreMatrix(gp, C);

        // data fit
        VectorType df = -0.5 * (Y.adjoint() * C * Y);

        // complexity penalty
        HighPrecisionType cp;

        if(determinant <= std::numeric_limits<HighPrecisionType>::min()){
            cp = -0.5 * std::log(std::numeric_limits<HighPrecisionType>::min());
        }
        else if(determinant > std::numeric_limits<HighPrecisionType>::max()){
            cp = -0.5 * std::log(std::numeric_limits<HighPrecisionType>::max());
        }
        else{
            cp = -0.5 * std::log(determinant);
        }


        // constant term
        TScalarType ct = -C.rows()/2.0 * std::log(2*M_PI);

        VectorType value = df.array() + (cp + ct);

        if(std::isinf(value.array().sum())){
            std::cout << "df: " << df << ", cp: " << cp << ", ct: " << ct << ", determinant: " << determinant << std::endl;
            throw std::string("GaussianLogLikelihood::GetValueAndParameterDerivatives: likelihood is infinite.");
        }

        // DERIVATIVE

        // D has the dimensions of num_params*C.rows x C.cols
        MatrixType D;
        this->GetDerivativeKernelMatrix(gp, D);

        unsigned num_params = static_cast<unsigned>(D.rows()/D.cols());
        if(static_cast<double>(D.rows())/static_cast<double>(D.cols()) - num_params != 0){
            throw std::string("GaussianLogLikelihood: wrong dimension of derivative kernel matrix.");
        }

        MatrixType jacobian = MatrixType::Zero(Y.cols(), num_params);

        for(unsigned i=0; i<Y.cols(); i++){
            MatrixType alpha = C*Y.col(i);

            for(unsigned p=0; p<num_params; p++){
                jacobian(i,p) = 0.5 * ((alpha*alpha.adjoint() - C) * D.block(p*D.cols(),0,D.cols(),D.cols())).trace();
            }
        }

        return std::make_pair(value, jacobian);
    }

    GaussianLogLikelihood() : Superclass(){  }
    virtual ~GaussianLogLikelihood() {}

    virtual std::string ToString() const{ return "GaussianLogLikelihood"; }

private:
    GaussianLogLikelihood(const Self&); //purposely not implemented
    void operator=(const Self&); //purposely not implemented
};

}
