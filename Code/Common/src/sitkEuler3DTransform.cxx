/*=========================================================================
*
*  Copyright Insight Software Consortium
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*         http://www.apache.org/licenses/LICENSE-2.0.txt
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*
*=========================================================================*/
#include "sitkEuler3DTransform.h"

#include "itkEuler3DTransform.h"

namespace itk
{
namespace simple
{

// construct identity
Euler3DTransform::Euler3DTransform()
  : Transform(3,sitkEuler)
{
  Self::InternalInitialization(Self::GetITKBase());
}

Euler3DTransform::Euler3DTransform(const std::vector<double> &fixedCenter,
                                   double angleX, double angleY, double angleZ,
                                   const std::vector<double> &translation)
  : Transform(3,sitkEuler)

{
  Self::InternalInitialization(Self::GetITKBase());

  this->SetFixedParameters(fixedCenter);
  std::vector<double> params(6);
  params[0] = angleX;
  params[1] = angleY;
  params[2] = angleZ;
  std::copy( translation.begin(), translation.end(), params.begin()+3 );
  this->SetParameters(params);
}

Euler3DTransform::Euler3DTransform( const Euler3DTransform &arg )
  : Transform(arg)
{
  Self::InternalInitialization(Self::GetITKBase());
}


Euler3DTransform &Euler3DTransform::operator=( const Euler3DTransform &arg )
{
  Superclass::operator=(arg);
  return *this;
}


/** fixed parameter */
Euler3DTransform::Self &Euler3DTransform::SetCenter(const std::vector<double> &params)
{
  this->MakeUniqueForWrite();
  this->m_pfSetCenter(params);
  return *this;
}

std::vector<double> Euler3DTransform::GetCenter( ) const
{
  return this->m_pfGetCenter();
}

double Euler3DTransform::GetAngleX () const
{
  return this->m_pfGetAngleX();
}

double Euler3DTransform::GetAngleY () const
{
  return this->m_pfGetAngleY();
}

double Euler3DTransform::GetAngleZ () const
{
  return this->m_pfGetAngleZ();
}

/** parameter */
Euler3DTransform::Self &Euler3DTransform::SetRotation (double angleX, double angleY, double angleZ)
{
  this->MakeUniqueForWrite();
  this->m_pfSetRotation(angleX,angleY,angleZ);
  return *this;
}

Euler3DTransform::Self &Euler3DTransform::SetTranslation(const std::vector<double> &params)
{
  this->MakeUniqueForWrite();
  this->m_pfSetTranslation(params);
  return *this;
}

std::vector<double> Euler3DTransform::GetTranslation( ) const
{
  return this->m_pfGetTranslation();
}


Euler3DTransform::Self &Euler3DTransform::SetComputeZYX (bool _arg)
{
  this->MakeUniqueForWrite();
  this->m_pfSetComputeZYX(_arg);
  return *this;
}

bool Euler3DTransform::GetComputeZYX () const
{
  return this->m_pfGetComputeZYX();
}

void Euler3DTransform::SetPimpleTransform( PimpleTransformBase *pimpleTransform )
{
  Superclass::SetPimpleTransform(pimpleTransform);
  Self::InternalInitialization(this->GetITKBase());
}

void Euler3DTransform::InternalInitialization(itk::TransformBase *transform)
{

  typedef itk::Euler3DTransform<double> TransformType;
  TransformType *t = dynamic_cast<TransformType*>(transform);

  if (t)
    {
    this->InternalInitialization(t);
    return;
    }

  this->m_pfSetCenter = NULL;
  this->m_pfGetCenter = NULL;
  this->m_pfSetTranslation = NULL;
  this->m_pfGetTranslation = NULL;
  this->m_pfSetRotation = NULL;
  this->m_pfGetAngleX = NULL;
  this->m_pfGetAngleY = NULL;
  this->m_pfGetAngleZ = NULL;
  this->m_pfSetComputeZYX = NULL;
  this->m_pfGetComputeZYX = NULL;
}


template<class TransformType>
void Euler3DTransform::InternalInitialization(TransformType *t)
{
  typename TransformType::InputPointType (*pfSTLVectorToITKPoint)(const std::vector<double> &) = &sitkSTLVectorToITK<typename TransformType::InputPointType, double>;
  this->m_pfSetCenter = nsstd::bind(&TransformType::SetCenter,t,nsstd::bind(pfSTLVectorToITKPoint,nsstd::placeholders::_1));

  std::vector<double> (*pfITKPointToSTL)( const typename TransformType::InputPointType &) = &sitkITKVectorToSTL<double,typename TransformType::InputPointType>;
  this->m_pfGetCenter = nsstd::bind(pfITKPointToSTL,nsstd::bind(&TransformType::GetCenter,t));

  typename TransformType::OutputVectorType (*pfSTLVectorToITK)(const std::vector<double> &) = &sitkSTLVectorToITK<typename TransformType::OutputVectorType, double>;
  this->m_pfSetTranslation = nsstd::bind(&TransformType::SetTranslation,t,nsstd::bind(pfSTLVectorToITK,nsstd::placeholders::_1));

  std::vector<double> (*pfITKVectorToSTL)( const typename TransformType::OutputVectorType &) = &sitkITKVectorToSTL<double,typename TransformType::OutputVectorType>;
  this->m_pfGetTranslation = nsstd::bind(pfITKVectorToSTL,nsstd::bind(&TransformType::GetTranslation,t));

  this->m_pfSetRotation = nsstd::bind(&TransformType::SetRotation,t,nsstd::placeholders::_1,nsstd::placeholders::_2,nsstd::placeholders::_3);
  this->m_pfGetAngleX = nsstd::bind(&TransformType::GetAngleX,t);
  this->m_pfGetAngleY = nsstd::bind(&TransformType::GetAngleY,t);
  this->m_pfGetAngleZ = nsstd::bind(&TransformType::GetAngleZ,t);
  this->m_pfSetComputeZYX = nsstd::bind(&TransformType::SetComputeZYX,t,nsstd::placeholders::_1);
  this->m_pfGetComputeZYX = nsstd::bind(&TransformType::GetComputeZYX,t);

}

}
}