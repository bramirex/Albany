//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Albany_Utils.hpp"
#include <boost/math/special_functions/fpclassify.hpp>
#include "LocalNonlinearSolver.hpp"

//#define  PRINT_DEBUG
//#define  PRINT_OUTPUT
//#define  DECOUPLE
//#define LINE_SEARCH

#include <typeinfo>
#include <iostream>
#include <Sacado_Traits.hpp>
namespace LCM
{

template<typename EvalT, typename Traits>
CrystalPlasticityModel<EvalT, Traits>::
CrystalPlasticityModel(Teuchos::ParameterList* p,
    const Teuchos::RCP<Albany::Layouts>& dl) :
    LCM::ConstitutiveModel<EvalT, Traits>(p, dl),
    num_slip_(p->get<int>("Number of Slip Systems", 0))
{
  integration_scheme_ = EXPLICIT;
  if(p->isParameter("Integration Scheme")){
    std::string integrationSchemeString = p->get<std::string>("Integration Scheme");
    if(integrationSchemeString == "Implicit"){
      integration_scheme_ = IMPLICIT;
    }
    else if(integrationSchemeString == "Explicit"){
      integration_scheme_ = EXPLICIT;
    }
    else{
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "\n**** Error in CrystalPlasticityModel, invalid value for\"Integration Scheme\", must be \"Implicit\" or \"Explicit\".\n");
    }
  }
  implicit_nonlinear_solver_relative_tolerance_ = p->get<double>("Implicit Integration Relative Tolerance", 1.0e-6);
  implicit_nonlinear_solver_absolute_tolerance_ = p->get<double>("Implicit Integration Absolute Tolerance", 1.0e-10);
  implicit_nonlinear_solver_max_iterations_ = p->get<int>("Implicit Integration Max Iterations", 100);

  slip_systems_.resize(num_slip_);

#ifdef PRINT_DEBUG
  std::cout << ">>> in cp constructor\n";
  std::cout << ">>> parameter list:\n" << *p << std::endl;
#endif

  Teuchos::ParameterList e_list = p->sublist("Crystal Elasticity");
  // assuming cubic symmetry
  c11_ = e_list.get<RealType>("C11");
  c12_ = e_list.get<RealType>("C12");
  c44_ = e_list.get<RealType>("C44");
  Intrepid::Tensor4<RealType> C(num_dims_);
  C.fill(Intrepid::ZEROS);
  for (int i = 0; i < num_dims_; ++i) {
    C(i, i, i, i) = c11_;
    for (int j = i + 1; j < num_dims_; ++j) {
      C(i, i, j, j) = C(j, j, i, i) = c12_;
      C(i, j, i, j) = C(j, i, j, i) = C(i, j, j, i) = C(j, i, i, j) = c44_;
    }
  }
// NOTE check if basis is given else default
// NOTE default to coordinate axes and also construct 3rd direction if only 2 given
  orientation_.set_dimension(num_dims_);
  for (int i = 0; i < num_dims_; ++i) {
    std::vector < RealType > b_temp = e_list.get<Teuchos::Array<RealType> >(
        Albany::strint("Basis Vector", i + 1)).toVector();
    RealType norm = 0.;
    for (int j = 0; j < num_dims_; ++j) {
      norm += b_temp[j] * b_temp[j];
    }
// NOTE check zero, rh system
// Filling columns of transformation with basis vectors
// We are forming R^{T} which is equivalent to the direction cosine matrix
    norm = 1. / std::sqrt(norm);
    for (int j = 0; j < num_dims_; ++j) {
      orientation_(j, i) = b_temp[j] * norm;
    }
  }

// print rotation tensor employed for transformations
#ifdef PRINT_DEBUG
  std::cout << ">>> orientation_ :\n" << orientation_ << std::endl;
#endif

  // rotate elastic tensor and slip systems to match given orientation
  C_ = Intrepid::kronecker(orientation_, C);
  for (int num_ss = 0; num_ss < num_slip_; ++num_ss) {
    Teuchos::ParameterList ss_list = p->sublist(
        Albany::strint("Slip System", num_ss + 1));

    // Obtain and normalize slip directions. Miller indices need to be normalized.
    std::vector < RealType > s_temp = ss_list.get<Teuchos::Array<RealType> >(
        "Slip Direction").toVector();
    Intrepid::Vector<RealType> s_temp_normalized(num_dims_, &s_temp[0]);
    s_temp_normalized = Intrepid::unit(s_temp_normalized);
    slip_systems_[num_ss].s_ = orientation_ * s_temp_normalized;

    // Obtain and normal slip normals. Miller indices need to be normalized.
    std::vector < RealType > n_temp = ss_list.get<Teuchos::Array<RealType> >(
        "Slip Normal").toVector();
    Intrepid::Vector<RealType> n_temp_normalized(num_dims_, &n_temp[0]);
    n_temp_normalized = Intrepid::unit(n_temp_normalized);
    slip_systems_[num_ss].n_ = orientation_ * n_temp_normalized;

    // print each slip direction and slip normal after transformation
#ifdef PRINT_DEBUG
    std::cout << ">>> slip direction " << num_ss + 1 << ": " << slip_systems_[num_ss].s_ << std::endl;
    std::cout << ">>> slip normal " << num_ss + 1 << ": " << slip_systems_[num_ss].n_ << std::endl;
#endif

    slip_systems_[num_ss].projector_ = Intrepid::dyad(
        slip_systems_[num_ss].s_,
        slip_systems_[num_ss].n_);

    // print projector
#ifdef PRINT_DEBUG
    std::cout << ">>> projector_ " << num_ss + 1 << ": " << slip_systems_[num_ss].projector_ << std::endl;
#endif

    slip_systems_[num_ss].tau_critical_ = ss_list.get<RealType>("Tau Critical");
    TEUCHOS_TEST_FOR_EXCEPTION(slip_systems_[num_ss].tau_critical_ <= 0, std::logic_error, "\n**** Error in CrystalPlasticityModel, invalid value for Tau Critical\n");
    slip_systems_[num_ss].gamma_dot_0_ = ss_list.get<RealType>("Gamma Dot");
    slip_systems_[num_ss].gamma_exp_ = ss_list.get<RealType>("Gamma Exponent");
    slip_systems_[num_ss].H_ = ss_list.get<RealType>("Hardening", 0.0);
    slip_systems_[num_ss].p_ = ss_list.get<RealType>("Hardening Exponent",1.0);
  }
#ifdef PRINT_DEBUG
  std::cout << "<<< done with parameter list\n";
#endif

  // retrive appropriate field name strings (ref to problems/FieldNameMap)
  std::string eqps_string = (*field_name_map_)["eqps"];
  std::string Re_string = (*field_name_map_)["Re"];
  std::string cauchy_string = (*field_name_map_)["Cauchy_Stress"];
  std::string Fp_string = (*field_name_map_)["Fp"];
  std::string L_string = (*field_name_map_)["Velocity_Gradient"];
  std::string F_string = (*field_name_map_)["F"];
  std::string J_string = (*field_name_map_)["J"];
  std::string source_string = (*field_name_map_)["Mechanical_Source"];
  std::string residual_string = (*field_name_map_)["CP_Residual"];

  // define the dependent fields
  // required for calculation
  this->dep_field_map_.insert(std::make_pair(F_string, dl->qp_tensor));
  this->dep_field_map_.insert(std::make_pair(J_string, dl->qp_scalar));
  this->dep_field_map_.insert(std::make_pair("Delta Time", dl->workset_scalar));

  // define the evaluated fields
  // optional output
  this->eval_field_map_.insert(std::make_pair(eqps_string, dl->qp_scalar));
  this->eval_field_map_.insert(std::make_pair(Re_string, dl->qp_tensor));
  this->eval_field_map_.insert(std::make_pair(cauchy_string, dl->qp_tensor));
  this->eval_field_map_.insert(std::make_pair(Fp_string, dl->qp_tensor));
  this->eval_field_map_.insert(std::make_pair(L_string, dl->qp_tensor));
  this->eval_field_map_.insert(std::make_pair(source_string, dl->qp_scalar));
  this->eval_field_map_.insert(std::make_pair(residual_string, dl->qp_scalar));
  this->eval_field_map_.insert(std::make_pair("Time", dl->workset_scalar));

  // define the state variables
  //
  // eqps
  this->num_state_variables_++;
  this->state_var_names_.push_back(eqps_string);
  this->state_var_layouts_.push_back(dl->qp_scalar);
  this->state_var_init_types_.push_back("scalar");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(false);
  this->state_var_output_flags_.push_back(
      p->get<bool>("Output EQPS", false));
  //
  // Re
  this->num_state_variables_++;
  this->state_var_names_.push_back(Re_string);
  this->state_var_layouts_.push_back(dl->qp_tensor);
  this->state_var_init_types_.push_back("identity");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(false);
  this->state_var_output_flags_.push_back(p->get<bool>("Output Re", false));
  //
  // stress
  this->num_state_variables_++;
  this->state_var_names_.push_back(cauchy_string);
  this->state_var_layouts_.push_back(dl->qp_tensor);
  this->state_var_init_types_.push_back("scalar");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(false);
  this->state_var_output_flags_.push_back(
      p->get<bool>("Output Cauchy Stress", false));
  //
  // Fp
  this->num_state_variables_++;
  this->state_var_names_.push_back(Fp_string);
  this->state_var_layouts_.push_back(dl->qp_tensor);
  this->state_var_init_types_.push_back("identity");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(true);
  this->state_var_output_flags_.push_back(p->get<bool>("Output Fp", false));
  //
  // L
  this->num_state_variables_++;
  this->state_var_names_.push_back(L_string);
  this->state_var_layouts_.push_back(dl->qp_tensor);
  this->state_var_init_types_.push_back("identity");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(true);
  this->state_var_output_flags_.push_back(p->get<bool>("Output L", false));
  //
  // mechanical source (body force)
  this->num_state_variables_++;
  this->state_var_names_.push_back(source_string);
  this->state_var_layouts_.push_back(dl->qp_scalar);
  this->state_var_init_types_.push_back("scalar");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(false);
  this->state_var_output_flags_.push_back(
      p->get<bool>("Output Mechanical Source", false));
  //
  // gammas for each slip system
  for (int num_ss = 0; num_ss < num_slip_; ++num_ss) {
    std::string g = Albany::strint("gamma", num_ss + 1, '_');
    std::string gamma_string = (*field_name_map_)[g];
    std::string output_gamma_string = "Output " + gamma_string;
    this->eval_field_map_.insert(std::make_pair(gamma_string, dl->qp_scalar));
    this->num_state_variables_++;
    this->state_var_names_.push_back(gamma_string);
    this->state_var_layouts_.push_back(dl->qp_scalar);
    this->state_var_init_types_.push_back("scalar");
    this->state_var_init_values_.push_back(0.0);
    this->state_var_old_state_flags_.push_back(true);
    this->state_var_output_flags_.push_back(
        p->get<bool>(output_gamma_string, false));
  }
  // tau_hard - state variable for hardening on each slip system
  for (int num_ss = 0; num_ss < num_slip_; ++num_ss) {
    std::string t_h = Albany::strint("tau_hard", num_ss + 1, '_');
    std::string tau_hard_string = (*field_name_map_)[t_h];
    std::string output_tau_hard_string = "Output " + tau_hard_string;
    this->eval_field_map_.insert(
        std::make_pair(tau_hard_string, dl->qp_scalar));
    this->num_state_variables_++;
    this->state_var_names_.push_back(tau_hard_string);
    this->state_var_layouts_.push_back(dl->qp_scalar);
    this->state_var_init_types_.push_back("scalar");
    this->state_var_init_values_.push_back(0.0);
    this->state_var_old_state_flags_.push_back(true);
    this->state_var_output_flags_.push_back(
        p->get<bool>(output_tau_hard_string, false));
  }
  //
  // taus - output resolved shear stress for debugging - not stated
  for (int num_ss = 0; num_ss < num_slip_; ++num_ss) {
    std::string t = Albany::strint("tau", num_ss + 1, '_');
    std::string tau_string = (*field_name_map_)[t];
    std::string output_tau_string = "Output " + tau_string;
    this->eval_field_map_.insert(std::make_pair(tau_string, dl->qp_scalar));
    this->num_state_variables_++;
    this->state_var_names_.push_back(tau_string);
    this->state_var_layouts_.push_back(dl->qp_scalar);
    this->state_var_init_types_.push_back("scalar");
    this->state_var_init_values_.push_back(0.0);
    this->state_var_old_state_flags_.push_back(false);
    this->state_var_output_flags_.push_back(
        p->get<bool>(output_tau_string, false));
  }
  // residual
  this->num_state_variables_++;
  this->state_var_names_.push_back(residual_string);
  this->state_var_layouts_.push_back(dl->qp_scalar);
  this->state_var_init_types_.push_back("scalar");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(false);
  this->state_var_output_flags_.push_back(
        p->get<bool>("Output CP_Residual", false));

#ifdef PRINT_DEBUG
  std::cout << "<<< done in cp constructor\n";
#endif
}

//------------------------------------------------------------------------------

template<typename EvalT, typename Traits>
void CrystalPlasticityModel<EvalT, Traits>::
computeState(typename Traits::EvalData workset,
    std::map<std::string, Teuchos::RCP<PHX::MDField<ScalarT> > > dep_fields,
    std::map<std::string, Teuchos::RCP<PHX::MDField<ScalarT> > > eval_fields)
{

bool print_debug = false;
#ifdef PRINT_DEBUG
  if (typeid(ScalarT) == typeid(RealType)) {
    print_debug = true;
  }
  std::cout.precision(15);
#endif

#ifdef PRINT_DEBUG
  std::cout << ">>> in cp compute state\n";
#endif
  // retrive appropriate field name strings
  std::string eqps_string = (*field_name_map_)["eqps"];
  std::string Re_string = (*field_name_map_)["Re"];
  std::string cauchy_string = (*field_name_map_)["Cauchy_Stress"];
  std::string Fp_string = (*field_name_map_)["Fp"];
  std::string L_string = (*field_name_map_)["Velocity_Gradient"];
  std::string residual_string = (*field_name_map_)["CP_Residual"];
  std::string source_string = (*field_name_map_)["Mechanical_Source"];
  std::string F_string = (*field_name_map_)["F"];
  std::string J_string = (*field_name_map_)["J"];

  // extract dependent MDFields
  PHX::MDField<ScalarT> def_grad = *dep_fields[F_string];
  PHX::MDField<ScalarT> J = *dep_fields[J_string];
  PHX::MDField<ScalarT> delta_time = *dep_fields["Delta Time"];

  // extract evaluated MDFields
  PHX::MDField<ScalarT> eqps = *eval_fields[eqps_string];
  PHX::MDField<ScalarT> xtal_rotation = *eval_fields[Re_string];
  PHX::MDField<ScalarT> stress = *eval_fields[cauchy_string];
  PHX::MDField<ScalarT> plastic_deformation = *eval_fields[Fp_string];
  PHX::MDField<ScalarT> velocity_gradient = *eval_fields[L_string];
  PHX::MDField<ScalarT> source = *eval_fields[source_string];
  PHX::MDField<ScalarT> cp_residual = *eval_fields[residual_string];

  PHX::MDField<ScalarT> time = *eval_fields["Time"];
  // extract slip on each slip system
  std::vector<Teuchos::RCP<PHX::MDField<ScalarT> > > slips;
  std::vector<Albany::MDArray *> previous_slips;
  for (int num_ss = 0; num_ss < num_slip_; ++num_ss) {
    std::string g = Albany::strint("gamma", num_ss + 1, '_');
    std::string gamma_string = (*field_name_map_)[g];
    slips.push_back(eval_fields[gamma_string]);
    previous_slips.push_back(
        &((*workset.stateArrayPtr)[gamma_string + "_old"]));
  }
  // extract hardening on each slip system
  std::vector<Teuchos::RCP<PHX::MDField<ScalarT> > > hards;
  std::vector<Albany::MDArray *> previous_hards;
  for (int num_ss = 0; num_ss < num_slip_; ++num_ss) {
    std::string t_h = Albany::strint("tau_hard", num_ss + 1, '_');
    std::string tau_hard_string = (*field_name_map_)[t_h];
    hards.push_back(eval_fields[tau_hard_string]);
    previous_hards.push_back(
        &((*workset.stateArrayPtr)[tau_hard_string + "_old"]));
  }

  // store shear on each slip system for output
  std::vector<Teuchos::RCP<PHX::MDField<ScalarT> > > shears;
  for (int num_ss = 0; num_ss < num_slip_; ++num_ss) {
    std::string t = Albany::strint("tau", num_ss + 1, '_');
    std::string tau_string = (*field_name_map_)[t];
    shears.push_back(eval_fields[tau_string]);
  }

  // get state variables
  Albany::MDArray previous_plastic_deformation =
      (*workset.stateArrayPtr)[Fp_string + "_old"];
  ScalarT tau, gamma, dgamma;
  ScalarT dt = delta_time(0);
  ScalarT tcurrent = time(0);

#ifdef PRINT_OUTPUT
  std::ofstream out("output.dat", std::fstream::app);
#endif

  Intrepid::Tensor<RealType> I = Intrepid::eye<RealType>(num_dims_);

  // -- Local variables for implicit integration routine --

  // Known quantities
  Intrepid::Tensor<ScalarT> F_np1(num_dims_);
  Intrepid::Tensor<ScalarT> Fp_n(num_dims_);
  std::vector<ScalarT> slip_n(num_slip_);
  std::vector<ScalarT> hardness_n(num_slip_);

  // Unknown quantities
  std::vector<ScalarT> slip_np1(num_slip_);
  std::vector<ScalarT> slip_np1_km1(num_slip_);
  std::vector<ScalarT> delta_delta_slip(num_slip_);
  Intrepid::Tensor<ScalarT> Lp_np1(num_dims_);
  Intrepid::Tensor<ScalarT> Fp_np1(num_dims_);
  Intrepid::Tensor<ScalarT> sigma_np1(num_dims_);
  Intrepid::Tensor<ScalarT> S_np1(num_dims_);
  std::vector<ScalarT> shear_np1(num_slip_);
  std::vector<ScalarT> slip_residual(num_slip_);
  std::vector<ScalarT> hardness_np1(num_slip_);
  ScalarT norm_slip_residual;

  // LocalNonlinearSolver
  LocalNonlinearSolver<EvalT, Traits> solver;
  std::vector<ScalarT> solver_matrix(num_slip_*num_slip_);

  // The following variables are dependent on the slip increment
  // Create AD objects for use in the implicit integration routine
  std::vector<Fad> slip_np1_ad(num_slip_);
  Intrepid::Tensor<Fad> Lp_np1_ad(num_dims_);
  Intrepid::Tensor<Fad> Fp_np1_ad(num_dims_);
  Intrepid::Tensor<Fad> sigma_np1_ad(num_dims_);
  Intrepid::Tensor<Fad> S_np1_ad(num_dims_);
  std::vector<Fad> shear_np1_ad(num_slip_);
  std::vector<Fad> slip_residual_ad(num_slip_);
  std::vector<Fad> hardness_np1_ad(num_slip_);
  Fad norm_slip_residual_ad;

  for (int cell(0); cell < workset.numCells; ++cell) {
    for (int pt(0); pt < num_pts_; ++pt) {

      // Copy data from Albany fields into local data structures
      for (int i(0); i < num_dims_; ++i) {
        for (int j(0); j < num_dims_; ++j) {
	  F_np1(i, j) = def_grad(cell, pt, i, j);
          Fp_n(i, j) = previous_plastic_deformation(cell, pt, i, j);
        }
      }
      for (int s(0); s < num_slip_; ++s) {
	slip_n[s] = (*(previous_slips[s]))(cell, pt);
	slip_np1[s] = slip_n[s]; // initialize state n+1 assuming zero slip increment
	slip_np1_km1[s] = slip_np1[s];
	hardness_n[s] = (*(previous_hards[s]))(cell, pt);
      }

      if(integration_scheme_ == EXPLICIT){

	// compute sigma_np1, S_np1, and shear_np1 using Fp_n
 	computeStress(F_np1, Fp_n, sigma_np1, S_np1, shear_np1);

	// compute hardness_np1 using slip_n
 	updateHardness(slip_n, hardness_n, hardness_np1);

	// compute slip_np1
	updateSlipViaExplicitIntegration(dt, slip_n, hardness_np1, S_np1, shear_np1, slip_np1);

 	// compute Lp_np1, and Fp_np1
 	applySlipIncrement(slip_n, slip_np1, Fp_n, Lp_np1, Fp_np1);

	// compute sigma_np1, S_np1, and shear_np1 using Fp_np1
 	computeStress(F_np1, Fp_np1, sigma_np1, S_np1, shear_np1);

	// compute slip_residual and norm_slip_residual
	computeResidual(dt, slip_n, slip_np1, hardness_np1, shear_np1, slip_residual, norm_slip_residual);
	RealType residual_val = Sacado::ScalarValue<ScalarT>::eval(norm_slip_residual);

#ifdef PRINT_DEBUG
	std::cout << "CP model explicit integration residual " << residual_val << std::endl;
#endif
      }
      else if(integration_scheme_ == IMPLICIT){

	// Evaluate quantities under the initial guess for the slip increment
	applySlipIncrement(slip_n, slip_np1, Fp_n, Lp_np1, Fp_np1);
	updateHardness(slip_np1, hardness_n, hardness_np1);
	computeStress(F_np1, Fp_np1, sigma_np1, S_np1, shear_np1);
	computeResidual(dt, slip_n, slip_np1, hardness_np1, shear_np1, slip_residual, norm_slip_residual);
	RealType residual_val = Sacado::ScalarValue<ScalarT>::eval(norm_slip_residual);

	// Determine convergence tolerances for the nonlinear solver
	RealType residual_relative_tolerance = implicit_nonlinear_solver_relative_tolerance_ * residual_val;
	RealType residual_absolute_tolerance = implicit_nonlinear_solver_absolute_tolerance_;
	int iteration = 0;
	bool converged = false;

#ifdef PRINT_DEBUG
	std::cout << "CP model initial residual " << residual_val << std::endl;
	std::cout << "CP model convergence tolerance (relative) " << residual_relative_tolerance << std::endl;
	std::cout << "CP model convergence tolerance (absolute) " << residual_absolute_tolerance << std::endl;
#endif

	while(!converged){

#ifdef LINE_SEARCH
	  // Line search
	  for(int s=0 ; s<num_slip_ ; s++){
	    delta_delta_slip[s] = slip_np1[s] - slip_np1_km1[s];
	  }
	  RealType alpha;
	  lineSearch(dt, Fp_n, F_np1, slip_n, slip_np1_km1, delta_delta_slip, hardness_n, alpha);
	  for(int s=0 ; s<num_slip_ ; s++){
	    slip_np1[s] = slip_np1_km1[s] + alpha*delta_delta_slip[s];
	    slip_np1_km1[s] = slip_np1[s];
	  }
#endif

	  // Seed the AD objects
	  for (int s(0); s < num_slip_; ++s) {
	    ScalarT slip_np1_val = Sacado::ScalarValue<ScalarT>::eval(slip_np1[s]);
	    slip_np1_ad[s] = Fad(num_slip_, s, slip_np1_val);
	  }

	  // Compute Lp_np1, and Fp_np1
	  applySlipIncrement(slip_n, slip_np1_ad, Fp_n, Lp_np1_ad, Fp_np1_ad);

	  // Compute hardness_np1
	  updateHardness(slip_np1_ad, hardness_n, hardness_np1_ad);

	  // Compute sigma_np1, S_np1, and shear_np1
	  computeStress(F_np1, Fp_np1_ad, sigma_np1_ad, S_np1_ad, shear_np1_ad);

	  // Compute slip_residual and norm_slip_residual
	  computeResidual(dt, slip_n, slip_np1_ad, hardness_np1_ad, shear_np1_ad, slip_residual_ad, norm_slip_residual_ad);
	  residual_val = Sacado::ScalarValue<ScalarT>::eval(norm_slip_residual_ad.val());

	  // Copy values of slip_np1 and slip_residual from AD objects to ScalarT objects
	  // Construct the matrix for the solver using the derivative information stored in the AD objects
	  for(int i=0 ; i<num_slip_ ; i++){
	    slip_np1[i] = slip_np1_ad[i].val();
	    slip_residual[i] = slip_residual_ad[i].val();
	    for(int j=0 ; j<num_slip_ ; j++){
	      solver_matrix[num_slip_*i + j] = slip_residual_ad[i].dx(j);
	    }
	  }

#ifdef PRINT_DEBUG
	  std::cout << "\nAD MATRIX" << std::endl;
	  for(int i=0 ; i<num_slip_ ; i++){
	    for(int j=0 ; j<num_slip_ ; j++){
	      std::cout << std::setprecision(4) << Sacado::ScalarValue<ScalarT>::eval(solver_matrix[num_slip_*i+j]) << "  ";
	    }
	    std::cout << std::endl;
	  }
	  std::cout << std::endl;

	  std::vector<ScalarT> finite_difference_matrix(num_slip_*num_slip_);
	  constructMatrixFiniteDifference(dt, Fp_n, F_np1, slip_n, slip_np1, hardness_n, finite_difference_matrix);
	  std::cout << "\nFD MATRIX" << std::endl;
	  for(int i=0 ; i<num_slip_ ; i++){
	    for(int j=0 ; j<num_slip_ ; j++){
	      std::cout << std::setprecision(4) << Sacado::ScalarValue<ScalarT>::eval(finite_difference_matrix[num_slip_*i+j]) << "  ";
	    }
	    std::cout << std::endl;
	  }
	  std::cout << std::endl;

	  std::cout << "\nDIFFERENCE BETWEEN AD AND FD MATRICES" << std::endl;
	  for(int i=0 ; i<num_slip_ ; i++){
	    for(int j=0 ; j<num_slip_ ; j++){
	      std::cout << std::setprecision(4) << std::fabs(Sacado::ScalarValue<ScalarT>::eval(finite_difference_matrix[num_slip_*i+j]) - Sacado::ScalarValue<ScalarT>::eval(solver_matrix[num_slip_*i+j])) << "  ";
	    }
	    std::cout << std::endl;
	  }
	  std::cout << std::endl;
#endif

	  // This call solves the system for the slip increment and applies it to slip_np1
	  solver.solve(solver_matrix, slip_np1, slip_residual);

	  iteration += 1;

	  if(residual_val < residual_relative_tolerance || residual_val < residual_absolute_tolerance ){
	    converged = true;
	  }
	  if(iteration >= implicit_nonlinear_solver_max_iterations_){
	    // DJL need a graceful way to exit the constitutive model with an error if the maximum
	    // number of iterations has been reached but the system has not converged
	    std::cout << "\n****WARNING: CrystalPlasticity model failed to converge in " << iteration << " iterations!  Final residual value = " << residual_val << ".\n" << std::endl;
	    converged = true;
	  }

#ifdef PRINT_DEBUG
	  std::cout << "CP model residual iteration " << iteration << " = " << residual_val << std::endl;
#endif
	}

	// Glue derivative information together for the case where ScalarT is an AD type (e.g., Jacobian evaluation)
	solver.computeFadInfo(solver_matrix, slip_np1, slip_residual);

	// Load data back into ScalarT objects
	for (int i(0); i < num_dims_; ++i) {
	  for (int j(0); j < num_dims_; ++j) {
	    Lp_np1(i,j) = Lp_np1_ad(i,j).val();
	    Fp_np1(i,j) = Fp_np1_ad(i,j).val();
	  }
	}
	for (int s(0) ; s < num_slip_; ++s){
	  hardness_np1[s] = hardness_np1_ad[s].val();
	}

	// Re-evaluate everything (and propagate derivative information) with the final values of slip_np1
	applySlipIncrement(slip_n, slip_np1, Fp_n, Lp_np1, Fp_np1);
	updateHardness(slip_np1, hardness_n, hardness_np1);
	computeStress(F_np1, Fp_np1, sigma_np1, S_np1, shear_np1);
	computeResidual(dt, slip_n, slip_np1, hardness_np1, shear_np1, slip_residual, norm_slip_residual);
	residual_val = Sacado::ScalarValue<ScalarT>::eval(norm_slip_residual);
#ifdef PRINT_DEBUG
	std::cout << "CP model final residual " << residual_val << std::endl;
#endif
      } // integration_scheme == IMPLICIT
      
      // The EQPS can be computed (or can it?) from the Cauchy Green strain of Fp.
      Intrepid::Tensor<ScalarT> Re_np1(num_dims_);
      Intrepid::Tensor<ScalarT> Fe(num_dims_);
      Intrepid::Tensor<ScalarT> CGS_Fp(num_dims_);
      CGS_Fp = 0.5*(((Intrepid::transpose(Fp_np1))*Fp_np1) - I);
      ScalarT equivalent_plastic_strain = (2.0/3.0)*Intrepid::dotdot(CGS_Fp, CGS_Fp);
      if(equivalent_plastic_strain > 0.0){
	equivalent_plastic_strain = std::sqrt(equivalent_plastic_strain);
      }
      eqps(cell, pt) = equivalent_plastic_strain;
      // The xtal rotation from the polar decomp of Fe.
      // Saint Venant–Kirchhoff model
#ifdef DECOUPLE
      Fe = F_np1;
#else
      Fe = F_np1 * (Intrepid::inverse(Fp_np1));
#endif
      Re_np1 = Intrepid::polar_rotation(Fe);

      // Copy data from local data structures back into Albany fields
      source(cell, pt) = 0.0;
      cp_residual(cell, pt) = norm_slip_residual;
      for (int i(0); i < num_dims_; ++i) {
        for (int j(0); j < num_dims_; ++j) {
          xtal_rotation(cell, pt, i, j) = Re_np1(i, j);
          plastic_deformation(cell, pt, i, j) = Fp_np1(i, j);
          stress(cell, pt, i, j) = sigma_np1(i, j);
          velocity_gradient(cell, pt, i, j) = Lp_np1(i, j);
        }
      }
      for (int s(0); s < num_slip_; ++s) {
	(*(slips[s]))(cell, pt) = slip_np1[s];
	(*(hards[s]))(cell, pt) = hardness_np1[s];
	(*(shears[s]))(cell, pt) = shear_np1[s];
      }
#ifdef PRINT_OUTPUT
      if (cell == 0 && pt == 0) {
        out << "\n" << "time: ";
        out << std::setprecision(12) << Sacado::ScalarValue<ScalarT>::eval(tcurrent) << " ";
        out << "\n";
        out << "\n" << "F: ";
        for (int i(0); i < num_dims_; ++i) {
          for (int j(0); j < num_dims_; ++j) {
            out << std::setprecision(12) << Sacado::ScalarValue<ScalarT>::eval(F_np1(i,j)) << " ";
          }
        }
        out << "\n" << "Fp: ";
        for (int i(0); i < num_dims_; ++i) {
          for (int j(0); j < num_dims_; ++j) {
            out << std::setprecision(12) << Sacado::ScalarValue<ScalarT>::eval(Fp_np1(i,j)) << " ";
          }
        }
        out << "\n" << "Sigma: ";
        for (int i(0); i < num_dims_; ++i) {
          for (int j(0); j < num_dims_; ++j) {
            out << std::setprecision(12) << Sacado::ScalarValue<ScalarT>::eval(sigma_np1(i,j)) << " ";
          }
        }
        out << "\n" << "Lp: ";
        for (int i(0); i < num_dims_; ++i) {
          for (int j(0); j < num_dims_; ++j) {
            out << std::setprecision(12) << Sacado::ScalarValue<ScalarT>::eval(L_np1(i,j)) << " ";
          }
        }
        out << "\n";
      }
#endif

    }
  }
#ifdef PRINT_DEBUG
  std::cout << "<<< done in cp compute state\n" << std::flush;
#endif
}

//------------------------------------------------------------------------------

template<typename EvalT, typename Traits>
template<typename ArgT>
void CrystalPlasticityModel<EvalT, Traits>::
updateSlipViaExplicitIntegration(ScalarT                         dt,
				 std::vector<ScalarT> const &    slip_n,
				 std::vector<ScalarT> const &    hardness,
				 Intrepid::Tensor<ArgT> const &  S,
				 std::vector<ArgT> const &       shear,
				 std::vector<ArgT> &             slip_np1) const
{
  ScalarT g0, tauC, m, temp;

  for (int s(0); s < num_slip_; ++s) {

    tauC = slip_systems_[s].tau_critical_;
    m = slip_systems_[s].gamma_exp_;
    g0 = slip_systems_[s].gamma_dot_0_;

    int sign = shear[s] < 0 ? -1 : 1;
    temp = std::fabs(shear[s] / (tauC + hardness[s]));
    // JWF - m is positive, we don't need std::fabs(std::pow(temp,m))
    slip_np1[s] = slip_n[s] + dt * g0 * std::pow(temp, m) * sign;
  }
}

//------------------------------------------------------------------------------

template<typename EvalT, typename Traits>
template<typename ArgT>
void CrystalPlasticityModel<EvalT, Traits>::
applySlipIncrement(std::vector<ScalarT> const &       slip_n,
		   std::vector<ArgT> const &          slip_np1,
		   Intrepid::Tensor<ScalarT> const &  Fp_n,
		   Intrepid::Tensor<ArgT> &           Lp_np1,
		   Intrepid::Tensor<ArgT> &           Fp_np1) const
{
  ScalarT temp;
  Intrepid::Tensor<RealType> P(num_dims_);
  Intrepid::Tensor<ArgT> expL(num_dims_);

  Lp_np1.fill(Intrepid::ZEROS);
  for (int s(0); s < num_slip_; ++s) {

    // material parameters
    P = slip_systems_[s].projector_;

    // calculate plastic velocity gradient
    Lp_np1 += (slip_np1[s] - slip_n[s]) * P;
  }

  confirmTensorSanity(Lp_np1, "Lp_np1 in applySlipIncrement().");

  // update plastic deformation gradient
  expL = Intrepid::exp(Lp_np1);
  Fp_np1 = expL * Fp_n;

  confirmTensorSanity(Fp_np1, "Fp_np1 in applySlipIncrement()");
}

//------------------------------------------------------------------------------

template<typename EvalT, typename Traits>
template<typename ArgT>
void CrystalPlasticityModel<EvalT, Traits>::
updateHardness(std::vector<ArgT> const &     slip_np1,
	       std::vector<ScalarT> const &  hardness_n,
	       std::vector<ArgT> &           hardness_np1) const
{
  ScalarT H, p;
  ArgT temp;

  for (int s(0); s < num_slip_; ++s) {

    // material parameters
    H = slip_systems_[s].H_;
    p = slip_systems_[s].p_;

    hardness_np1[s] = hardness_n[s];

    // calculate additional hardening
    // function form is power law, H \gamma^{p}
    // if H is not specified, H = 0.0, if p is not specified, p = 1.0
    temp = H * std::pow(std::fabs(slip_np1[s]),p);
    if (temp > hardness_np1[s]) {
      hardness_np1[s] = temp;
    }
  }
}

//------------------------------------------------------------------------------

template<typename EvalT, typename Traits>
template<typename ArgT>
void CrystalPlasticityModel<EvalT, Traits>::
computeResidual(ScalarT                       dt,
		std::vector<ScalarT> const &  slip_n,
		std::vector<ArgT> const &     slip_np1,
		std::vector<ArgT> const &     hardness_np1,
		std::vector<ArgT> const &     shear_np1,
		std::vector<ArgT> &           slip_residual,
		ArgT &                        norm_slip_residual) const
{
  ScalarT g0, tauC, m;
  ArgT dgamma_value1, dgamma_value2, temp;
  Intrepid::Tensor<RealType> P(num_dims_);
  int sign;

  for (int s(0); s < num_slip_; ++s) {

    // Material properties
    P = slip_systems_[s].projector_;
    tauC = slip_systems_[s].tau_critical_;
    m = slip_systems_[s].gamma_exp_;
    g0 = slip_systems_[s].gamma_dot_0_;

    // The current computed value of dgamma
    dgamma_value1 = slip_np1[s] - slip_n[s];

    // Compute slip increment using Fe_np1
    sign = shear_np1[s] < 0 ? -1 : 1;
    temp = std::fabs(shear_np1[s] / (tauC + hardness_np1[s]));
    // Establishing filter for active slip systems (help from JTO)
    const double filter = std::numeric_limits<RealType>::epsilon()*100.0;
    if (temp < filter) {
      dgamma_value2 = dt * g0 * 0.0 * sign;
    }
    else {
      // JWF - m is positive, we don't need std::fabs(std::pow(temp,m))
      dgamma_value2 = dt * g0 * std::pow(temp, m) * sign;
    }
    // The difference between the slip increment calculations is the residual for this slip system
    slip_residual[s] = dgamma_value2 - dgamma_value1;
  }

  // Take norm of residual - protect sqrt (Saccado)
  norm_slip_residual = 0.0;
  for(unsigned int i=0 ; i<slip_residual.size() ; ++i){
    norm_slip_residual += slip_residual[i]*slip_residual[i];
  }
  if (norm_slip_residual > 0.0) {
    norm_slip_residual = std::sqrt(norm_slip_residual);
  }
}

//------------------------------------------------------------------------------

template<typename EvalT, typename Traits>
template<typename ArgT>
void CrystalPlasticityModel<EvalT, Traits>::
constructMatrixFiniteDifference(ScalarT                            dt,
				Intrepid::Tensor<ScalarT> const &  Fp_n,
				Intrepid::Tensor<ScalarT> const &  F_np1,
				std::vector<ScalarT> const &       slip_n,
				std::vector<ArgT> const &          slip_np1,
				std::vector<ScalarT> const &       hardness_n,
				std::vector<ArgT> &                matrix) const
{
  std::vector<ArgT> slip_np1_temp(num_slip_);
  std::vector<ArgT> hardness_np1_temp(num_slip_);
  Intrepid::Tensor<ArgT> Lp_np1_temp(num_dims_);
  Intrepid::Tensor<ArgT> Fp_np1_temp(num_dims_);
  Intrepid::Tensor<ArgT> sigma_np1_temp(num_dims_), S_np1_temp(num_dims_);
  std::vector<ArgT> shear_np1_temp(num_slip_);
  std::vector<ArgT> slip_residual_temp_plus(num_slip_);
  std::vector<ArgT> slip_residual_temp_minus(num_slip_);
  ArgT norm_slip_residual_temp;

  // Compute the entries in the matrix via finite difference
  ScalarT epsilon = 1.0e-6;
  for(int column=0 ; column<num_slip_ ; column++){

    // Forward probe
    for (int s(0); s < num_slip_; ++s) {
      slip_np1_temp[s] = slip_np1[s];
    }
    slip_np1_temp[column] += epsilon;
    applySlipIncrement(slip_n, slip_np1_temp, Fp_n, Lp_np1_temp, Fp_np1_temp);
    updateHardness(slip_np1_temp, hardness_n, hardness_np1_temp);
    computeStress(F_np1, Fp_np1_temp, sigma_np1_temp, S_np1_temp, shear_np1_temp);
    computeResidual(dt, slip_n, slip_np1_temp, hardness_np1_temp, shear_np1_temp, slip_residual_temp_plus, norm_slip_residual_temp);

    // Backward probe
    for (int s(0); s < num_slip_; ++s) {
      slip_np1_temp[s] = slip_np1[s];
    }
    slip_np1_temp[column] -= epsilon;
    applySlipIncrement(slip_n, slip_np1_temp, Fp_n, Lp_np1_temp, Fp_np1_temp);
    updateHardness(slip_np1_temp, hardness_n, hardness_np1_temp);
    computeStress(F_np1, Fp_np1_temp, sigma_np1_temp, S_np1_temp, shear_np1_temp);
    computeResidual(dt, slip_n, slip_np1_temp, hardness_np1_temp, shear_np1_temp, slip_residual_temp_minus, norm_slip_residual_temp);

    // Central difference approximation of the derivative
    for(int row=0 ; row<num_slip_ ; row++){
      matrix[row*num_slip_ + column] = (slip_residual_temp_plus[row] - slip_residual_temp_minus[row])/(2.0*epsilon);
    }
  }
}

//------------------------------------------------------------------------------

template<typename EvalT, typename Traits>
template<typename ArgT>
void CrystalPlasticityModel<EvalT, Traits>::
lineSearch(ScalarT                            dt,
	   Intrepid::Tensor<ScalarT> const &  Fp_n,
	   Intrepid::Tensor<ScalarT> const &  F_np1,
	   std::vector<ScalarT> const &       slip_n,
	   std::vector<ArgT> const &          slip_np1_km1,
	   std::vector<ArgT> const &          delta_delta_slip,
	   std::vector<ScalarT> const &       hardness_n,
	   RealType &                         alpha) const
{
  std::vector<ArgT> slip_np1_temp(num_slip_);
  std::vector<ArgT> hardness_np1_temp(num_slip_);
  Intrepid::Tensor<ArgT> Lp_np1_temp(num_dims_);
  Intrepid::Tensor<ArgT> Fp_np1_temp(num_dims_);
  Intrepid::Tensor<ArgT> sigma_np1_temp(num_dims_), S_np1_temp(num_dims_);
  std::vector<ArgT> shear_np1_temp(num_slip_);
  std::vector<ArgT> slip_residual_unperturbed(num_slip_);
  std::vector<ArgT> slip_residual_temp(num_slip_);
  ArgT slip_increment, norm_slip_residual_temp;

  RealType residual_val;
  RealType bestAlpha = 1.0;
  RealType bestResidual = std::numeric_limits<RealType>::max();

  // DJL We'll want to replace this brute-force approach with something clever.
  std::vector<RealType> candidateAlpha;
  candidateAlpha.push_back(0.00001);
  candidateAlpha.push_back(0.0001);
  candidateAlpha.push_back(0.001);
  candidateAlpha.push_back(0.01);
  candidateAlpha.push_back(0.1);
  candidateAlpha.push_back(0.2);
  candidateAlpha.push_back(0.3);
  candidateAlpha.push_back(0.4);
  candidateAlpha.push_back(0.5);
  candidateAlpha.push_back(0.6);
  candidateAlpha.push_back(0.7);
  candidateAlpha.push_back(0.8);
  candidateAlpha.push_back(0.9);
  candidateAlpha.push_back(1.0);
  candidateAlpha.push_back(1.1);
  candidateAlpha.push_back(1.2);
  candidateAlpha.push_back(1.3);
  candidateAlpha.push_back(1.4);
  candidateAlpha.push_back(1.5);
  candidateAlpha.push_back(1.6);
  candidateAlpha.push_back(1.7);
  candidateAlpha.push_back(1.8);
  candidateAlpha.push_back(1.9);
  candidateAlpha.push_back(2.0);
  candidateAlpha.push_back(3.0);
  candidateAlpha.push_back(4.0);
  candidateAlpha.push_back(5.0);
  candidateAlpha.push_back(10.0);

  for(unsigned int iAlpha = 0 ; iAlpha < candidateAlpha.size() ; iAlpha++){

    for (int s(0); s < num_slip_; ++s) {
      slip_np1_temp[s] = slip_np1_km1[s] + candidateAlpha[iAlpha] * delta_delta_slip[s];
    }

    try{
      applySlipIncrement(slip_n, slip_np1_temp, Fp_n, Lp_np1_temp, Fp_np1_temp);
      updateHardness(slip_np1_temp, hardness_n, hardness_np1_temp);
      computeStress(F_np1, Fp_np1_temp, sigma_np1_temp, S_np1_temp, shear_np1_temp);
      computeResidual(dt, slip_n, slip_np1_temp, hardness_np1_temp, shear_np1_temp, slip_residual_unperturbed, norm_slip_residual_temp);
      residual_val = Sacado::ScalarValue<ScalarT>::eval(norm_slip_residual_temp);
      std::cout << "DJL DEBUGGING alpha " << candidateAlpha[iAlpha] << ", residual " << residual_val << std::endl;
    } catch (...) {
      std::cout << "DJL DEBUGGING caught exception in line search!" << std::endl;
      residual_val = std::numeric_limits<RealType>::max();
    }

    if(residual_val < bestResidual){
      bestAlpha = candidateAlpha[iAlpha];
      bestResidual = residual_val;
    }
  }

  std::cout << "DJL DEBUGGING Best alpha " << bestAlpha << ", best residual " << bestResidual << "\n" << std::endl;  

  alpha = bestAlpha;
}

//------------------------------------------------------------------------------

template<typename EvalT, typename Traits>
template<typename ArgT>
void CrystalPlasticityModel<EvalT, Traits>::
computeStress(Intrepid::Tensor<ScalarT> const &  F,
	      Intrepid::Tensor<ArgT> const &     Fp,
	      Intrepid::Tensor<ArgT> &           sigma,
	      Intrepid::Tensor<ArgT> &           S,
	      std::vector<ArgT> &                shear) const
{
  Intrepid::Tensor<ArgT> Fpinv, Fe, E;
  Intrepid::Tensor<RealType> I = Intrepid::eye<RealType>(num_dims_);

  // Saint Venant–Kirchhoff model
  Fpinv = Intrepid::inverse(Fp);
#ifdef DECOUPLE
  std::cout << "ELASTIC STRESS ONLY\n";
  Fe = F;
#else
  Fe = F * Fpinv;
#endif
  E = 0.5 * (Intrepid::transpose(Fe) * Fe - I);
  S = Intrepid::dotdot(C_, E);
  sigma = (1.0 / Intrepid::det(F)) * F * S * Intrepid::transpose(F);
  confirmTensorSanity(sigma, "Cauchy stress in CrystalPlasticityModel::computeStress()");

  // Compute resolved shear stresses
  for (int s(0); s < num_slip_; ++s) {
    shear[s] = Intrepid::dotdot(slip_systems_[s].projector_, S);
  }
}

//------------------------------------------------------------------------------

template<typename EvalT, typename Traits>
template<typename ArgT>
void CrystalPlasticityModel<EvalT, Traits>::
confirmTensorSanity(Intrepid::Tensor<ArgT> const & input,
		    std::string const & message) const
{
  int dim = input.get_dimension();
  for (int i = 0; i < dim; i++) {
    for (int j = 0; j < dim; j++) {
      if (!boost::math::isfinite(Sacado::ScalarValue<ArgT>::eval(input(i, j)))) {
        std::string msg =
            "**** Invalid data detected in CrystalPlasticityModel::confirmTensorSanity(): "
                + message;
        TEUCHOS_TEST_FOR_EXCEPTION(
	    !boost::math::isfinite(Sacado::ScalarValue<ArgT>::eval(input(i, j))),
            std::logic_error,
            msg);
      }
    }
  }
}

} // namespace LCM
