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

#ifndef ALBANY_REDUCEDJACOBIANFACTORY_HPP
#define ALBANY_REDUCEDJACOBIANFACTORY_HPP

#include "Epetra_CrsGraph.h"

class Epetra_Operator;
class Epetra_CrsMatrix;
class Epetra_MultiVector;

#include "Teuchos_RCP.hpp"

namespace Albany {

class ReducedJacobianFactory {
public:
  explicit ReducedJacobianFactory(const Teuchos::RCP<const Epetra_MultiVector> &rightProjector);

  Teuchos::RCP<const Epetra_MultiVector> rightProjector() const { return rightProjector_; }
  Teuchos::RCP<const Epetra_MultiVector> premultipliedRightProjector() const { return premultipliedRightProjector_; }

  void fullJacobianIs(const Epetra_Operator &op);

  Teuchos::RCP<Epetra_CrsMatrix> reducedMatrixNew() const;
  const Epetra_CrsMatrix &reducedMatrix(const Epetra_MultiVector &leftProjector, Epetra_CrsMatrix &result) const;

private:
  Teuchos::RCP<const Epetra_MultiVector> rightProjector_;
  Teuchos::RCP<Epetra_MultiVector> premultipliedRightProjector_;

  Epetra_CrsGraph reducedGraph_;

  bool isMasterProcess() const;
};

} // namespace Albany

#endif /* ALBANY_REDUCEDJACOBIANFACTORY_HPP */