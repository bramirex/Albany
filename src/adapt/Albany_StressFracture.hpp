//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

/**
 * Albany_StressFracture.hpp
 *
 * This class contains the abstract interface for determining if fracture has occurred between
 * two elements, based on the average stress in the elements sharing the edge between them. 
 * 
 */

#ifndef ALBANY_STRESSFRACTURE_H_
#define ALBANY_STRESSFRACTURE_H_

#include "Fracture.h"
#include "Albany_STKDiscretization.hpp"

namespace LCM{

class StressFracture: public AbstractFractureCriterion {

public:

	/**
	 * \brief Default constructor for the criterion object
	 */
	StressFracture(int numDim_, EntityRank& elementRank, const std::vector<std::vector<double> >& stresses,
    double crit_stress, Albany::STKDiscretization& stk);


	/**
	 * \brief Stress fracture criterion function.
	 *
	 * \param[in] entity
	 * \param[in] probability
	 * \return is criterion met
	 *
	 * Given an entity and probability, will determine if fracture criterion
	 * is met. Will return true if fracture criterion is met, else false.
	 * Fracture only defined on surface of elements. Thus, input entity
	 * must be of rank dimension-1, else error. For 2D, entity rank must = 1.
	 * For 3D, entity rank must = 2.
	 */
  virtual
	bool
	fracture_criterion(
			Entity& entity,
			float p);

private:

   StressFracture();
   StressFracture(const StressFracture&);
   StressFracture&operator=(const StressFracture&);

  const std::vector<std::vector<double> >& avg_stresses;
  Albany::STKDiscretization& stk;

  double crit_stress;

}; // class StressFracture


} // namespace LCM

#endif /* ALBANY_STRESSFRACTURE_H_ */