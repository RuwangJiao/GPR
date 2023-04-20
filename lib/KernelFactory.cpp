//
// Created by lixin on 2021/5/20.
//
#include "Kernel.h"
#include "KernelFactory.h"

namespace gpr{
    template<> std::shared_ptr< std::map<const std::string, std::shared_ptr< Kernel<float> > (*)(const typename Kernel<float>::StringParameterVectorType&)> > KernelFactory<float>::m_Map( new std::map<const std::string, std::shared_ptr< Kernel<float> > (*)(const typename Kernel<float>::StringParameterVectorType&)>());
    template<> std::shared_ptr< std::map<const std::string, std::shared_ptr< Kernel<double> > (*)(const typename Kernel<double>::StringParameterVectorType&)> > KernelFactory<double>::m_Map( new std::map<const std::string, std::shared_ptr< Kernel<double> > (*)(const typename Kernel<double>::StringParameterVectorType&)>());
    template class KernelFactory<float>;
    template class KernelFactory<double>;
}


