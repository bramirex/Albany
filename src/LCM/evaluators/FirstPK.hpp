//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#if !defined(LCM_FirstPK_hpp)
#define LCM_FirstPK_hpp

#include <Phalanx_config.hpp>
#include <Phalanx_Evaluator_WithBaseImpl.hpp>
#include <Phalanx_Evaluator_Derived.hpp>
#include <Phalanx_MDField.hpp>
#include <Sacado_ParameterAccessor.hpp>
#include "Albany_Layouts.hpp"

namespace LCM
{
///
/// \brief First Piola-Kirchhoff Stress
///
/// This evaluator computes the first PK stress from the deformation gradient
/// and Cauchy stress, and optionally volume averages the pressure
///
template<typename EvalT, typename Traits>
class FirstPK:
    public PHX::EvaluatorWithBaseImpl<Traits>,
    public PHX::EvaluatorDerived<EvalT, Traits>
{

public:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  ///
  /// Constructor
  ///
  FirstPK(Teuchos::ParameterList& p,
      const Teuchos::RCP<Albany::Layouts>& dl);

  ///
  /// Phalanx method to allocate space
  ///
  void
  postRegistrationSetup(typename Traits::SetupData d,
      PHX::FieldManager<Traits>& vm);

  ///
  /// Implementation of physics
  ///
  void
  evaluateFields(typename Traits::EvalData d);

private:

  ///
  /// Input: Cauchy Stress
  ///
  PHX::MDField<ScalarT, Cell, QuadPoint, Dim, Dim> stress_;

  ///
  /// Input: Deformation Gradient
  ///
  PHX::MDField<ScalarT, Cell, QuadPoint, Dim, Dim> def_grad_;

  ///
  /// Output: First PK stress
  ///
  PHX::MDField<ScalarT, Cell, QuadPoint, Dim, Dim> first_pk_stress_;

  ///
  /// Optional
  /// Input: Pore Pressure
  ///
  PHX::MDField<ScalarT, Cell, QuadPoint> pore_pressure_;

  ///
  /// Optional
  /// Input: Biot Coefficient
  ///
  PHX::MDField<ScalarT, Cell, QuadPoint> biot_coeff_;

  ///
  /// Optional
  /// Input: Stabilized Pressure
  ///
  PHX::MDField<ScalarT, Cell, QuadPoint> stab_pressure_;

  ///
  /// Number of integration points
  ///
  int num_pts_;

  ///
  /// Number of spatial dimensions
  ///
  int num_dims_;

  ///
  /// Pore Pressure flag
  ///
  bool have_pore_pressure_;

  ///
  /// Stabilized Pressure flag
  ///
  bool have_stab_pressure_;

  ///
  /// Small Strain flag
  ///
  bool small_strain_;

 //Kokkos
#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
   public:

   struct have_stab_pressure_Tag{};
   struct have_pore_pressure_Tag{};
   struct small_strain_Tag{};
   struct no_small_strain_Tag{};

   typedef Kokkos::View<int***, PHX::Device>::execution_space ExecutionSpace;

   typedef Kokkos::RangePolicy<ExecutionSpace,have_stab_pressure_Tag> have_stab_pressure_Policy;
   typedef Kokkos::RangePolicy<ExecutionSpace,have_pore_pressure_Tag> have_pore_pressure_Policy;
   typedef Kokkos::RangePolicy<ExecutionSpace,small_strain_Tag> small_strain_Policy;
   typedef Kokkos::RangePolicy<ExecutionSpace,no_small_strain_Tag> no_small_strain_Policy;

   KOKKOS_INLINE_FUNCTION
   void operator() (const have_stab_pressure_Tag& tag, const int& i) const;
   KOKKOS_INLINE_FUNCTION
   void operator() (const have_pore_pressure_Tag& tag, const int& i) const;
   KOKKOS_INLINE_FUNCTION
   void operator() (const small_strain_Tag& tag, const int& i) const;
   KOKKOS_INLINE_FUNCTION
   void operator() (const no_small_strain_Tag& tag, const int& i) const;

   typedef PHX::KokkosViewFactory<ScalarT,PHX::Device> ViewFactory;
   std::vector<PHX::index_size_type> ddims_;
   int derivative_dim;
   PHX::MDField<ScalarT,Dim,Dim> I;
   PHX::MDField<ScalarT,Cell,Dim,Dim> sig;
   PHX::MDField<ScalarT,Cell,Dim,Dim> F;
   PHX::MDField<ScalarT,Cell,Dim,Dim> P;

   template <class ArrayT>
   KOKKOS_INLINE_FUNCTION
   const ScalarT trace(const ArrayT &A, const int cell) const;
 
   //template <class ArrayT>
  // KOKKOS_INLINE_FUNCTION
  // void piola(const ArrayT &P, const ArrayT &F, const ArrayT &sigma) const;
 
#endif
};
}

#endif
