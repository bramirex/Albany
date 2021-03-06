//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
//
// Program for testing material models in LCM
// Reads in material.xml file and runs at single material point
//

#include <iostream>

#include <Teuchos_GlobalMPISession.hpp>
#include <Teuchos_CommandLineProcessor.hpp>
#include <Teuchos_RCP.hpp>
#include <Teuchos_ParameterList.hpp>
#include <Teuchos_TestForException.hpp>
#include <Teuchos_as.hpp>
#include <QCAD_MaterialDatabase.hpp>
#include <Phalanx.hpp>

#include <PHAL_AlbanyTraits.hpp>
#include <PHAL_SaveStateField.hpp>
#include <Albany_Utils.hpp>
#include <Albany_StateManager.hpp>
#include <Albany_TmplSTKMeshStruct.hpp>
#include <Albany_STKDiscretization.hpp>
#include <Albany_Layouts.hpp>

#include <Intrepid_MiniTensor.h>

#include "FieldNameMap.hpp"

#include "SetField.hpp"

#include "ConstitutiveModelInterface.hpp"
#include "ConstitutiveModelParameters.hpp"
#include "BifurcationCheck.hpp"

bool TpetraBuild = false;

int main(int ac, char* av[])
{

  typedef PHX::MDField<PHAL::AlbanyTraits::Residual::ScalarT>::size_type size_type;
  typedef PHAL::AlbanyTraits::Residual Residual;
  typedef PHAL::AlbanyTraits::Residual::ScalarT ScalarT;
  typedef PHAL::AlbanyTraits Traits;
  std::cout.precision(15);
  //
  // Create a command line processor and parse command line options
  //
  Teuchos::CommandLineProcessor command_line_processor;

  command_line_processor.setDocString("Material Point Simulator.\n"
      "For testing material models in LCM.\n");

  std::string input_file = "materials.xml";
  command_line_processor.setOption("input", &input_file, "Input File Name");

  // Throw a warning and not error for unrecognized options
  command_line_processor.recogniseAllOptions(true);

  // Don't throw exceptions for errors
  command_line_processor.throwExceptions(false);

  // Parse command line
  Teuchos::CommandLineProcessor::EParseCommandLineReturn parse_return =
      command_line_processor.parse(ac, av);

  if (parse_return == Teuchos::CommandLineProcessor::PARSE_HELP_PRINTED) {
    return 0;
  }

  if (parse_return != Teuchos::CommandLineProcessor::PARSE_SUCCESSFUL) {
    return 1;
  }

  //
  // Process material.xml file
  // Read into materialDB and get material model name
  //

  // A mpi object must be instantiated before using the comm to read
  // material file
  Teuchos::GlobalMPISession mpi_session(&ac, &av);
  Teuchos::RCP<const Teuchos_Comm> commT =
    Albany::createTeuchosCommFromMpiComm(Albany_MPI_COMM_WORLD);

  Teuchos::RCP<QCAD::MaterialDatabase> material_db;
  material_db = Teuchos::rcp(new QCAD::MaterialDatabase(input_file, commT));

  // Get the name of the material model to be used (and make sure there is one)
  std::string element_block_name = "Block0";
  std::string material_model_name;
  material_model_name = material_db->getElementBlockSublist(
      element_block_name,
      "Material Model").get<std::string>("Model Name");
  TEUCHOS_TEST_FOR_EXCEPTION(
      material_model_name.length() == 0,
      std::logic_error,
      "A material model must be defined for block: " + element_block_name);

  //
  // Preloading stage setup
  // set up evaluators, create field and state managers
  //

  // Set up the data layout
  const int workset_size = 1;
  const int num_pts = 1;
  const int num_dims = 3;
  const int num_vertices = 8;
  const int num_nodes = 8;
  const Teuchos::RCP<Albany::Layouts> dl = Teuchos::rcp(
      new Albany::Layouts(
          workset_size,
          num_vertices,
          num_nodes,
          num_pts,
          num_dims));

  // create field name strings
  LCM::FieldNameMap field_name_map(false);
  Teuchos::RCP<std::map<std::string, std::string> > fnm =
      field_name_map.getMap();

  //---------------------------------------------------------------------------
  // Deformation gradient
  // initially set the deformation gradient to the identity

  Teuchos::ArrayRCP<ScalarT> def_grad(9);
  for (int i(0); i < 9; ++i)
    def_grad[i] = 0.0;

  def_grad[0] = 1.0;
  def_grad[4] = 1.0;
  def_grad[8] = 1.0;
  // SetField evaluator, which will be used to manually assign a value
  // to the def_grad field
  Teuchos::ParameterList setDefGradP("SetFieldDefGrad");
  setDefGradP.set<std::string>("Evaluated Field Name", "F");
  setDefGradP.set<Teuchos::RCP<PHX::DataLayout> >(
      "Evaluated Field Data Layout",
      dl->qp_tensor);
  setDefGradP.set<Teuchos::ArrayRCP<ScalarT> >("Field Values", def_grad);
  Teuchos::RCP<LCM::SetField<Residual, Traits> > setFieldDefGrad = Teuchos::rcp(
      new LCM::SetField<Residual, Traits>(setDefGradP));

  //---------------------------------------------------------------------------
  // Det(deformation gradient)
  Teuchos::ArrayRCP<ScalarT> detdefgrad(1);
  detdefgrad[0] = 1.0;
  // SetField evaluator, which will be used to manually assign a value
  // to the detdefgrad field
  Teuchos::ParameterList setDetDefGradP("SetFieldDetDefGrad");
  setDetDefGradP.set<std::string>("Evaluated Field Name", "J");
  setDetDefGradP.set<Teuchos::RCP<PHX::DataLayout> >(
      "Evaluated Field Data Layout",
      dl->qp_scalar);
  setDetDefGradP.set<Teuchos::ArrayRCP<ScalarT> >("Field Values", detdefgrad);
  Teuchos::RCP<LCM::SetField<Residual, Traits> > setFieldDetDefGrad =
      Teuchos::rcp(new LCM::SetField<Residual, Traits>(setDetDefGradP));

  //---------------------------------------------------------------------------
  // Small strain tensor
  // initially set the strain tensor to zeros

  Teuchos::ArrayRCP<ScalarT> strain(9);
  for (int i(0); i < 9; ++i)
    strain[i] = 0.0;

  // SetField evaluator, which will be used to manually assign a value
  // to the strain field
  Teuchos::ParameterList setStrainP("SetFieldStrain");
  setStrainP.set<std::string>("Evaluated Field Name", "Strain");
  setStrainP.set<Teuchos::RCP<PHX::DataLayout> >(
      "Evaluated Field Data Layout",
      dl->qp_tensor);
  setStrainP.set<Teuchos::ArrayRCP<ScalarT> >("Field Values", strain);
  Teuchos::RCP<LCM::SetField<Residual, Traits> > setFieldStrain = Teuchos::rcp(
      new LCM::SetField<Residual, Traits>(setStrainP));
  //---------------------------------------------------------------------------
  // Instantiate a field manager
  PHX::FieldManager<Traits> fieldManager;

  // Instantiate a field manager for States
  PHX::FieldManager<Traits> stateFieldManager;

  // Register the evaluators with the field manager
  fieldManager.registerEvaluator<Residual>(setFieldDefGrad);
  fieldManager.registerEvaluator<Residual>(setFieldDetDefGrad);
  fieldManager.registerEvaluator<Residual>(setFieldStrain);

  // Register the evaluators with the state field manager
  stateFieldManager.registerEvaluator<Residual>(setFieldDefGrad);
  stateFieldManager.registerEvaluator<Residual>(setFieldDetDefGrad);
  stateFieldManager.registerEvaluator<Residual>(setFieldStrain);

  // Instantiate a state manager
  Albany::StateManager stateMgr;

  // extract the Material ParameterList for use below
  std::string matName = material_db->getElementBlockParam<std::string>(
      element_block_name,
      "material");
  Teuchos::ParameterList& paramList = material_db->getElementBlockSublist(
      element_block_name,
      matName);
  Teuchos::ParameterList& mpsParams = paramList.sublist(
      "Material Point Simulator");

  // Get loading parameters from .xml file
  std::string load_case = mpsParams.get<std::string>(
      "Loading Case Name",
      "uniaxial");
  int number_steps = mpsParams.get<int>("Number of Steps", 10);
  double step_size = mpsParams.get<double>("Step Size", 1.0e-2);

  std::cout << "Loading parameters:" <<
    "\n  number of steps: " << number_steps <<
    "\n  step_size      : " << step_size << std::endl;

  // determine if temperature is being used
  bool have_temperature = mpsParams.get<bool>("Use Temperature", false);
  std::cout << "have_temp: " << have_temperature << std::endl;
  //---------------------------------------------------------------------------
  // Temperature (optional)
  if (have_temperature) {
    Teuchos::ArrayRCP<ScalarT> temperature(1);
    temperature[0] = mpsParams.get<double>("Temperature", 1.0);
    // SetField evaluator, which will be used to manually assign a value
    // to the detdefgrad field
    Teuchos::ParameterList setTempP("SetFieldTemperature");
    setTempP.set<std::string>("Evaluated Field Name", "Temperature");
    setTempP.set<Teuchos::RCP<PHX::DataLayout> >(
        "Evaluated Field Data Layout",
        dl->qp_scalar);
    setTempP.set<Teuchos::ArrayRCP<ScalarT> >("Field Values", temperature);
    Teuchos::RCP<LCM::SetField<Residual, Traits> > setFieldTemperature =
        Teuchos::rcp(new LCM::SetField<Residual, Traits>(setTempP));
    fieldManager.registerEvaluator<Residual>(setFieldTemperature);
    stateFieldManager.registerEvaluator<Residual>(setFieldTemperature);
  }

  //---------------------------------------------------------------------------
  // Time step
  Teuchos::ArrayRCP<ScalarT> delta_time(1);
  delta_time[0] = step_size;;
  Teuchos::ParameterList setDTP("SetFieldTimeStep");
  setDTP.set<std::string>("Evaluated Field Name", "Delta Time");
  setDTP.set<Teuchos::RCP<PHX::DataLayout> >("Evaluated Field Data Layout",
                                             dl->workset_scalar);
  setDTP.set<Teuchos::ArrayRCP<ScalarT> >("Field Values", delta_time);
  Teuchos::RCP<LCM::SetField<Residual, Traits> > setFieldDT =
    Teuchos::rcp(new LCM::SetField<Residual, Traits>(setDTP));
  fieldManager.registerEvaluator<Residual>(setFieldDT);
  stateFieldManager.registerEvaluator<Residual>(setFieldDT);

  // check if the material wants the tangent to be computed
  bool check_stability;
  check_stability = mpsParams.get<bool>("Check Stability", false);
  paramList.set<bool>("Compute Tangent", check_stability);

  //---------------------------------------------------------------------------
  //std::cout << "// Constitutive Model Parameters"
            //<< std::endl;
  Teuchos::ParameterList cmpPL;
  paramList.set<Teuchos::RCP<std::map<std::string, std::string> > >(
      "Name Map",
      fnm);
  cmpPL.set<Teuchos::ParameterList*>("Material Parameters", &paramList);
  if (have_temperature) {
    cmpPL.set<std::string>("Temperature Name", "Temperature");
    paramList.set<bool>("Have Temperature", true);
  }
  Teuchos::RCP<LCM::ConstitutiveModelParameters<Residual, Traits> > CMP =
      Teuchos::rcp(
          new LCM::ConstitutiveModelParameters<Residual, Traits>(cmpPL, dl));
  fieldManager.registerEvaluator<Residual>(CMP);
  stateFieldManager.registerEvaluator<Residual>(CMP);

  //---------------------------------------------------------------------------
  //std::cout << "// Constitutive Model Interface Evaluator"
           // << std::endl;
  Teuchos::ParameterList cmiPL;
  cmiPL.set<Teuchos::ParameterList*>("Material Parameters", &paramList);
  if (have_temperature) {
    cmiPL.set<std::string>("Temperature Name", "Temperature");
  }
  Teuchos::RCP<LCM::ConstitutiveModelInterface<Residual, Traits> > CMI =
      Teuchos::rcp(
          new LCM::ConstitutiveModelInterface<Residual, Traits>(cmiPL, dl));
  fieldManager.registerEvaluator<Residual>(CMI);
  stateFieldManager.registerEvaluator<Residual>(CMI);

  // Set the evaluated fields as required
  for (std::vector<Teuchos::RCP<PHX::FieldTag> >::const_iterator it = CMI
      ->evaluatedFields().begin(); it != CMI->evaluatedFields().end(); ++it) {
    fieldManager.requireField<Residual>(**it);
  }

  // register state variables
  Teuchos::RCP<Teuchos::ParameterList> p;
  Teuchos::RCP<PHX::Evaluator<Traits> > ev;
  for (int sv(0); sv < CMI->getNumStateVars(); ++sv) {
    CMI->fillStateVariableStruct(sv);
    p = stateMgr.registerStateVariable(
        CMI->getName(),
        CMI->getLayout(),
        dl->dummy,
        element_block_name,
        CMI->getInitType(),
        CMI->getInitValue(),
        CMI->getStateFlag(),
        CMI->getOutputFlag());
    ev = Teuchos::rcp(new PHAL::SaveStateField<Residual, Traits>(*p));
    fieldManager.registerEvaluator<Residual>(ev);
    stateFieldManager.registerEvaluator<Residual>(ev);
  }

  //---------------------------------------------------------------------------
  std::cout << "// Bifurcation Check Evaluator"
            << std::endl;

  if (check_stability) {
    
    std::string parametrization_type = mpsParams.get<std::string>(
      "Parametrization Type",
      "Spherical");
    std::cout << "Parametrization Type: " 
	      << parametrization_type << std::endl;

    Teuchos::ParameterList bcPL;
    bcPL.set<Teuchos::ParameterList*>("Material Parameters", &paramList);
    bcPL.set<std::string>("Parametrization Type Name", parametrization_type);
    bcPL.set<std::string>("Material Tangent Name", "Material Tangent");
    bcPL.set<std::string>("Ellipticity Flag Name", "Ellipticity_Flag");
    bcPL.set<std::string>("Bifurcation Direction Name", "Direction");
    bcPL.set<std::string>("Min detA Name", "Min detA");
    Teuchos::RCP<LCM::BifurcationCheck<Residual, Traits> > BC = Teuchos::rcp(
        new LCM::BifurcationCheck<Residual, Traits>(bcPL, dl));
    fieldManager.registerEvaluator<Residual>(BC);
    stateFieldManager.registerEvaluator<Residual>(BC);

    // register the ellipticity flag
    p = stateMgr.registerStateVariable(
        "Ellipticity_Flag",
        dl->qp_scalar,
        dl->dummy,
        element_block_name,
        "scalar",
        0.0,
        false,
        true);
    ev = Teuchos::rcp(new PHAL::SaveStateField<Residual, Traits>(*p));
    fieldManager.registerEvaluator<Residual>(ev);
    stateFieldManager.registerEvaluator<Residual>(ev);

    // register the direction
    p = stateMgr.registerStateVariable(
        "Direction",
        dl->qp_vector,
        dl->dummy,
        element_block_name,
        "scalar",
        0.0,
        false,
        true);
    ev = Teuchos::rcp(new PHAL::SaveStateField<Residual, Traits>(*p));
    fieldManager.registerEvaluator<Residual>(ev);
    stateFieldManager.registerEvaluator<Residual>(ev);
    
    // register min(det(A))
    p = stateMgr.registerStateVariable(
        "Min detA",
        dl->qp_scalar,
        dl->dummy,
        element_block_name,
        "scalar",
        0.0,
        false,
        true);
    ev = Teuchos::rcp(new PHAL::SaveStateField<Residual, Traits>(*p));
    fieldManager.registerEvaluator<Residual>(ev);
    stateFieldManager.registerEvaluator<Residual>(ev);    
    
  }

  //---------------------------------------------------------------------------
  //std::cout << "// register deformation gradient"
           // << std::endl;
  p = stateMgr.registerStateVariable(
      "F",
      dl->qp_tensor,
      dl->dummy,
      element_block_name,
      "identity",
      1.0,
      false,
      true);
  ev = Teuchos::rcp(new PHAL::SaveStateField<Residual, Traits>(*p));
  fieldManager.registerEvaluator<Residual>(ev);
  stateFieldManager.registerEvaluator<Residual>(ev);
  //---------------------------------------------------------------------------
  //std::cout << "// register small strain tensor"
           // << std::endl;
  p = stateMgr.registerStateVariable(
      "Strain",
      dl->qp_tensor,
      dl->dummy,
      element_block_name,
      "scalar",
      0.0,
      false,
      true);
  ev = Teuchos::rcp(new PHAL::SaveStateField<Residual, Traits>(*p));
  fieldManager.registerEvaluator<Residual>(ev);
  stateFieldManager.registerEvaluator<Residual>(ev);
  //---------------------------------------------------------------------------
  //
  Traits::SetupData setupData = "Test String";
  //std::cout << "Calling postRegistrationSetup" << std::endl;
  fieldManager.postRegistrationSetup(setupData);

  //std::cout << "// set the required fields for the state manager"
            //<< std::endl;
  Teuchos::RCP<PHX::DataLayout> dummy = Teuchos::rcp(
      new PHX::MDALayout<Dummy>(0));
  std::vector<std::string> responseIDs =
    stateMgr.getResidResponseIDsToRequire(element_block_name);
  std::vector<std::string>::const_iterator it;
  for (it = responseIDs.begin(); it != responseIDs.end(); it++) {
    const std::string& responseID = *it;
    PHX::Tag<PHAL::AlbanyTraits::Residual::ScalarT> res_response_tag(
        responseID,
        dummy);
    stateFieldManager.requireField<PHAL::AlbanyTraits::Residual>(
        res_response_tag);
  }
  stateFieldManager.postRegistrationSetup("");

  //std::cout << "Process using 'dot -Tpng -O <name>'\n";
  fieldManager.writeGraphvizFile<Residual>("FM", true, true);
  stateFieldManager.writeGraphvizFile<Residual>("SFM", true, true);

  //---------------------------------------------------------------------------
  // grab the output file name
  //
  std::string output_file = mpsParams.get<std::string>(
      "Output File Name",
      "output.exo");

  //---------------------------------------------------------------------------
  // Create discretization, as required by the StateManager
  //
  Teuchos::RCP<Teuchos::ParameterList> discretizationParameterList =
      Teuchos::rcp(new Teuchos::ParameterList("Discretization"));
  discretizationParameterList->set<int>("1D Elements", workset_size);
  discretizationParameterList->set<int>("2D Elements", 1);
  discretizationParameterList->set<int>("3D Elements", 1);
  discretizationParameterList->set<std::string>("Method", "STK3D");
  discretizationParameterList->set<std::string>(
      "Exodus Output File Name",
      output_file);
  Teuchos::RCP<Tpetra_Map> mapT = 
    Teuchos::rcp(new Tpetra_Map(workset_size*num_dims*num_nodes,
                                0,
                                commT,
                                Tpetra::LocallyReplicated));
  Teuchos::RCP<Tpetra_Vector> solution_vectorT =
    Teuchos::rcp(new Tpetra_Vector(mapT));

  int numberOfEquations = 3;
  Albany::AbstractFieldContainer::FieldContainerRequirements req;

  Teuchos::RCP<Albany::GenericSTKMeshStruct> stkMeshStruct = Teuchos::rcp(
      new Albany::TmplSTKMeshStruct<3>(
          discretizationParameterList,
          Teuchos::null,
          commT));
  stkMeshStruct->setFieldAndBulkData(
      commT,
      discretizationParameterList,
      numberOfEquations,
      req,
      stateMgr.getStateInfoStruct(),
      stkMeshStruct->getMeshSpecs()[0]->worksetSize);

  Teuchos::RCP<Albany::AbstractDiscretization> discretization = Teuchos::rcp(
      new Albany::STKDiscretization(stkMeshStruct, commT));

  //---------------------------------------------------------------------------
  // Associate the discretization with the StateManager
  //
  stateMgr.setStateArrays(discretization);

  //---------------------------------------------------------------------------
  // Create a workset
  //
  PHAL::Workset workset;
  workset.numCells = workset_size;
  workset.stateArrayPtr = &stateMgr.getStateArray(
      Albany::StateManager::ELEM,
      0);

  // create MDFields
  PHX::MDField<ScalarT, Cell, QuadPoint, Dim, Dim> stressField(
      "Cauchy_Stress",
      dl->qp_tensor);

  // construct the final deformation gradient based on the loading case
  std::vector<ScalarT> F_vector(9, 0.0);
  if (load_case == "uniaxial") {
    F_vector[0] = 1.0 + number_steps * step_size;
    F_vector[4] = 1.0;
    F_vector[8] = 1.0;
  } else if (load_case == "simple-shear") {
    F_vector[0] = 1.0;
    F_vector[1] = number_steps * step_size;
    F_vector[4] = 1.0;
    F_vector[8] = 1.0;
  } else if (load_case == "hydrostatic") {
    F_vector[0] = 1.0 + number_steps * step_size;
    F_vector[4] = 1.0 + number_steps * step_size;
    F_vector[8] = 1.0 + number_steps * step_size;
  } else if (load_case == "general") {
    F_vector = mpsParams.get<Teuchos::Array<double> >(
        "Deformation Gradient Components").toVector();
  } else {
    TEUCHOS_TEST_FOR_EXCEPTION(
        true,
        std::runtime_error,
        "Improper Loading Case in Material Point Simulator block");
  }

  Intrepid::Tensor<ScalarT> F_tensor(3, &F_vector[0]);
  Intrepid::Tensor<ScalarT> log_F_tensor = Intrepid::log(F_tensor);

  std::cout << "F\n" << F_tensor << std::endl;
  //std::cout << "log F\n" << log_F_tensor << std::endl;

  //
  // Setup loading scenario and instantiate evaluatFields
  //
  PHX::MDField<ScalarT, Cell, QuadPoint> minDetA("Min detA", dl->qp_scalar);
  double mu_0 = 0;
  double mu_k = 0;
  int bifurcationTime_rough = number_steps;
  bool bifurcation_flag = false;

  for (int istep(0); istep <= number_steps; ++istep) {

    //std::cout << "****** in MPS step " << istep << " ****** " << std::endl;
    // alpha \in [0,1]
    double alpha = double(istep) / number_steps;
    //std::cout << "alpha: " << alpha << std::endl;
    Intrepid::Tensor<ScalarT> scaled_log_F_tensor = alpha * log_F_tensor;
    Intrepid::Tensor<ScalarT> current_F = Intrepid::exp(scaled_log_F_tensor);

    //std::cout << "scaled log F\n" << scaled_log_F_tensor << std::endl;
    //std::cout << "current F\n" << current_F << std::endl;

    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        def_grad[3 * i + j] = current_F(i, j);
      }
    }

    // jacobian
    detdefgrad[0] = Intrepid::det(current_F);

    // small strain tensor
    Intrepid::Tensor<ScalarT> current_strain;
    current_strain = 0.5 * (current_F + Intrepid::transpose(current_F)) 
      - Intrepid::eye<ScalarT>(3);

    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        strain[3 * i + j] = current_strain(i, j);
      }
    }

    //std::cout << "current strain\n" << current_strain << std::endl;

    // Call the evaluators, evaluateFields() is the function that
    // computes stress based on deformation gradient
    fieldManager.preEvaluate<Residual>(workset);
    fieldManager.evaluateFields<Residual>(workset);
    fieldManager.postEvaluate<Residual>(workset);

    stateFieldManager.getFieldData<ScalarT, Residual, Cell, QuadPoint, Dim, Dim>(
        stressField);

    // Check the computed stresses
/*
    for (size_type cell = 0; cell < workset_size; ++cell) {
      for (size_type qp = 0; qp < num_pts; ++qp) {
        std::cout << "in MPS Stress tensor at cell " << cell
            << ", quadrature point " << qp << ":" << std::endl;
        std::cout << "  " << stressField(cell, qp, 0, 0);
        std::cout << "  " << stressField(cell, qp, 0, 1);
        std::cout << "  " << stressField(cell, qp, 0, 2) << std::endl;
        std::cout << "  " << stressField(cell, qp, 1, 0);
        std::cout << "  " << stressField(cell, qp, 1, 1);
        std::cout << "  " << stressField(cell, qp, 1, 2) << std::endl;
        std::cout << "  " << stressField(cell, qp, 2, 0);
        std::cout << "  " << stressField(cell, qp, 2, 1);
        std::cout << "  " << stressField(cell, qp, 2, 2) << std::endl;

        std::cout << std::endl;

      }
    }
*/
    // Call the state field manager
    //std::cout << "+++ calling the stateFieldManager\n";
    stateFieldManager.preEvaluate<Residual>(workset);
    stateFieldManager.evaluateFields<Residual>(workset);
    stateFieldManager.postEvaluate<Residual>(workset);

    stateMgr.updateStates();

    // output to the exodus file
    discretization->writeSolutionT(*solution_vectorT, Teuchos::as<double>(istep));

    // Adaptive step for bifurcation check
    if (check_stability) {
      // get current minDet(A)
      stateFieldManager.getFieldData<ScalarT, Residual, Cell, QuadPoint>(minDetA);
    	
      if (istep == 0) {
	    mu_0 = minDetA(0,0);
      }

      if (minDetA(0, 0) < 0 && !bifurcation_flag) {
        bifurcationTime_rough = istep;
      	bifurcation_flag = true;
      	mu_k = minDetA(0,0);

  	    // adaptive step begin
  	    std::cout << "Adaptive step begin" << std::endl;
        // output results to txt file
  	    std::string output_adaptive_file = mpsParams.get<std::string>(
        "Adaptive Step Output File Name",
        "output-adaptive.txt");
      
        // initialization
  	    double tol = 1E-8;
  	    double alpha_local = 1;
  	    double alpha_local_step = 0.5;
  	    int k = 1;
  	    int maxIteration = 50;

	    std::ofstream fout(output_adaptive_file);
	    fout << "Tol                   : " << tol << std::endl;
	    fout << "Bifurcation_time_rough: " << bifurcationTime_rough << std::endl;
	    fout << "Min_det(A)_0          : " << mu_0 << std::endl;
	    fout << "Min_det(A)_k          : " << mu_k << std::endl;
	    fout << std::endl;
  
  	    fout.width(2);
  	    fout << "k" ;
  	    fout.width(15);
  	    fout << "alpha_local";
  	    fout.width(15);
  	    fout << "mu_k";
  	    fout.width(15);
  	    fout << "mu_k/mu_0" << std::endl;

        // small strain tensor
        Intrepid::Tensor<ScalarT> current_strain;

        // iteration begin
        while ( (mu_k < 0) || (abs(mu_k / mu_0) > tol)) {

          double alpha = (bifurcationTime_rough - 1 + alpha_local)
            / number_steps;

          Intrepid::Tensor<ScalarT> scaled_log_F_tensor = alpha * log_F_tensor;
          Intrepid::Tensor<ScalarT> current_F = Intrepid::exp(scaled_log_F_tensor);

          for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
              def_grad[3 * i + j] = current_F(i, j);
            }
          }

         // jacobian
         detdefgrad[0] = Intrepid::det(current_F);

         current_strain = 0.5 * (current_F + Intrepid::transpose(current_F)) 
    	   - Intrepid::eye<ScalarT>(3);

         for (int i = 0; i < 3; ++i) {
           for (int j = 0; j < 3; ++j) {
             strain[3 * i + j] = current_strain(i, j);
           }
         }

         // Call the evaluators, evaluateFields() is the function that
         // computes stress based on deformation gradient
         fieldManager.preEvaluate<Residual>(workset);
  	     fieldManager.evaluateFields<Residual>(workset);
   	     fieldManager.postEvaluate<Residual>(workset);
    
         // Call the state field manager
         //std::cout << "+++ calling the stateFieldManager\n";
         stateFieldManager.preEvaluate<Residual>(workset);
         stateFieldManager.evaluateFields<Residual>(workset);
         stateFieldManager.postEvaluate<Residual>(workset);
    
         stateFieldManager.getFieldData<ScalarT, Residual, Cell, QuadPoint>
           (minDetA);
         
         mu_k = minDetA(0, 0);
    
         fout.width(2);
         fout << k;
         fout.width(15);
         fout << alpha_local;
         fout.width(15);
         fout << mu_k;
         fout.width(15);
         fout << mu_k/mu_0 << std::endl;

         if (mu_k > 0) {
           alpha_local += alpha_local_step;
         } else {
           alpha_local -= alpha_local_step;
         }
    
         alpha_local_step /= 2;
         k = k+1;
    
         if (k>=maxIteration) break;
         
         stateMgr.updateStates();
        } // iteration end

        fout << std::endl;
        fout << "current strain: \n" << current_strain << std::endl;
        fout << std::flush;
        fout.close();

        std::cout << "Adaptive step end" << std::endl;
     
      } // end adaptive step

    } // end check bifurcation

  } // end loading steps

}
