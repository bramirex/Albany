//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef AERAS_XZHYDROSTATICRESID_HPP
#define AERAS_XZHYDROSTATICRESID_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Aeras_Layouts.hpp"
#include "Aeras_Dimension.hpp"
#include "Sacado_ParameterAccessor.hpp"

namespace Aeras {
/** \brief XScalarAdvection equation Residual for atmospheric modeling

    This evaluator computes the residual of the XScalarAdvection equation for
    atmospheric dynamics.

*/

template<typename EvalT, typename Traits>
class XZHydrostatic_TracerResid : public PHX::EvaluatorWithBaseImpl<Traits>,
                   public PHX::EvaluatorDerived<EvalT, Traits> {

public:
  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  XZHydrostatic_TracerResid(Teuchos::ParameterList& p,
                        const Teuchos::RCP<Aeras::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);
private:

  // Input:
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint>     wBF;

  PHX::MDField<ScalarT,Cell,QuadPoint,Level>     TracerDot;
  PHX::MDField<ScalarT,Cell,QuadPoint,Level>     TracerSrc;
  PHX::MDField<ScalarT,Cell,QuadPoint,Level>     UTracerDiv;
  PHX::MDField<ScalarT,Cell,QuadPoint,Level>     etadotdTracer;

  // Output:
  PHX::MDField<ScalarT,Cell,Node,Level>          Residual;

  const int numNodes   ;
  const int numQPs     ;
  const int numLevels  ;
};
}

#endif
