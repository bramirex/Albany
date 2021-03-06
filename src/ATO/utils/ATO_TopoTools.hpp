//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef TopoTools_HPP
#define TopoTools_HPP

#include "Teuchos_ParameterList.hpp"

namespace ATO {

class Simp;
class Ramp;
class H1;
class H2;
/** \brief Topology support utilities

    This class provides basic support for various penalization approaches.

*/
class Topology 
{
  enum PenaltyType {SIMP, RAMP, HONE, HTWO};

public:
  Topology(const Teuchos::ParameterList& topoParams);
  virtual ~Topology(){};

  template<typename T> T Penalize(int fIndex, T rho);
  template<typename T> T dPenalize(int fIndex, T rho);

  const std::string& getName(){return name;}
  const std::string& getOutputNames(){return name;}
  const Teuchos::Array<std::string>& getFixedBlocks(){return fixedBlocks;}
  double getInitialValue(){return initValue;}
  double getMaterialValue(){return materialValue;}
  double getVoidValue(){return voidValue;}
  const Teuchos::Array<double> getBounds(){return bounds;}
  std::string getEntityType(){return entityType;}
  int TopologyOutputFilter(){return topologyOutputFilter;}
  int SpatialFilterIndex(){return spatialFilterIndex;}
private:
  // this should be a vector of strings at some point since, in the
  // general case, the topology may be defined by multiple fields.
  std::string name;
  std::string entityType;

  // JR: There's probably a better way to do this.  
  typedef struct PenaltyFunction {
    PenaltyFunction(){}
    PenaltyFunction(const Teuchos::ParameterList& fParams);
    PenaltyType pType;
    Teuchos::RCP<Simp> simp; 
    Teuchos::RCP<Ramp> ramp; 
    Teuchos::RCP<H1> h1;
    Teuchos::RCP<H2> h2;
  } PenaltyFunction;
  Teuchos::Array<PenaltyFunction> penaltyFunctions;

  std::vector<std::string> outputNames;
  double initValue;
  double materialValue;
  Teuchos::Array<double> bounds;
  double voidValue;
  Teuchos::Array<std::string> fixedBlocks;

  int topologyOutputFilter;
  int spatialFilterIndex;
};

class Simp {
 public:
  Simp(const Teuchos::ParameterList& topoParams);
  template<typename T> T Penalize(T rho);
  template<typename T> T dPenalize(T rho);
  double penaltyParam;
  double minValue;
};

class Ramp {
 public:
  Ramp(const Teuchos::ParameterList& topoParams);
  template<typename T> T Penalize(T rho);
  template<typename T> T dPenalize(T rho);
  double penaltyParam;
  double minValue;
};
class H1 {
 public:
  H1(const Teuchos::ParameterList& topoParams);
  template<typename T> T Penalize(T rho);
  template<typename T> T dPenalize(T rho);
  double regLength;
  double minValue;
};
class H2 {
 public:
  H2(const Teuchos::ParameterList& topoParams);
  template<typename T> T Penalize(T rho);
  template<typename T> T dPenalize(T rho);
  double regLength;
  double minValue;
};

}

#include "ATO_TopoTools_Def.hpp"
#endif
