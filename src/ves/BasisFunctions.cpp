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

#include "BasisFunctions.h"
#include "TargetDistribution.h"
#include "TargetDistributionRegister.h"
#include "VesBias.h"
#include "VesTools.h"
#include "GridIntegrationWeights.h"

#include "tools/Grid.h"


namespace PLMD{
namespace ves{

void BasisFunctions::registerKeywords(Keywords& keys){
  Action::registerKeywords(keys);
  ActionWithValue::registerKeywords(keys);
  keys.add("compulsory","ORDER","The order of the basis functions.");
  keys.add("compulsory","INTERVAL_MIN","the minimum of the interval on which the basis functions are defined");
  keys.add("compulsory","INTERVAL_MAX","the maximum of the interval on which the basis functions are defined");
  keys.add("optional","NGRID_POINTS","the number of grid points used for numerical integrals");
  keys.addFlag("DEBUG_INFO",false,"print out more detailed information about the basis set, useful for debugging");
  keys.addFlag("NUMERICAL_INTEGRALS",false,"calculate basis function integral for the uniform distribution numerically");
}


BasisFunctions::BasisFunctions(const ActionOptions&ao):
Action(ao),
ActionWithValue(ao),
print_debug_info_(false),
has_been_set(false),
description_("Undefined"),
type_("Undefined"),
norder_(0),
nbasis_(1),
bf_label_prefix_("f"),
bf_labels_(nbasis_,"f0"),
periodic_(false),
interval_bounded_(true),
interval_intrinsic_min_str_("1.0"),
interval_intrinsic_max_str_("-1.0"),
interval_intrinsic_min_(1.0),
interval_intrinsic_max_(-1.0),
interval_intrinsic_range_(0.0),
interval_intrinsic_mean_(0.0),
interval_min_str_(""),
interval_max_str_(""),
interval_min_(0.0),
interval_max_(0.0),
interval_range_(0.0),
interval_mean_(0.0),
argT_derivf_(1.0),
numerical_uniform_integrals_(false),
nbins_(1001),
uniform_integrals_(nbasis_,0.0),
vesbias_pntr_(NULL),
action_pntr_(NULL)
{
  bf_keywords_.push_back(getName());
  if(keywords.exists("ORDER")){
    parse("ORDER",norder_); addKeywordToList("ORDER",norder_);
  }
  nbasis_=norder_+1;
  //
  std::string str_imin; std::string str_imax;
  if(keywords.exists("INTERVAL_MIN") && keywords.exists("INTERVAL_MAX")){
    parse("INTERVAL_MIN",str_imin); addKeywordToList("INTERVAL_MIN",str_imin);
    parse("INTERVAL_MAX",str_imax); addKeywordToList("INTERVAL_MAX",str_imax);
  }
  else {
    str_imin = "-1.0";
    str_imax = "1.0";
  }
  interval_min_str_ = str_imin;
  interval_max_str_ = str_imax;
  if(!Tools::convert(str_imin,interval_min_)){
    plumed_merror("cannot convert the value given in INTERVAL_MIN to a double");
  }
  if(!Tools::convert(str_imax,interval_max_)){
    plumed_merror("cannot convert the value given in INTERVAL_MAX to a double");
  }
  if(interval_min_>interval_max_){plumed_merror("INTERVAL_MIN and INTERVAL_MAX are not correctly defined");}
  //
  parseFlag("DEBUG_INFO",print_debug_info_);
  if(keywords.exists("NUMERICAL_INTEGRALS")){
    parseFlag("NUMERICAL_INTEGRALS",numerical_uniform_integrals_);
  }
  if(keywords.exists("NGRID_POINTS")){
    parse("NGRID_POINTS",nbins_);
  }
  // log.printf(" %s \n",getKeywordString().c_str());

}


void BasisFunctions::setIntrinsicInterval(const double interval_intrinsic_min_in, const double interval_intrinsic_max_in) {
  interval_intrinsic_min_ = interval_intrinsic_min_in;
  interval_intrinsic_max_ = interval_intrinsic_max_in;
  VesTools::convertDbl2Str(interval_intrinsic_min_,interval_intrinsic_min_str_);
  VesTools::convertDbl2Str(interval_intrinsic_max_,interval_intrinsic_max_str_);
  plumed_massert(interval_intrinsic_min_<interval_intrinsic_max_,"setIntrinsicInterval: intrinsic intervals are not defined correctly");
}


void BasisFunctions::setIntrinsicInterval(const std::string& interval_intrinsic_min_str_in, const std::string& interval_intrinsic_max_str_in) {
  interval_intrinsic_min_str_ = interval_intrinsic_min_str_in;
  interval_intrinsic_max_str_ = interval_intrinsic_max_str_in;
  if(!Tools::convert(interval_intrinsic_min_str_,interval_intrinsic_min_)){
    plumed_merror("setIntrinsicInterval: cannot convert string value given for the minimum of the intrinsic interval to a double");
  }
  if(!Tools::convert(interval_intrinsic_max_str_,interval_intrinsic_max_)){
    plumed_merror("setIntrinsicInterval: cannot convert string value given for the maximum of the intrinsic interval to a double");
  }
  plumed_massert(interval_intrinsic_min_<interval_intrinsic_max_,"setIntrinsicInterval: intrinsic intervals are not defined correctly");
}


void BasisFunctions::setInterval(const double interval_min_in, const double interval_max_in) {
  interval_min_ = interval_min_in;
  interval_max_ = interval_max_in;
  VesTools::convertDbl2Str(interval_min_,interval_min_str_);
  VesTools::convertDbl2Str(interval_max_,interval_max_str_);
  plumed_massert(interval_min_<interval_max_,"setInterval: intervals are not defined correctly");
}


void BasisFunctions::setInterval(const std::string& interval_min_str_in, const std::string& interval_max_str_in) {
  interval_min_str_ = interval_min_str_in;
  interval_max_str_ = interval_max_str_in;
  if(!Tools::convert(interval_min_str_,interval_min_)){
    plumed_merror("setInterval: cannot convert string value given for the minimum of the interval to a double");
  }
  if(!Tools::convert(interval_max_str_,interval_max_)){
    plumed_merror("setInterval: cannot convert string value given for the maximum of the interval to a double");
  }
  plumed_massert(interval_min_<interval_max_,"setInterval: intervals are not defined correctly");
}


void BasisFunctions::setupInterval() {
  // if(!intervalBounded()){plumed_merror("setupInterval() only works for bounded interval");}
  interval_intrinsic_range_ = interval_intrinsic_max_-interval_intrinsic_min_;
  interval_intrinsic_mean_  = 0.5*(interval_intrinsic_max_+interval_intrinsic_min_);
  interval_range_ = interval_max_-interval_min_;
  interval_mean_  = 0.5*(interval_max_+interval_min_);
  argT_derivf_ = interval_intrinsic_range_/interval_range_;
}


void BasisFunctions::setupLabels() {
  for(unsigned int i=0; i < nbasis_;i++){
    std::string is; Tools::convert(i,is);
    bf_labels_[i]=bf_label_prefix_+is+"(s)";
  }
}


void BasisFunctions::setupUniformIntegrals() {
  numerical_uniform_integrals_=true;
  numericalUniformIntegrals();
}


void BasisFunctions::setupBF() {
  if(interval_intrinsic_min_>interval_intrinsic_max_){plumed_merror("setupBF: default intervals are not correctly set");}
  setupInterval();
  setupLabels();
  if(bf_labels_.size()==1){plumed_merror("setupBF: the labels of the basis functions are not correct.");}
  if(!numerical_uniform_integrals_){setupUniformIntegrals();}
  else{numericalUniformIntegrals();}
  if(uniform_integrals_.size()==1){plumed_merror("setupBF: the integrals of the basis functions is not correct.");}
  if(type_=="Undefined"){plumed_merror("setupBF: the type of the basis function is not defined.");}
  if(description_=="Undefined"){plumed_merror("setupBF: the description of the basis function is not defined.");}
  has_been_set=true;
  printInfo();
}


void BasisFunctions::printInfo() const {
  if(!has_been_set){plumed_merror("the basis set has not be setup correctly");}
  log.printf("  One-dimensional basis set\n");
  log.printf("   Description: %s\n",description_.c_str());
  log.printf("   Type: %s\n",type_.c_str());
  if(periodic_){log.printf("   The basis functions are periodic\n");}
  log.printf("   Order of basis set: %u\n",norder_);
  log.printf("   Number of basis functions: %u\n",nbasis_);
  // log.printf("   Interval of basis set: %f to %f\n",interval_min_,interval_max_);
  log.printf("   Interval of basis set: %s to %s\n",interval_min_str_.c_str(),interval_max_str_.c_str());
  log.printf("   Description of basis functions:\n");
  for(unsigned int i=0; i < nbasis_;i++){log.printf("    %2u       %10s\n",i,bf_labels_[i].c_str());}
  //
  if(print_debug_info_){
    log.printf("  Debug information:\n");
    // log.printf("   Default interval of basis set: [%f,%f]\n",interval_intrinsic_min_,interval_intrinsic_max_);
    log.printf("   Intrinsic interval of basis set: [%s,%s]\n",interval_intrinsic_min_str_.c_str(),interval_intrinsic_max_str_.c_str());
    log.printf("   Intrinsic interval of basis set: range=%f,  mean=%f\n",interval_intrinsic_range_,interval_intrinsic_mean_);
    // log.printf("   Defined interval of basis set: [%f,%f]\n",interval_min_,interval_max_);
    log.printf("   Defined interval of basis set: [%s,%s]\n",interval_min_str_.c_str(),interval_max_str_.c_str());
    log.printf("   Defined interval of basis set: range=%f,  mean=%f\n",interval_range_,interval_mean_);
    log.printf("   Derivative factor due to interval translation: %f\n",argT_derivf_);
    log.printf("   Integral of basis functions over the interval:\n");
    if(numerical_uniform_integrals_){log.printf("   Note: calculated numerically\n");}
    for(unsigned int i=0; i < nbasis_;i++){log.printf("    %2u       %16.10f\n",i,uniform_integrals_[i]);}
    log.printf("   --------------------------\n");
  }
}


void BasisFunctions::linkVesBias(VesBias* vesbias_pntr_in){
  vesbias_pntr_ = vesbias_pntr_in;
  action_pntr_ = static_cast<Action*>(vesbias_pntr_in);
}


void BasisFunctions::linkAction(Action* action_pntr_in){
  action_pntr_ = action_pntr_in;
}


void BasisFunctions::numericalUniformIntegrals() {
  double h=(interval_max_-interval_min_)/nbins_;
  //
  bool dummy_bool=true;
  double dummy_dbl=0.0;
  for(unsigned int i=0; i < nbasis_;i++){
    // Trapezoidal rule on a uniform grid with Nbins+1 grid points
    double sum=0.0;
    for(unsigned int k=0; k < nbins_;k++){
      double x1 = interval_min_+(k)*h;
      double x2 = interval_min_+(k+1)*h;
      double v1 = getValue(x1,i,dummy_dbl,dummy_bool);
      double v2 = getValue(x2,i,dummy_dbl,dummy_bool);
      sum = sum + (v1+v2);
    }
    // norm with the "volume of the interval"
    uniform_integrals_[i] = (0.5*h*sum)/interval_range_;
  }
  // assume that the first function is the constant
  uniform_integrals_[0] = getValue(0.0,0,dummy_dbl,dummy_bool);
}


std::vector<double> BasisFunctions::numericalTargetDistributionIntegralsFromGrid(const Grid* grid_pntr) const {
  plumed_massert(grid_pntr!=NULL,"the grid is not defined");
  plumed_massert(grid_pntr->getDimension()==1,"the target distribution grid should be one dimensional");
  //
  std::vector<double> targetdist_integrals(nbasis_,0.0);
  std::vector<double> integration_weights = GridIntegrationWeights::getIntegrationWeights(grid_pntr);

  bool dummy_bool=true;
  double dummy_dbl=0.0;
  for(unsigned int i=0; i < nbasis_; i++){
    // Trapezoidal rule on a uniform grid with Nbins+1 grid points
    double sum=0.0;
    for(unsigned int k=0; k < grid_pntr->getSize(); k++){
      double arg = grid_pntr->getPoint(k)[0];
      sum += (integration_weights[k] * grid_pntr->getValue(k) ) * getValue(arg,i,dummy_dbl,dummy_bool);
    }
    targetdist_integrals[i] = sum;
  }
  //
  // assume that the first function is the constant
  targetdist_integrals[0] = getValue(0.0,0,dummy_dbl,dummy_bool);
  return targetdist_integrals;
}


std::vector<double> BasisFunctions::getTargetDistributionIntegrals(const TargetDistribution* targetdist_pntr) const {
  if(targetdist_pntr==NULL){
    return getUniformIntegrals();
  }
  else{
    Grid* targetdist_grid = targetdist_pntr->getTargetDistGridPntr();
    return numericalTargetDistributionIntegralsFromGrid(targetdist_grid);
  }
}


std::string BasisFunctions::getKeywordString() const {
  std::string str_keywords=bf_keywords_[0];
  for(unsigned int i=1; i<bf_keywords_.size();i++){str_keywords+=" "+bf_keywords_[i];}
  return str_keywords;
}


void BasisFunctions::getMultipleValue(const std::vector<double>& args, std::vector<double>& argsT, std::vector<std::vector<double> >& values, std::vector<std::vector<double> >& derivs) const {
  argsT.resize(args.size());
  values.clear();
  derivs.clear();
  for(unsigned int i=0; i<args.size(); i++){
    std::vector<double> tmp_values(getNumberOfBasisFunctions());
    std::vector<double> tmp_derivs(getNumberOfBasisFunctions());
    bool inside_interval=true;
    getAllValues(args[i],argsT[i],inside_interval,tmp_values,tmp_derivs);
    values.push_back(tmp_values);
    derivs.push_back(tmp_derivs);
  }
}


void BasisFunctions::writeBasisFunctionsToFile(OFile& ofile_values, OFile& ofile_derivs, unsigned int nbins_in, const bool ignore_periodicity, std::string output_fmt) const {

  std::vector<std::string> min(1); min[0]=intervalMinStr();
  std::vector<std::string> max(1); max[0]=intervalMaxStr();
  std::vector<unsigned int> nbins(1); nbins[0]=nbins_in;
  std::vector<Value*> value_pntr(1);
  value_pntr[0]= new Value(NULL,"arg",false);
  if(arePeriodic() && !ignore_periodicity){value_pntr[0]->setDomain(intervalMinStr(),intervalMaxStr());}
  else {value_pntr[0]->setNotPeriodic();}
  Grid args_grid = Grid("grid",value_pntr,min,max,nbins,false,false);

  std::vector<double> args(args_grid.getSize(),0.0);
  for(unsigned int i=0; i<args.size(); i++){
    args[i] = args_grid.getPoint(i)[0];
  }
  std::vector<double> argsT;
  std::vector<std::vector<double> > values;
  std::vector<std::vector<double> > derivs;

  ofile_values.addConstantField("bf_keywords").printField("bf_keywords","{"+getKeywordString()+"}");
  ofile_derivs.addConstantField("bf_keywords").printField("bf_keywords","{"+getKeywordString()+"}");

  ofile_values.addConstantField("min").printField("min",intervalMinStr());
  ofile_values.addConstantField("max").printField("max",intervalMaxStr());

  ofile_derivs.addConstantField("min").printField("min",intervalMinStr());
  ofile_derivs.addConstantField("max").printField("max",intervalMaxStr());

  ofile_values.addConstantField("nbins").printField("nbins",static_cast<int>(args_grid.getNbin()[0]));
  ofile_derivs.addConstantField("nbins").printField("nbins",static_cast<int>(args_grid.getNbin()[0]));

  if(arePeriodic()){
    ofile_values.addConstantField("periodic").printField("periodic","true");
    ofile_derivs.addConstantField("periodic").printField("periodic","true");
  }
  else{
    ofile_values.addConstantField("periodic").printField("periodic","false");
    ofile_derivs.addConstantField("periodic").printField("periodic","false");
  }

  getMultipleValue(args,argsT,values,derivs);
  ofile_values.fmtField(output_fmt);
  ofile_derivs.fmtField(output_fmt);
  for(unsigned int i=0; i<args.size(); i++){
    ofile_values.printField("arg",args[i]);
    ofile_derivs.printField("arg",args[i]);
    for(unsigned int k=0; k<getNumberOfBasisFunctions(); k++){
      ofile_values.printField(getBasisFunctionLabel(k),values[i][k]);
      ofile_derivs.printField("d_"+getBasisFunctionLabel(k),derivs[i][k]);
    }
    ofile_values.printField();
    ofile_derivs.printField();
  }
  ofile_values.fmtField();
  ofile_derivs.fmtField();

}


}
}
