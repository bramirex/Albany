//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#include "Albany_ResponseFactory.hpp"

#include "Albany_SolutionAverageResponseFunction.hpp"
#include "Albany_SolutionTwoNormResponseFunction.hpp"
#include "Albany_SolutionValuesResponseFunction.hpp"
#include "Albany_SolutionMaxValueResponseFunction.hpp"
#include "Albany_SolutionFileResponseFunction.hpp"
#ifdef ALBANY_PERIDIGM
#ifdef ALBANY_EPETRA
#include "AlbanyPeridigmOBCFunctional.hpp"
#endif
#endif
#include "Albany_AggregateScalarResponseFunction.hpp"
#include "Albany_FieldManagerScalarResponseFunction.hpp"
#include "Albany_FieldManagerResidualOnlyResponseFunction.hpp"
#if defined(ALBANY_EPETRA)
#include "Albany_SolutionResponseFunction.hpp"
#endif
#include "Albany_KLResponseFunction.hpp"

#ifdef ALBANY_QCAD
#if defined(ALBANY_EPETRA)
#include "QCAD_SaddleValueResponseFunction.hpp"
#endif
#endif

#ifdef ALBANY_GOAL
#include "GOAL_MechAdjResponse.hpp"
#endif

#include "Teuchos_TestForException.hpp"

void
Albany::ResponseFactory::
createResponseFunction(
  const std::string& name,
  Teuchos::ParameterList& responseParams,
  Teuchos::Array< Teuchos::RCP<AbstractResponseFunction> >& responses) const
{
  using std::string;
  using Teuchos::RCP;
  using Teuchos::rcp;
  using Teuchos::ParameterList;
  using Teuchos::Array;

  RCP<const Teuchos_Comm> comm = app->getComm();

  if (name == "Solution Average") {
    responses.push_back(rcp(new Albany::SolutionAverageResponseFunction(comm)));
  }

  else if (name == "Solution Two Norm") {
    responses.push_back(rcp(new Albany::SolutionTwoNormResponseFunction(comm)));
  }
  else if (name == "Solution Values") {
    responses.push_back(rcp(new Albany::SolutionValuesResponseFunction(app, responseParams)));
  }

  else if (name == "Solution Max Value") {
    int eq = responseParams.get("Equation", 0);
    int neq = responseParams.get("Num Equations", 1);
    bool inor =  responseParams.get("Interleaved Ordering", true);

    responses.push_back(
      rcp(new Albany::SolutionMaxValueResponseFunction(comm, neq, eq, inor)));
  }

  else if (name == "Solution Two Norm File") {
    responses.push_back(
      rcp(new Albany::SolutionFileResponseFunction<Albany::NormTwo>(comm)));
  }

  else if (name == "Solution Inf Norm File") {
    responses.push_back(
      rcp(new Albany::SolutionFileResponseFunction<Albany::NormInf>(comm)));
  }

  else if (name == "OBC Functional") {
#ifdef ALBANY_PERIDIGM
#ifdef ALBANY_EPETRA
    responses.push_back(rcp(new Albany::AlbanyPeridigmOBCFunctional(comm)));
#endif
#endif
  }

  else if (name == "Aggregated") {
    int num_aggregate_responses = responseParams.get<int>("Number");
    Array< RCP<AbstractResponseFunction> > aggregated_responses;
    Array< RCP<ScalarResponseFunction> > scalar_responses;
    for (int i=0; i<num_aggregate_responses; i++) {
      std::string id = Albany::strint("Response",i);
      std::string name = responseParams.get<std::string>(id);
      std::string sublist_name = Albany::strint("ResponseParams",i);
      createResponseFunction(name, responseParams.sublist(sublist_name),
			     aggregated_responses);

    }
    scalar_responses.resize(aggregated_responses.size());
    for (int i=0; i<aggregated_responses.size(); i++) {
      TEUCHOS_TEST_FOR_EXCEPTION(
	aggregated_responses[i]->isScalarResponse() != true, std::logic_error,
	"Response function " << i << " is not a scalar response function." <<
	std::endl <<
	"The aggregated response can only aggregate scalar response " <<
	"functions!");
      scalar_responses[i] =
	Teuchos::rcp_dynamic_cast<ScalarResponseFunction>(
	  aggregated_responses[i]);
    }
    responses.push_back(
      rcp(new Albany::AggregateScalarResponseFunction(comm, scalar_responses)));
  }

  else if (name == "Field Integral" ||
	   name == "Field Value" ||
	   name == "Field Average" ||
	   name == "Surface Velocity Mismatch" ||
           name == "Aeras Shallow Water L2 Error" ||
           name == "Aeras Total Volume" ||
	   name == "Center Of Mass" ||
	   name == "Save Field" ||
	   name == "Region Boundary" ||
	   name == "Element Size Field" ||
	   name == "Save Nodal Fields" ||
	   name == "Stiffness Objective" ||
	   name == "Internal Energy Objective" ||
	   name == "Tensor PNorm Objective" ||
	   name == "Modal Objective" ||
           name == "PHAL Field Integral" ||
           name == "PHAL Field IntegralT") {
    responseParams.set("Name", name);
    for (int i=0; i<meshSpecs.size(); i++) {
#if defined(ALBANY_LCM)
      // Skip if dealing with interface block
      //if (meshSpecs[i]->ebName == "Surface Element") continue;
#endif
      responses.push_back(
          rcp(new Albany::FieldManagerScalarResponseFunction(
              app, prob, meshSpecs[i], stateMgr, responseParams)));
    }
  }

  else if (name == "IP to Nodal Field" ||
           name == "Project IP to Nodal Field") {
    responseParams.set("Name", name);
    for (int i=0; i<meshSpecs.size(); i++) {
#if defined(ALBANY_LCM)
      // Skip if dealing with interface block
      //if (meshSpecs[i]->ebName == "Surface Element") continue;
#endif
      // For these RFs, default to true for this parm.
      //eb-todo But not yet for ProjectIPtoNodalField.
      const char* reb_parm = "Restrict to Element Block";
      if ( ! responseParams.isType<bool>(reb_parm) &&
          name == "IP to Nodal Field" /* for now just this one */)
        responseParams.set<bool>(reb_parm, true);
      responses.push_back(
        rcp(new Albany::FieldManagerResidualOnlyResponseFunction(
              app, prob, meshSpecs[i], stateMgr, responseParams)));
    }
  }

  else if (name == "Solution") {
#if defined(ALBANY_EPETRA)
    responses.push_back(
      rcp(new Albany::SolutionResponseFunction(app, responseParams)));
#endif
  }

  else if (name == "KL") {
    Array< RCP<AbstractResponseFunction> > base_responses;
    std::string name = responseParams.get<std::string>("Response");
    createResponseFunction(name, responseParams.sublist("ResponseParams"),
			   base_responses);
    for (int i=0; i<base_responses.size(); i++)
      responses.push_back(
	rcp(new Albany::KLResponseFunction(base_responses[i], responseParams)));
  }

#ifdef ALBANY_QCAD
#if defined(ALBANY_EPETRA)
  else if (name == "Saddle Value") {
    responseParams.set("Name", name);
    for (int i=0; i<meshSpecs.size(); i++) {
      responses.push_back(
	rcp(new QCAD::SaddleValueResponseFunction(
	      app, prob, meshSpecs[i], stateMgr, responseParams)));
    }
  }
#endif
#endif

#ifdef ALBANY_GOAL
  else if (name == "Mechanics Adjoint") {
    responseParams.set("Name", name);
    responses.push_back( rcp( new GOAL::MechAdjResponse(
            app,prob,stateMgr,meshSpecs,responseParams)));
  }
#endif

  else {
    TEUCHOS_TEST_FOR_EXCEPTION(
      true, Teuchos::Exceptions::InvalidParameter,
      std::endl << "Error!  Unknown response function " << name <<
      "!" << std::endl << "Supplied parameter list is " <<
      std::endl << responseParams);
  }
}

Teuchos::Array< Teuchos::RCP<Albany::AbstractResponseFunction> >
Albany::ResponseFactory::
createResponseFunctions(Teuchos::ParameterList& responseList) const
{
  using Teuchos::RCP;
  using Teuchos::rcp;
  using Teuchos::ParameterList;
  using Teuchos::Array;

  // First check for the old response specification
  if (responseList.isType<int>("Number")) {
    int num_aggregate_responses = responseList.get<int>("Number");
    if (num_aggregate_responses > 0) {
      Array<RCP<AbstractResponseFunction> > responses;
      createResponseFunction("Aggregated", responseList, responses);
      return responses;
    }
  }

  int num_response_vecs = responseList.get("Number of Response Vectors", 0);
  Array<RCP<AbstractResponseFunction> > responses;

  for (int i=0; i<num_response_vecs; i++) {
    std::string sublist_name = Albany::strint("Response Vector",i);
    ParameterList& response_params =
      responseList.sublist(sublist_name);
    std::string response_name = response_params.get<std::string>("Name");
    createResponseFunction(response_name, response_params, responses);
  }

  return responses;
}
