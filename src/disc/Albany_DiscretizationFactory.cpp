//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Albany_DiscretizationFactory.hpp"
#include "Albany_STKDiscretization.hpp"
#include "Albany_TmplSTKMeshStruct.hpp"
#include "Albany_GenericSTKMeshStruct.hpp"
#ifdef ALBANY_SEACAS
#include "Albany_IossSTKMeshStruct.hpp"
#endif
#ifdef ALBANY_CUTR
#include "Albany_FromCubitSTKMeshStruct.hpp"
#endif
#ifdef ALBANY_SCOREC
#include "Albany_FMDBDiscretization.hpp"
#include "Albany_FMDBMeshStruct.hpp"
#endif


Albany::DiscretizationFactory::DiscretizationFactory(
	    const Teuchos::RCP<Teuchos::ParameterList>& discParams_, bool adaptive,
               const Teuchos::RCP<const Epetra_Comm>& epetra_comm_) :
  discParams(discParams_), adaptiveMesh(adaptive), epetra_comm(epetra_comm_)
{
}

#ifdef ALBANY_CUTR
void
Albany::DiscretizationFactory::setMeshMover(const Teuchos::RCP<CUTR::CubitMeshMover>& meshMover_)
{
  meshMover = meshMover_;
}
#endif

Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >
Albany::DiscretizationFactory::createMeshSpecs()
{
  std::string& method = discParams->get("Method", "STK1D");
  if (method == "STK1D") {
    meshStruct = Teuchos::rcp(new Albany::TmplSTKMeshStruct<1>(discParams, adaptiveMesh, epetra_comm));
  }
  else if (method == "STK0D") {
    meshStruct = Teuchos::rcp(new Albany::TmplSTKMeshStruct<0>(discParams, adaptiveMesh, epetra_comm));
  }
  else if (method == "STK2D") {
    meshStruct = Teuchos::rcp(new Albany::TmplSTKMeshStruct<2>(discParams, adaptiveMesh, epetra_comm));
  }
  else if (method == "STK3D") {
    meshStruct = Teuchos::rcp(new Albany::TmplSTKMeshStruct<3>(discParams, adaptiveMesh, epetra_comm));
  }
  else if (method == "Ioss" || method == "Exodus" ||  method == "Pamgen") {
#ifdef ALBANY_SEACAS
    meshStruct = Teuchos::rcp(new Albany::IossSTKMeshStruct(discParams, adaptiveMesh, epetra_comm));
#else
    TEUCHOS_TEST_FOR_EXCEPTION(method == "Ioss" || method == "Exodus" ||  method == "Pamgen",
          Teuchos::Exceptions::InvalidParameter,
         "Error: Discretization method " << method 
          << " requested, but not compiled in" << std::endl);
#endif
  }
  else if (method == "Cubit") {
#ifdef ALBANY_CUTR
    AGS"need to inherit from Generic"
    meshStruct = Teuchos::rcp(new Albany::FromCubitSTKMeshStruct(meshMover, discParams, neq));
#else 
    TEUCHOS_TEST_FOR_EXCEPTION(method == "Cubit", 
          Teuchos::Exceptions::InvalidParameter,
         "Error: Discretization method " << method 
          << " requested, but not compiled in" << std::endl);
#endif
  }
  else if (method == "FMDB") {
#ifdef ALBANY_SCOREC
    meshStruct = Teuchos::rcp(new Albany::FMDBMeshStruct(discParams, epetra_comm));
#else 
    TEUCHOS_TEST_FOR_EXCEPTION(method == "FMDB", 
          Teuchos::Exceptions::InvalidParameter,
         "Error: Discretization method " << method 
          << " requested, but not compiled in" << std::endl);
#endif
  }
  else {
    TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter, std::endl << 
       "Error!  Unknown discretization method in DiscretizationFactory: " << method << 
       "!" << std::endl << "Supplied parameter list is " << std::endl << *discParams 
       << "\nValid Methods are: STK1D, STK2D, STK3D, Ioss, Exodus, Cubit, FMDB" << std::endl);
  }

  return meshStruct->getMeshSpecs();

}

Teuchos::RCP<Albany::AbstractDiscretization>
Albany::DiscretizationFactory::createDiscretization(unsigned int neq,
                           const Teuchos::RCP<Albany::StateInfoStruct>& sis)
{
  TEUCHOS_TEST_FOR_EXCEPTION(meshStruct==Teuchos::null,
       std::logic_error,
       "meshStruct accessed, but it has not been constructed" << std::endl);

  meshStruct->setFieldAndBulkData(epetra_comm, discParams, neq,
                                     sis, meshStruct->getMeshSpecs()[0]->worksetSize);

  switch(meshStruct->meshSpecsType()){

      case Albany::AbstractMeshStruct::STK_MS:
        return Teuchos::rcp(new Albany::STKDiscretization(
               Teuchos::rcp_dynamic_cast<Albany::AbstractSTKMeshStruct>(meshStruct), epetra_comm));
      break;

#ifdef ALBANY_SCOREC
      case Albany::AbstractMeshStruct::FMDB_MS:
        return Teuchos::rcp(new Albany::FMDBDiscretization(
               Teuchos::rcp_dynamic_cast<Albany::FMDBMeshStruct>(meshStruct), epetra_comm));
      break;
#endif

  }

}