/********************************************************************\
*            Albany, Copyright (2010) Sandia Corporation             *
*                                                                    *
* Notice: This computer software was prepared by Sandia Corporation, *
* hereinafter the Contractor, under Contract DE-AC04-94AL85000 with  *
* the Department of Energy (DOE). All rights in the computer software*
* are reserved by DOE on behalf of the United States Government and  *
* the Contractor as provided in the Contract. You are authorized to  *
* use this computer software for Governmental purposes but it is not *
* to be released or distributed to the public. NEITHER THE GOVERNMENT*
* NOR THE CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR      *
* ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This notice    *
* including this sentence must appear on any copies of this software.*
*    Questions to Andy Salinger, agsalin@sandia.gov                  *
\********************************************************************/


#ifndef QCAD_RESPONSESAVEFIELD_HPP
#define QCAD_RESPONSESAVEFIELD_HPP

#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Albany_ProblemUtils.hpp"

namespace QCAD {
/** 
 * \brief Response Description
 */
  template<typename EvalT, typename Traits>
  class ResponseSaveField : 
    public PHX::EvaluatorWithBaseImpl<Traits>,
    public PHX::EvaluatorDerived<EvalT, Traits>
  {
     public:
    typedef typename EvalT::ScalarT ScalarT;
    ResponseSaveField(Teuchos::ParameterList& p,
		      const Teuchos::RCP<Albany::Layouts>& dl);
  
    void postRegistrationSetup(typename Traits::SetupData d,
			       PHX::FieldManager<Traits>& vm);
  
    void evaluateFields(typename Traits::EvalData d);

    Teuchos::RCP<const PHX::FieldTag> getEvaluatedFieldTag() const {
      return response_field_tag;
    }

    Teuchos::RCP<const PHX::FieldTag> getResponseFieldTag() const {
      return response_field_tag;
    }
    
  private:
    Teuchos::RCP<const Teuchos::ParameterList>
    getValidResponseParameters() const;

    Teuchos::RCP< PHX::Tag<ScalarT> > response_field_tag;
  };

  template<typename Traits>
  class ResponseSaveField<PHAL::AlbanyTraits::Residual, Traits> : 
    public PHX::EvaluatorWithBaseImpl<Traits>,
    public PHX::EvaluatorDerived<PHAL::AlbanyTraits::Residual, Traits>
  {
  public:
    typedef typename PHAL::AlbanyTraits::Residual::ScalarT ScalarT;
    ResponseSaveField(Teuchos::ParameterList& p,
		      const Teuchos::RCP<Albany::Layouts>& dl);
  
    void postRegistrationSetup(typename Traits::SetupData d,
			       PHX::FieldManager<Traits>& vm);
    
    void evaluateFields(typename Traits::EvalData d);

    Teuchos::RCP<const PHX::FieldTag> getEvaluatedFieldTag() const {
      return response_field_tag;
    }

    Teuchos::RCP<const PHX::FieldTag> getResponseFieldTag() const {
      return response_field_tag;
    }
    
  private:
    Teuchos::RCP<const Teuchos::ParameterList> getValidResponseParameters() const;

    std::string fieldName;
    std::string stateName;
    
    std::size_t numQPs;
    std::size_t numDims;
    
    PHX::MDField<ScalarT,Cell,QuadPoint> weights;
    PHX::MDField<ScalarT> field;

    bool outputToExodus;
    bool outputCellAverage;
    bool memoryHolderOnly;
    bool isVectorField;

    std::string vectorOp;

    Teuchos::RCP< PHX::Tag<ScalarT> > response_field_tag;
  };

	
}

#endif
