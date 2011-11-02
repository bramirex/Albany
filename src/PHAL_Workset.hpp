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


#ifndef PHAL_WORKSET_HPP
#define PHAL_WORKSET_HPP

#include <list>

#include "Phalanx_ConfigDefs.hpp" // for std::vector
#include "Albany_DataTypes.hpp" 
#include "Epetra_Vector.h"
#include "Epetra_CrsMatrix.h"
#include "Albany_AbstractDiscretization.hpp"
#include "Albany_StateManager.hpp"
#include "Stokhos_OrthogPolyExpansion.hpp"
#include "Stokhos_EpetraVectorOrthogPoly.hpp"
#include "Stokhos_EpetraMultiVectorOrthogPoly.hpp"
#include <Intrepid_FieldContainer.hpp>

#include "PHAL_AlbanyTraits.hpp"
#include "PHAL_TypeKeyMap.hpp"
#include "Teuchos_RCP.hpp"
#include "Teuchos_Comm.hpp"
#include "Epetra_Import.h"

namespace PHAL {

struct Workset {

  typedef AlbanyTraits::EvalTypes ET;
  
  Workset() :
    transientTerms(false), ignore_residual(false) {}

  unsigned int numCells;

  Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,double> > sg_expansion;

  Teuchos::RCP<const Epetra_Vector> x;
  Teuchos::RCP<const Epetra_Vector> xdot;
  Teuchos::RCP<ParamVec> params;
  Teuchos::RCP<const Epetra_MultiVector> Vx;
  Teuchos::RCP<const Epetra_MultiVector> Vxdot;
  Teuchos::RCP<const Epetra_MultiVector> Vp;
  Teuchos::RCP<const Stokhos::EpetraVectorOrthogPoly > sg_x;
  Teuchos::RCP<const Stokhos::EpetraVectorOrthogPoly > sg_xdot;
  Teuchos::RCP<const Stokhos::ProductEpetraVector > mp_x;
  Teuchos::RCP<const Stokhos::ProductEpetraVector > mp_xdot;

  Teuchos::RCP<Epetra_Vector> f;
  Teuchos::RCP<Epetra_CrsMatrix> Jac;
  Teuchos::RCP<Epetra_MultiVector> JV;
  Teuchos::RCP<Epetra_MultiVector> fp;
  Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly > sg_f;
  Teuchos::RCP< Stokhos::VectorOrthogPoly<Epetra_CrsMatrix> > sg_Jac;
  Teuchos::RCP< Stokhos::EpetraMultiVectorOrthogPoly > sg_JV;
  Teuchos::RCP< Stokhos::EpetraMultiVectorOrthogPoly > sg_fp;
  Teuchos::RCP< Stokhos::ProductEpetraVector > mp_f;
  Teuchos::RCP< Stokhos::ProductContainer<Epetra_CrsMatrix> > mp_Jac;
  Teuchos::RCP< Stokhos::ProductEpetraMultiVector > mp_JV;
  Teuchos::RCP< Stokhos::ProductEpetraMultiVector > mp_fp;

  Teuchos::RCP<const Albany::NodeSetList> nodeSets;
  Teuchos::RCP<const Albany::NodeSetCoordList> nodeSetCoords;

  // jacobian and mass matrix coefficients for matrix fill
  double j_coeff;
  double m_coeff;

  // Current Time as defined by Rythmos
  double current_time;
  double previous_time;
 
  // flag indicating whether to sum tangent derivatives, i.e.,
  // compute alpha*df/dxdot*Vxdot + beta*df/dx*Vx + df/dp*Vp or
  // compute alpha*df/dxdot*Vxdot + beta*df/dx*Vx and df/dp*Vp separately
  int num_cols_x;
  int num_cols_p;
  int param_offset;

  std::vector<int> *coord_deriv_indices;

  Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> > >  wsElNodeEqID;
  Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> >  wsCoords;
  Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> > > >  ws_coord_derivs;
  std::string EBName;

  Albany::StateArray* stateArrayPtr;
  Teuchos::RCP<Albany::EigendataStruct> eigenDataPtr;

  bool transientTerms;

  // Flag indicating whether to ignore residual calculations in the 
  // Jacobian calculation.  This only works for some problems where the 
  // the calculation of the Jacobian doesn't require calculation of the
  // residual (such as linear problems), but if it does work it can
  // significantly reduce Jacobian calculation cost.
  bool ignore_residual;

  // Flag indicated whether we are solving the adjoint operator or the
  // forward operator.  This is used in the Albany application when
  // either the Jacobian or the transpose of the Jacobian is scattered. 
  bool is_adjoint;


  // Responses and their derivatives, stored as a separate vector 
  //  or multivector of values for each "Response" requested by user.
  Teuchos::ArrayRCP< Teuchos::RCP< Epetra_Vector > >      responses;
  Teuchos::ArrayRCP< Teuchos::RCP< Epetra_MultiVector > > responseDerivatives;


  // New field manager response stuff
  Teuchos::RCP<const Teuchos::Comm<int> > comm;
  Teuchos::RCP<const Epetra_Import> x_importer;
  Teuchos::RCP<Epetra_Vector> g;
  Teuchos::RCP<Epetra_MultiVector> dgdx;
  Teuchos::RCP<Epetra_MultiVector> dgdxdot;
  Teuchos::RCP<Epetra_MultiVector> overlapped_dgdx;
  Teuchos::RCP<Epetra_MultiVector> overlapped_dgdxdot;
  Teuchos::RCP<Epetra_MultiVector> dgdp;
  Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly > sg_g;
  Teuchos::RCP< Stokhos::EpetraMultiVectorOrthogPoly > sg_dgdx;
  Teuchos::RCP< Stokhos::EpetraMultiVectorOrthogPoly > sg_dgdxdot;
  Teuchos::RCP< Stokhos::EpetraMultiVectorOrthogPoly > overlapped_sg_dgdx;
  Teuchos::RCP< Stokhos::EpetraMultiVectorOrthogPoly > overlapped_sg_dgdxdot;
  Teuchos::RCP< Stokhos::EpetraMultiVectorOrthogPoly > sg_dgdp;
  Teuchos::RCP< Stokhos::ProductEpetraVector > mp_g;
  Teuchos::RCP< Stokhos::ProductEpetraMultiVector > mp_dgdx;
  Teuchos::RCP< Stokhos::ProductEpetraMultiVector > mp_dgdxdot;
  Teuchos::RCP< Stokhos::ProductEpetraMultiVector > overlapped_mp_dgdx;
  Teuchos::RCP< Stokhos::ProductEpetraMultiVector > overlapped_mp_dgdxdot;
  Teuchos::RCP< Stokhos::ProductEpetraMultiVector > mp_dgdp;

  // Meta-function class encoding T<EvalT::ScalarT> given EvalT
  // where T is any lambda expression (typically a placeholder expression)
  template <typename T>
  struct ApplyEvalT {
    template <typename EvalT> struct apply {
      typedef typename boost::mpl::apply<T, typename EvalT::ScalarT>::type type;
    };
  };

  // Meta-function class encoding RCP<ValueTypeSerializer<int,T> > for a given
  // type T.  This is to eliminate an error when using a placeholder expression
  // for the same thing in CreateLambdaKeyMap below
  struct ApplyVTS {
    template <typename T>
    struct apply {
      typedef Teuchos::RCP< Teuchos::ValueTypeSerializer<int,T> > type;
    };
  };

  // mpl::vector mapping evaluation type EvalT to serialization class
  // ValueTypeSerializer<int, EvalT::ScalarT>, which is used for MPI
  // communication of scalar types.
  typedef typename 
    PHAL::CreateLambdaKeyMap<AlbanyTraits::BEvalTypes,
			     ApplyEvalT<ApplyVTS> >::type SerializerMap;

  // Container storing serializers for each evaluation type
  PHAL::TypeKeyMap<SerializerMap> serializerManager;
  
};

}

#endif
