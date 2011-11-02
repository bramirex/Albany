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


#ifndef PHAL_SEPARABLE_SCATTER_SCALAR_RESPONSE_HPP
#define PHAL_SEPARABLE_SCATTER_SCALAR_RESPONSE_HPP

#include "PHAL_ScatterScalarResponse.hpp"

namespace PHAL {

/** \brief Handles scattering of separable scalar response functions into epetra
 * data structures.
 * 
 * Base implementation useable by specializations below
 */
template<typename EvalT, typename Traits> 
class SeparableScatterScalarResponseBase
  : public virtual PHX::EvaluatorWithBaseImpl<Traits>,
    public virtual PHX::EvaluatorDerived<EvalT, Traits>  {
  
public:
  
  SeparableScatterScalarResponseBase(const Teuchos::ParameterList& p,
			       const Teuchos::RCP<Albany::Layouts>& dl);
  
  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d) {}
  
protected:

  // Default constructor for child classes
  SeparableScatterScalarResponseBase() {}

  // Child classes should call setup once p is filled out
  void setup(const Teuchos::ParameterList& p,
	     const Teuchos::RCP<Albany::Layouts>& dl);

protected:

  typedef typename EvalT::ScalarT ScalarT;
  PHX::MDField<ScalarT> local_response;
};
  
/** \brief Handles scattering of separable scalar response functions into epetra
 * data structures.
 * 
 * A separable response function is one that is a sum of respones across cells.
 * In this case we can compute the Jacobian in a generic fashion.
 */
template <typename EvalT, typename Traits> 
class SeparableScatterScalarResponse : 
    public ScatterScalarResponse<EvalT, Traits>,
    public SeparableScatterScalarResponseBase<EvalT,Traits> {
public:
  SeparableScatterScalarResponse(const Teuchos::ParameterList& p,
			   const Teuchos::RCP<Albany::Layouts>& dl) :
    ScatterScalarResponse<EvalT,Traits>(p,dl) {}

  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm) {
    ScatterScalarResponse<EvalT, Traits>::postRegistrationSetup(d,vm);
    SeparableScatterScalarResponseBase<EvalT,Traits>::postRegistrationSetup(d,vm);
  }

  void evaluateFields(typename Traits::EvalData d) {}
    
protected:
  SeparableScatterScalarResponse() {}
  void setup(const Teuchos::ParameterList& p,
	     const Teuchos::RCP<Albany::Layouts>& dl) {
    ScatterScalarResponse<EvalT,Traits>::setup(p,dl);
    SeparableScatterScalarResponseBase<EvalT,Traits>::setup(p,dl);
  }
};

// **************************************************************
// **************************************************************
// * Specializations
// **************************************************************
// **************************************************************

// **************************************************************
// Jacobian
// **************************************************************
template<typename Traits>
class SeparableScatterScalarResponse<PHAL::AlbanyTraits::Jacobian,Traits>
  : public ScatterScalarResponseBase<PHAL::AlbanyTraits::Jacobian, Traits>,
    public SeparableScatterScalarResponseBase<PHAL::AlbanyTraits::Jacobian, Traits> {
public:
  SeparableScatterScalarResponse(const Teuchos::ParameterList& p,
			   const Teuchos::RCP<Albany::Layouts>& dl);
  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm) {
    ScatterScalarResponseBase<EvalT, Traits>::postRegistrationSetup(d,vm);
    SeparableScatterScalarResponseBase<EvalT,Traits>::postRegistrationSetup(d,vm);
  }
  void preEvaluate(typename Traits::PreEvalData d);
  void evaluateFields(typename Traits::EvalData d);
  void postEvaluate(typename Traits::PostEvalData d);
protected:
  typedef PHAL::AlbanyTraits::Jacobian EvalT;
  SeparableScatterScalarResponse() {}
  void setup(const Teuchos::ParameterList& p,
	     const Teuchos::RCP<Albany::Layouts>& dl) {
    ScatterScalarResponseBase<EvalT,Traits>::setup(p,dl);
    SeparableScatterScalarResponseBase<EvalT,Traits>::setup(p,dl);
    numNodes = dl->node_scalar->dimension(1);
  }
private:
  typedef typename PHAL::AlbanyTraits::Jacobian::ScalarT ScalarT;
  int numNodes;
};

// **************************************************************
// Stochastic Galerkin Jacobian
// **************************************************************
template<typename Traits>
class SeparableScatterScalarResponse<PHAL::AlbanyTraits::SGJacobian,Traits>
  : public ScatterScalarResponseBase<PHAL::AlbanyTraits::SGJacobian, Traits>,
    public SeparableScatterScalarResponseBase<PHAL::AlbanyTraits::SGJacobian, Traits>{
public:
  SeparableScatterScalarResponse(const Teuchos::ParameterList& p,
			   const Teuchos::RCP<Albany::Layouts>& dl);
  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm) {
    ScatterScalarResponseBase<EvalT, Traits>::postRegistrationSetup(d,vm);
    SeparableScatterScalarResponseBase<EvalT,Traits>::postRegistrationSetup(d,vm);
  }
  void preEvaluate(typename Traits::PreEvalData d);
  void evaluateFields(typename Traits::EvalData d);
  void postEvaluate(typename Traits::PostEvalData d);
protected:
  typedef PHAL::AlbanyTraits::SGJacobian EvalT;
  SeparableScatterScalarResponse() {}
  void setup(const Teuchos::ParameterList& p,
	     const Teuchos::RCP<Albany::Layouts>& dl) {
    ScatterScalarResponseBase<EvalT,Traits>::setup(p,dl);
    SeparableScatterScalarResponseBase<EvalT,Traits>::setup(p,dl);
    numNodes = dl->node_scalar->dimension(1);
  }
private:
  typedef typename PHAL::AlbanyTraits::SGJacobian::ScalarT ScalarT;
  int numNodes;
};

// **************************************************************
// Multi-point Jacobian
// **************************************************************
template<typename Traits>
class SeparableScatterScalarResponse<PHAL::AlbanyTraits::MPJacobian,Traits>
  : public ScatterScalarResponseBase<PHAL::AlbanyTraits::MPJacobian, Traits>,
    public SeparableScatterScalarResponseBase<PHAL::AlbanyTraits::MPJacobian, Traits>{
public:
  SeparableScatterScalarResponse(const Teuchos::ParameterList& p,
			   const Teuchos::RCP<Albany::Layouts>& dl);
  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm) {
    ScatterScalarResponseBase<EvalT, Traits>::postRegistrationSetup(d,vm);
    SeparableScatterScalarResponseBase<EvalT,Traits>::postRegistrationSetup(d,vm);
  }
  void preEvaluate(typename Traits::PreEvalData d);
  void evaluateFields(typename Traits::EvalData d);
  void postEvaluate(typename Traits::PostEvalData d);
protected:
  typedef PHAL::AlbanyTraits::MPJacobian EvalT;
  SeparableScatterScalarResponse() {}
  void setup(const Teuchos::ParameterList& p,
	     const Teuchos::RCP<Albany::Layouts>& dl) {
    ScatterScalarResponseBase<EvalT,Traits>::setup(p,dl);
    SeparableScatterScalarResponseBase<EvalT,Traits>::setup(p,dl);
    numNodes = dl->node_scalar->dimension(1);
  }
private:
  typedef typename PHAL::AlbanyTraits::MPJacobian::ScalarT ScalarT;
  int numNodes;
};

// **************************************************************
}

#endif
