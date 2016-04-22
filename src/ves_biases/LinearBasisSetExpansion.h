/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2015-2016 The ves-code team
   (see the PEOPLE-VES file at the root of the distribution for a list of names)

   See http://www.ves-code.org for more information.

   This file is part of ves-code, version 1.

   ves-code is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   ves-code is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with ves-code.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#ifndef __PLUMED_ves_biases_LinearBasisSetExpansion_h
#define __PLUMED_ves_biases_LinearBasisSetExpansion_h

#include <vector>
#include <string>

namespace PLMD {

class Action;
class Keywords;
class Value;
class Communicator;
class Grid;
class CoeffsVector;
class BasisFunctions;
class TargetDistribution;

namespace bias{
  class VesBias;


class LinearBasisSetExpansion{
private:
  std::string label_;
  //
  Action* action_pntr_;
  bias::VesBias* vesbias_pntr_;
  Communicator& mycomm_;
  bool serial_;
  //
  double beta_;
  double kbt_;
  //
  std::vector<Value*> args_pntrs_;
  unsigned int nargs_;
  //
  std::vector<BasisFunctions*> basisf_pntrs_;
  std::vector<unsigned int> nbasisf_;
  //
  CoeffsVector* bias_coeffs_pntr_;
  size_t ncoeffs_;
  CoeffsVector* coeffderivs_aver_ps_pntr_;
  CoeffsVector* fes_wt_coeffs_pntr_;
  //
  double bias_fes_scalingf_;
  bool log_ps_fes_contribution_;
  //
  double welltemp_biasf_;
  double inv_welltemp_biasf_;
  double beta_prime_;
  //
  bool bias_cutoff_active_;
  //
  std::vector<unsigned int> grid_bins_;
  //
  std::string targetdist_grid_label_;
  //
  long int step_of_last_biasgrid_update;
  long int step_of_last_fesgrid_update;
  //
  Grid* bias_grid_pntr_;
  Grid* bias_withoutcutoff_grid_pntr_;
  Grid* fes_grid_pntr_;
  Grid* log_ps_grid_pntr_;
  Grid* dynamic_ps_grid_pntr_;
public:
  static void registerKeywords( Keywords& keys );
  // Constructor
  explicit LinearBasisSetExpansion(
    const std::string&,
    const double,
    Communicator&,
    std::vector<Value*>,
    std::vector<BasisFunctions*>,
    CoeffsVector* bias_coeffs_pntr_in=NULL);
  //
private:
  // copy constructor is disabled (private and unimplemented)
  explicit LinearBasisSetExpansion(const LinearBasisSetExpansion&);
public:
  ~LinearBasisSetExpansion();
  //
  std::vector<Value*> getPntrsToArguments() const {return args_pntrs_;}
  std::vector<BasisFunctions*> getPntrsToBasisFunctions() const {return basisf_pntrs_;}
  CoeffsVector* getPntrToBiasCoeffs() const {return bias_coeffs_pntr_;}
  Grid* getPntrToBiasGrid() const {return bias_grid_pntr_;};
  //
  unsigned int getNumberOfArguments() const {return nargs_;};
  std::vector<unsigned int> getNumberOfBasisFunctions() const {return nbasisf_;};
  size_t getNumberOfCoeffs() const {return ncoeffs_;};
  //
  CoeffsVector& BiasCoeffs() const {return *bias_coeffs_pntr_;};
  CoeffsVector& FesWTCoeffs() const {return *fes_wt_coeffs_pntr_;};
  CoeffsVector& CoeffDerivsAverTargetDist() const {return *coeffderivs_aver_ps_pntr_;};
  //
  void setSerial() {serial_=true;}
  void setParallel() {serial_=false;}
  //
  void linkVesBias(bias::VesBias*);
  void linkAction(Action*);
  // calculate bias and derivatives
  static double getBiasAndForces(const std::vector<double>&, std::vector<double>&, std::vector<double>&, std::vector<BasisFunctions*>&, CoeffsVector*, Communicator* comm_in=NULL);
  double getBiasAndForces(const std::vector<double>&, std::vector<double>&, std::vector<double>&);
  double getBias(const std::vector<double>&, const bool parallel=true);
  double getFES_WellTempered(const std::vector<double>&, const bool parallel=true);
  //
  static void getBasisSetValues(const std::vector<double>&, std::vector<double>&, std::vector<BasisFunctions*>&, CoeffsVector*, Communicator* comm_in=NULL);
  void getBasisSetValues(const std::vector<double>&, std::vector<double>&, const bool parallel=true);
  // Bias grid and output stuff
  void setupBiasGrid(const bool usederiv=false);
  void updateBiasGrid();
  void resetStepOfLastBiasGridUpdate() {step_of_last_biasgrid_update = -1000;}
  void setStepOfLastBiasGridUpdate(long int step) {step_of_last_biasgrid_update = step;}
  long int getStepOfLastBiasGridUpdate() const {return step_of_last_biasgrid_update;}
  void writeBiasGridToFile(const std::string&, const bool append=false) const;
  //
  void setupFesGrid();
  void updateFesGrid();
  void resetStepOfLastFesGridUpdate() {step_of_last_fesgrid_update = -1000;}
  void setStepOfLastFesGridUpdate(long int step) {step_of_last_fesgrid_update = step;}
  long int getStepOfLastFesGridUpdate() const {return step_of_last_fesgrid_update;}
  void writeFesGridToFile(const std::string&, const bool append=false) const;
  //
  std::vector<unsigned int> getGridBins() const {return grid_bins_;}
  void setGridBins(const std::vector<unsigned int>&);
  void setGridBins(const unsigned int);
  //
  double getBeta() const {return beta_;}
  double getKbT() const {return kbt_;}
  //
  void setupUniformTargetDistribution();
  //
  void setupTargetDistribution(const std::vector<TargetDistribution*>&);
  void setupTargetDistribution(const std::vector<std::string>&);
  void setupTargetDistribution(TargetDistribution*);
  void setupTargetDistribution(const std::string&);
  //
  //
  double getBiasToFesScalingFactor() const {return bias_fes_scalingf_;}
  // Well-Tempered p(s) stuff
  void setupWellTemperedTargetDistribution(const double);
  double getWellTemperedBiasFactor() const {return welltemp_biasf_;}
  double getWellTemperedBetaPrime() const {return beta_prime_;}
  void setWellTemperedBiasFactor(const double);
  void updateWellTemperedTargetDistribution();
  //
  bool biasCutoffActive() const {return bias_cutoff_active_;}
  void setupBiasCutoffTargetDistribution();
  void updateBiasCutoffTargetDistribution();
  //
  void writeDynamicTargetDistGridToFile(const bool do_projections=false, const std::string& suffix="") const;
  //
private:
  //
  Grid* setupGeneralGrid(const std::string&, const std::vector<unsigned int>&, const bool usederiv=false);
  Grid* setupOneDimensionalMarginalGrid(const std::string&, const unsigned int, const unsigned int);
  void setupSeperableTargetDistribution(const std::vector<TargetDistribution*>&);
  void setupOneDimensionalTargetDistribution(const std::vector<TargetDistribution*>&);
  void setupNonSeperableTargetDistribution(const TargetDistribution*);
  //
  void calculateCoeffDerivsAverFromGrid(const Grid*, const bool normalize_dist=false);
  //
  void updateWellTemperedFESCoeffs();
  void updateWellTemperedPsGrid();
  //
  void updateBiasWithoutCutoffGrid();
  void updateBiasCutoffPsGrid();
  //
  bool isStaticTargetDistFileOutputActive() const;
  void writeTargetDistGridToFile(Grid*, const std::string& suffix="") const;
};


inline
void LinearBasisSetExpansion::setupTargetDistribution(const std::string& targetdist_keyword) {
  std::vector<std::string> targetdist_keywords(1);
  targetdist_keywords[0] = targetdist_keyword;
  setupTargetDistribution(targetdist_keywords);
}

inline
void LinearBasisSetExpansion::setupTargetDistribution(TargetDistribution* targetdist_pntr) {
  std::vector<TargetDistribution*> targetdist_pntrs(1);
  targetdist_pntrs[0] = targetdist_pntr;
  setupTargetDistribution(targetdist_pntrs);
}


inline
double LinearBasisSetExpansion::getBiasAndForces(const std::vector<double>& args_values, std::vector<double>& forces, std::vector<double>& coeffsderivs_values) {
  return getBiasAndForces(args_values,forces,coeffsderivs_values,basisf_pntrs_, bias_coeffs_pntr_, &mycomm_);
}


inline
double LinearBasisSetExpansion::getBias(const std::vector<double>& args_values, const bool parallel) {
  std::vector<double> forces_dummy(nargs_);
  std::vector<double> coeffsderivs_values_dummy(ncoeffs_);
  if(parallel){
    return getBiasAndForces(args_values,forces_dummy,coeffsderivs_values_dummy,basisf_pntrs_, bias_coeffs_pntr_, &mycomm_);
  }
  else{
    return getBiasAndForces(args_values,forces_dummy,coeffsderivs_values_dummy,basisf_pntrs_, bias_coeffs_pntr_, NULL);
  }
}


inline
double LinearBasisSetExpansion::getFES_WellTempered(const std::vector<double>& args_values, const bool parallel) {
  std::vector<double> forces_dummy(nargs_);
  std::vector<double> coeffsderivs_values_dummy(ncoeffs_);
  if(parallel){
    return getBiasAndForces(args_values,forces_dummy,coeffsderivs_values_dummy,basisf_pntrs_, fes_wt_coeffs_pntr_, &mycomm_);
  }
  else{
    return getBiasAndForces(args_values,forces_dummy,coeffsderivs_values_dummy,basisf_pntrs_, fes_wt_coeffs_pntr_, NULL);
  }
}


inline
void LinearBasisSetExpansion::getBasisSetValues(const std::vector<double>& args_values, std::vector<double>& basisset_values, const bool parallel) {
  if(parallel){
    getBasisSetValues(args_values,basisset_values,basisf_pntrs_, bias_coeffs_pntr_, &mycomm_);
  }
  else{
    getBasisSetValues(args_values,basisset_values,basisf_pntrs_, bias_coeffs_pntr_, NULL);
  }
}


}

}

#endif
