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


#include "sitkPimpleTransform.hxx"

#include "sitkTransform.h"
#include "sitkTemplateFunctions.h"
#include "sitkMemberFunctionFactory.h"
#include "sitkImageConvert.h"

#include "itkTransformBase.h"
#include "itkTransformFactory.h"

#include "itkIdentityTransform.h"
#include "itkTranslationTransform.h"
#include "itkScaleTransform.h"
#include "itkScaleLogarithmicTransform.h"
#include "itkSimilarity2DTransform.h"
#include "itkSimilarity3DTransform.h"
#include "itkEuler2DTransform.h"
#include "itkEuler3DTransform.h"
#include "itkQuaternionRigidTransform.h"
#include "itkVersorTransform.h"
#include "itkVersorRigid3DTransform.h"
#include "itkAffineTransform.h"
#include "itkCompositeTransform.h"

#include "itkDisplacementFieldTransform.h"

#include "itkTransformFileReader.h"
#include "itkTransformFileWriter.h"

#include "itkVectorImage.h"
#include "itkCommand.h"

#include <memory>

#include "nsstd/type_traits.h"

namespace
{

//
// Transform trait class to map to correct special transform type.
template<class T, unsigned int ImageDimension>
class TransformTraits
{
public:
  // undefined types for non-2D/3D
};

template<class T>
class TransformTraits<T,2>
{
public:
  typedef itk::Euler2DTransform<T>  EulerTransformType;
  typedef itk::Similarity2DTransform<T>  SimilarityTransformType;
};

template<class T>
class TransformTraits<T,3>
{
public:
  typedef itk::Euler3DTransform<T>  EulerTransformType;
  typedef itk::Similarity3DTransform<T>  SimilarityTransformType;
};


}

namespace itk
{
namespace simple
{

namespace
{
template<unsigned int Dimension>
bool RegisterMoreTransforms(void)
{
  typedef itk::MatrixOffsetTransformBase<double, Dimension, Dimension> MatrixOffsetTransformType;
  itk::TransformFactory<MatrixOffsetTransformType>::RegisterTransform();
  typedef itk::MatrixOffsetTransformBase<double, Dimension, Dimension> MatrixOffsetTransformType;
  itk::TransformFactory<MatrixOffsetTransformType>::RegisterTransform();
  return true;
}

bool initialized = RegisterMoreTransforms<2>() && RegisterMoreTransforms<3>();


/** \brief An ITK Command class to hold a object until destruction
 *
 * This command is to add is resource management, by utilizing that
 * the lifetime of a Command added to an object is about the same as
 * that object. So this command holds onto a resource or object for
 * lifetime of itself. By adding as a command to an ITK object it will
 * be released on destruction of the ITK object ( subject to the
 * reference counting on the Command ).
 */
template< class T >
class HolderCommand
  : public itk::Command
{
public:
  typedef T ObjectType;

  typedef  HolderCommand Self;
  typedef  itk::Command Superclass;

  typedef itk::SmartPointer<Self>        Pointer;
  typedef itk::SmartPointer<const Self>  ConstPointer;

  itkNewMacro( HolderCommand );

  void Set(const ObjectType object) { this->m_Object = object; }
  ObjectType &Get() {return this->m_Object;}
  const ObjectType &Get() const {return this->m_Object;}

  void Execute(itk::Object*, const itk::EventObject&) {}
  void Execute(const itk::Object*, const itk::EventObject&) {}

protected:
  HolderCommand() {};
  ~HolderCommand() {};

private:
  void operator=(const HolderCommand&); // not implemented
  HolderCommand(const HolderCommand&); // not implemented

  ObjectType m_Object;

};

template< class T >
class HolderCommand<T*>
  : public itk::Command
{
public:
  typedef T ObjectType;

  typedef  HolderCommand Self;
  typedef  itk::Command Superclass;

  typedef itk::SmartPointer<Self>        Pointer;
  typedef itk::SmartPointer<const Self>  ConstPointer;

  itkNewMacro( HolderCommand );

  void Set(ObjectType *object) { this->m_Object = object; }
  ObjectType *Get() {return this->m_Object;}
  const ObjectType *Get() const {return this->m_Object;}

  void Execute(itk::Object*, const itk::EventObject&) {}
  void Execute(const itk::Object*, const itk::EventObject&) {}

protected:
  HolderCommand() : m_Object(NULL) {};
  ~HolderCommand() { delete m_Object;}

private:
  void operator=(const HolderCommand&); // not implemented
  HolderCommand(const HolderCommand&); // not implemented

  ObjectType* m_Object;

};

}


//
// class Transform
//

Transform::Transform( )
  : m_PimpleTransform( NULL )
  {
    m_PimpleTransform = new PimpleTransform<itk::IdentityTransform< double, 3 > >();
  }

  Transform::Transform( unsigned int dimensions, TransformEnum type)
    : m_PimpleTransform( NULL )
  {
    if ( dimensions == 2 )
      {
      this->InternalInitialization<2>(type);
      }
    else if ( dimensions == 3 )
      {
      this->InternalInitialization<3>(type);
      }
    else
      {
      sitkExceptionMacro("Invalid dimension for transform");
      }
  }


  Transform::~Transform()
  {
    delete m_PimpleTransform;
    this->m_PimpleTransform = NULL;
  }

  Transform::Transform( const Transform &txf )
    : m_PimpleTransform( NULL )
  {
    m_PimpleTransform = txf.m_PimpleTransform->ShallowCopy();
  }

  Transform& Transform::operator=( const Transform & txf )
  {
    // note: if txf and this are the same,the following statements
    // will be safe. It's also exception safe.
    std::auto_ptr<PimpleTransformBase> temp( txf.m_PimpleTransform->ShallowCopy() );
    delete this->m_PimpleTransform;
    this->m_PimpleTransform = temp.release();
    return *this;
  }


Transform::Transform( Image &displacement, TransformEnum txType )
  {

    if (txType != sitkDisplacementField)
      {
      sitkExceptionMacro("Expected sitkDisplacementField for the Transformation type!")
      }

    const PixelIDValueEnum type = displacement.GetPixelID();
    const unsigned int dimension = displacement.GetDimension();

    // The pixel IDs supported
    typedef typelist::MakeTypeList<VectorPixelID<double> >::Type PixelIDTypeList;

    typedef void (Self::*MemberFunctionType)( Image & );

    typedef DisplacementInitializationMemberFunctionAddressor<MemberFunctionType> Addressor;

    detail::MemberFunctionFactory<MemberFunctionType> initializationMemberFactory(this);
    initializationMemberFactory.RegisterMemberFunctions< PixelIDTypeList, 3,  Addressor > ();
    initializationMemberFactory.RegisterMemberFunctions< PixelIDTypeList, 2,  Addressor > ();


    initializationMemberFactory.GetMemberFunction( type, dimension )( displacement );

  }

  template< typename TDisplacementType >
  void Transform::InternalDisplacementInitialization( Image & inImage )
  {
    typedef TDisplacementType VectorImageType;

    typedef typename VectorImageType::PixelType PixelType;
    typedef typename VectorImageType::InternalPixelType ComponentType;
    const unsigned int ImageDimension = VectorImageType::ImageDimension;

    typedef itk::Image< itk::Vector<ComponentType, ImageDimension>, ImageDimension > ITKDisplacementType;
    typedef itk::DisplacementFieldTransform< ComponentType, ImageDimension > DisplacementTransformType;

    typename VectorImageType::Pointer image = dynamic_cast < VectorImageType* > ( inImage.GetITKBase() );

    if ( image.IsNull() )
      {
      sitkExceptionMacro( "Unexpected template dispatch error!" );
      }

    typename ITKDisplacementType::Pointer itkDisplacement = GetImageFromVectorImage(image.GetPointer(), true );
    inImage = Image();

    this->m_PimpleTransform = new PimpleTransform< DisplacementTransformType >(itkDisplacement.GetPointer());
  }



void Transform::MakeUniqueForWrite( void )
{
  if ( this->m_PimpleTransform->GetReferenceCount() > 1 )
    {
    std::auto_ptr<PimpleTransformBase> temp( this->m_PimpleTransform->DeepCopy() );
    delete this->m_PimpleTransform;
    this->m_PimpleTransform = temp.release();
    }
}

  template< unsigned int VDimension>
  void  Transform::InternalInitialization(  TransformEnum type, itk::TransformBase *base )
  {
    switch( type )
      {

      case sitkTranslation:
        m_PimpleTransform = new PimpleTransform<itk::TranslationTransform< double, VDimension > >();
        break;
      case sitkScale:
        m_PimpleTransform = new PimpleTransform<itk::ScaleTransform< double, VDimension > >();
        break;
      case sitkScaleLogarithmic:
        m_PimpleTransform = new PimpleTransform<itk::ScaleLogarithmicTransform< double, VDimension > >();
        break;
      case sitkEuler:
        m_PimpleTransform = new PimpleTransform<typename TransformTraits<double,VDimension>::EulerTransformType>();
        break;
      case sitkSimilarity:
        m_PimpleTransform = new PimpleTransform<typename TransformTraits<double,VDimension>::SimilarityTransformType>();
        break;
      case sitkQuaternionRigid:
        if( VDimension != 3)
          {
          sitkExceptionMacro( "A sitkQuaternionRigid Transform only works for 3D!");
          }

        m_PimpleTransform = new PimpleTransform<itk::QuaternionRigidTransform< double > >();
        break;
      case sitkVersor:
        if( VDimension != 3)
          {
          sitkExceptionMacro( "A sitkVersor Transform only works for 3D!");
          }

        m_PimpleTransform = new PimpleTransform<itk::VersorTransform< double > >();
        break;
      case sitkVersorRigid:
        if( VDimension != 3)
          {
          sitkExceptionMacro( "A sitkVersorRigid Transform only works for 3D!");
          }

        m_PimpleTransform = new PimpleTransform<itk::VersorRigid3DTransform< double > >();
        break;
      case sitkAffine:
        m_PimpleTransform = new PimpleTransform<itk::AffineTransform< double, VDimension > >();
        break;
      case sitkComposite:
        {
        typename itk::CompositeTransform<double, VDimension>::Pointer compositeTransform;

        if ( !base )
          {
          compositeTransform = itk::CompositeTransform<double, VDimension>::New();
          }
        else
          {
          compositeTransform = dynamic_cast<itk::CompositeTransform<double, VDimension>*>( base );
          if ( !compositeTransform )
            {
            sitkExceptionMacro("Unexpectedly unable to convert to CompositeTransform" );
            }
          }

        if ( compositeTransform->IsTransformQueueEmpty() )
          {

          // Load an identity transform in case no transforms are loaded.
          typedef itk::IdentityTransform<double, VDimension> IdentityTransformType;
          typename IdentityTransformType::Pointer identityTransform = IdentityTransformType::New();

          compositeTransform->AddTransform( identityTransform );
          }

        compositeTransform->SetAllTransformsToOptimizeOff();
        compositeTransform->SetOnlyMostRecentTransformToOptimizeOn();

        m_PimpleTransform = new PimpleTransform<itk::CompositeTransform<double, VDimension> >( compositeTransform );

        }
        break;
      case sitkDisplacementField:
        // todo print transform type..
        sitkExceptionMacro("Incorrect constructor for transform type.");
      case sitkIdentity:
      default:
        m_PimpleTransform = new PimpleTransform<itk::IdentityTransform< double, VDimension > >();

      }
  }


  itk::TransformBase* Transform::GetITKBase ( void )
  {
    assert( m_PimpleTransform );
    return this->m_PimpleTransform->GetTransformBase();
  }

  const itk::TransformBase* Transform::GetITKBase ( void ) const
  {
    assert( m_PimpleTransform );
    return this->m_PimpleTransform->GetTransformBase();
  }

  unsigned int  Transform::GetDimension( void ) const
  {
    assert( m_PimpleTransform );
    return this->m_PimpleTransform->GetInputDimension();
  }

  void Transform::SetParameters ( const std::vector<double>& parameters )
  {
    assert( m_PimpleTransform );
    this->MakeUniqueForWrite();
    this->m_PimpleTransform->SetParameters( parameters );
  }

  std::vector<double> Transform::GetParameters( void ) const
  {
    assert( m_PimpleTransform );
    return this->m_PimpleTransform->GetParameters();
  }

  void Transform::SetFixedParameters ( const std::vector<double>& parameters )
  {
    assert( m_PimpleTransform );
    this->MakeUniqueForWrite();
    this->m_PimpleTransform->SetFixedParameters( parameters );
  }

  std::vector<double> Transform::GetFixedParameters( void ) const
  {
    assert( m_PimpleTransform );
    return this->m_PimpleTransform->GetFixedParameters();
  }

  Transform &Transform::AddTransform( Transform t )
  {
    assert( m_PimpleTransform );
    this->MakeUniqueForWrite();
    // this returns a pointer which may be the same or a new object
    PimpleTransformBase *temp = this->m_PimpleTransform->AddTransform( t );
    if ( temp != this->m_PimpleTransform )
      {
      delete this->m_PimpleTransform;
      this->m_PimpleTransform = temp;
      }
    return *this;
  }

  std::vector< double > Transform::TransformPoint( const std::vector< double > &point ) const
  {
    assert( m_PimpleTransform );
    return this->m_PimpleTransform->TransformPoint( point );
  }

  std::string Transform::ToString( void ) const
  {
    assert( m_PimpleTransform );
    return this->m_PimpleTransform->ToString();
  }

  Transform ReadTransform( const std::string &filename )
  {
    TransformFileReader::Pointer reader = TransformFileReader::New();
    reader->SetFileName(filename.c_str() );
    reader->Update();

    itk::TransformFileReader::TransformListType *list = reader->GetTransformList();

    if ( list->empty() )
      {
      sitkExceptionMacro( "Read transform file: \"" << filename << "\", but there appears to be not transform in the file!" );
      }

    if( list->front()->GetInputSpaceDimension() == 3
        && list->front()->GetOutputSpaceDimension() == 3 )
      {
      typedef itk::CompositeTransform<double, 3> CompositeTransformType;
      typedef CompositeTransformType::TransformType TransformType;

      CompositeTransformType::Pointer comp;

      // check if transform is a composite
      comp = dynamic_cast<CompositeTransformType*>(list->front().GetPointer());
      if ( comp )
        {
        return Transform( comp.GetPointer() );
        }

      if( list->size() != 1 )
        {
        std::cerr << "Warning: There is more than one tranform in the file! Only using the first transform.\n";
        }


      typedef itk::Transform<double, 3, 3> TransformType;
      TransformType* itktx = dynamic_cast<TransformType*>(list->front().GetPointer());
      if (itktx)
        {
        comp = CompositeTransformType::New();
        comp->ClearTransformQueue();
        comp->AddTransform( itktx );
        return Transform( comp.GetPointer() );
        }

      }

    if( list->front()->GetInputSpaceDimension() == 2
        && list->front()->GetOutputSpaceDimension() == 2)
      {
      typedef itk::CompositeTransform<double, 2> CompositeTransformType;

      CompositeTransformType::Pointer comp;

      // check if transform is a composite
      comp = dynamic_cast<CompositeTransformType*>(list->front().GetPointer());
      if ( comp )
        {
        return Transform( comp.GetPointer() );
        }

      if( list->size() != 1 )
        {
        std::cerr << "Warning: There is more than one tranform in the file! Only using the first transform.\n";
        }

      typedef itk::Transform<double, 2, 2> TransformType;
      TransformType* itktx = dynamic_cast<TransformType*>(list->front().GetPointer());

      if (itktx)
        {
        comp = CompositeTransformType::New();
        comp->ClearTransformQueue();
        comp->AddTransform( itktx );
        return Transform( comp.GetPointer() );
        }

      }

    sitkExceptionMacro( "Unable to transform with InputSpaceDimension: " <<  list->front()->GetInputSpaceDimension()
                        << " and OutputSpaceDimension: " << list->front()->GetOutputSpaceDimension() << "."
                        << "Transform of type " << list->front()->GetNameOfClass() << "is not supported." );

  }

  void Transform::WriteTransform( const std::string &filename ) const
  {
    itk::simple::WriteTransform( *this, filename );
  }

  // write
  void WriteTransform( const Transform &transform, const std::string &filename)
  {
    itk::TransformFileWriter::Pointer writer = itk::TransformFileWriter::New();
    writer->SetFileName(filename.c_str());
    writer->SetInput( transform.GetITKBase() );
    writer->Update();

  }

}
}
