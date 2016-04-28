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
#include "VesBias.h"
#include "ves_basisfunctions/BasisFunctions.h"
#include "ves_tools/CoeffsVector.h"
#include "ves_tools/CoeffsMatrix.h"
#include "ves_optimizers/Optimizer.h"
#include "ves_tools/FermiSwitchingFunction.h"
#include "ves_tools/VesTools.h"

#include "tools/Communicator.h"
#include "core/PlumedMain.h"
#include "core/Atoms.h"
#include "tools/File.h"


#include <iostream>


namespace PLMD{
namespace bias{

VesBias::VesBias(const ActionOptions&ao):
Action(ao),
Bias(ao),
ncoeffssets_(0),
coeffs_pntrs_(0),
targetdist_averages_pntrs_(0),
gradient_pntrs_(0),
hessian_pntrs_(0),
sampled_averages(0),
sampled_covariance(0),
use_multiple_coeffssets_(false),
coeffs_fnames(0),
ncoeffs_total_(0),
optimizer_pntr_(NULL),
optimize_coeffs_(false),
compute_hessian_(false),
diagonal_hessian_(true),
aver_counter(0.0),
kbt_(0.0),
targetdist_keywords_(0),
targetdist_pntrs_(0),
dynamic_targetdist_(false),
uniform_targetdist_(false),
welltemp_biasf_(1.0),
welltemp_targetdist_(false),
grid_bins_(0),
grid_min_(0),
grid_max_(0),
bias_filename_(""),
fes_filename_(""),
targetdist_filename_(""),
targetdist_averages_filename_(""),
coeffs_id_prefix_("c-"),
bias_fileoutput_active_(false),
fes_fileoutput_active_(false),
fesproj_fileoutput_active_(false),
dynamic_targetdist_fileoutput_active_(false),
static_targetdist_fileoutput_active_(true),
bias_cutoff_active_(false),
bias_cutoff_value_(0.0),
bias_current_max_value(0.0),
bias_cutoff_swfunc_pntr_(NULL)
{
  double temp=0.0;
  parse("TEMP",temp);
  if(temp>0.0){
    kbt_=plumed.getAtoms().getKBoltzmann()*temp;
  }
  else {
    kbt_=plumed.getAtoms().getKbT();
  }
  if(kbt_>0.0){
    log.printf("  KbT: %f\n",kbt_);
  }
  // NOTE: the check for that the temperature is given is done when linking the optimizer later on.

  if(keywords.exists("COEFFS")){
    parseVector("COEFFS",coeffs_fnames);
  }

  if(keywords.exists("GRID_BINS")){
    parseMultipleValues<unsigned int>("GRID_BINS",grid_bins_,getNumberOfArguments(),100);
  }

  if(keywords.exists("GRID_MIN") && keywords.exists("GRID_MAX")){
    parseMultipleValues("GRID_MIN",grid_min_,getNumberOfArguments());
    parseMultipleValues("GRID_MAX",grid_max_,getNumberOfArguments());
  }

  if(keywords.exists("TARGET_DISTRIBUTION")){
    std::string str_ps="";
    for(int i=1;;i++){
      if(!parseNumbered("TARGET_DISTRIBUTION",i,str_ps)){break;}
      targetdist_keywords_.push_back(str_ps);
    }
    str_ps="";
    parse("TARGET_DISTRIBUTION",str_ps);
    if(str_ps.size()>0){
      if(targetdist_keywords_.size()>0){
        plumed_merror("Either give a single target distribution using the TARGET_DISTRIBUTION keyword or multiple using numbered TARGET_DISTRIBUTION1,  TARGET_DISTRIBUTION2 keywords");
      }
      targetdist_keywords_.push_back(str_ps);
    }
  }

  if(getNumberOfArguments()>2){
    disableStaticTargetDistFileOutput();
  }

  if(keywords.exists("BIAS_FACTOR")){
    parse("BIAS_FACTOR",welltemp_biasf_);
    if(welltemp_biasf_<1.0){
      plumed_merror("the well-tempered bias factor doesn't make sense, it should be larger than 1.0");
    }
    if(welltemp_biasf_>1.0){
      welltemp_targetdist_=true;
      enableDynamicTargetDistribution();
      if(targetdist_keywords_.size()>0){
        plumed_merror("you cannot both specify a bias factor for a well-tempered target distribution using the BIAS_FACTOR keyword and give a target distribution with the TARGET_DISTRIBUTION keyword.");
      }
    }
  }

  if(keywords.exists("BIAS_FILENAME")){
    parse("BIAS_FILENAME",bias_filename_);
    if(bias_filename_.size()==0){
      bias_filename_ = "bias." + getLabel() + ".data";
    }
  }
  if(keywords.exists("FES_FILENAME")){
    parse("FES_FILENAME",fes_filename_);
    if(fes_filename_.size()==0){
      fes_filename_ = "fes." + getLabel() + ".data";
    }
  }
  if(keywords.exists("TARGETDIST_FILENAME")){
    parse("TARGETDIST_FILENAME",targetdist_filename_);
    if(targetdist_filename_.size()==0){
      targetdist_filename_ = "targetdist." + getLabel() + ".data";
    }
  }

  if(keywords.exists("TARGETDIST_AVERAGES_FILENAME")){
    parse("TARGETDIST_AVERAGES_FILENAME",targetdist_averages_filename_);
    if(targetdist_averages_filename_.size()==0){
      targetdist_averages_filename_ = "targetdist-averages." + getLabel() + ".data";
    }

  }

  if(keywords.exists("BIAS_CUTOFF")){
    double cutoff_value=0.0;
    parse("BIAS_CUTOFF",cutoff_value);
    if(cutoff_value<0.0){
      plumed_merror("the value given in BIAS_CUTOFF doesn't make sense, it should be larger than 0.0");
    }
    //
    if(cutoff_value>0.0){
      if(welltemp_targetdist_){
        plumed_merror("you cannot combined a cutoff on the bias (i.e. BIAS_CUTOFF) with a well-tempered target distribution (i.e. BIAS_FACTOR)");
      }
      if(targetdist_keywords_.size()>0){
        plumed_merror("you cannot combined a cutoff on the bias (i.e. BIAS_CUTOFF) with a target distribution given with the TARGET_DISTRIBUTION keyword.");
      }
      double fermi_lambda=1.0;
      parse("BIAS_CUTOFF_FERMI_LAMBDA",fermi_lambda);
      setupBiasCutoff(cutoff_value,fermi_lambda);
    }
  }

  if(keywords.exists("PROJ_ARG")){
    std::vector<std::string> proj_arg;
    for(int i=1;;i++){
      if(!parseNumberedVector("PROJ_ARG",i,proj_arg)){break;}
      // checks
      if(proj_arg.size() > (getNumberOfArguments()-1) ){
        plumed_merror("PROJ_ARG must be a subset of ARG");
      }
      //
      for(unsigned int k=0; k<proj_arg.size(); k++){
        bool found = false;
        for(unsigned int l=0; l<getNumberOfArguments(); l++){
          if(proj_arg[k]==getPntrToArgument(l)->getName()){
            found = true;
            break;
          }
        }
        if(!found){
          std::string s1; Tools::convert(i,s1);
          std::string error = "PROJ_ARG" + s1 + ": label " + proj_arg[k] + " is not among the arguments given in ARG";
          plumed_merror(error);
        }
      }
      //
      projection_args_.push_back(proj_arg);
    }
  }


}


VesBias::~VesBias(){
  for(unsigned int i=0; i<coeffs_pntrs_.size(); i++){
    delete coeffs_pntrs_[i];
  }
  for(unsigned int i=0; i<targetdist_averages_pntrs_.size(); i++){
    delete targetdist_averages_pntrs_[i];
  }
  for(unsigned int i=0; i<gradient_pntrs_.size(); i++){
    delete gradient_pntrs_[i];
  }
  for(unsigned int i=0; i<hessian_pntrs_.size(); i++){
    delete hessian_pntrs_[i];
  }
  if(bias_cutoff_swfunc_pntr_!=NULL){
    delete bias_cutoff_swfunc_pntr_;
  }
}


void VesBias::registerKeywords( Keywords& keys ) {
  Bias::registerKeywords(keys);
  keys.add("optional","TEMP","the system temperature - this is needed if the MD code does not pass the temperature to PLUMED.");
  //
  keys.addOutputComponent("bias","default","the instantaneous value of the bias potential.");
  keys.addOutputComponent("force2","default","the instantaneous value of the squared force due to this bias potential.");
  //
  keys.reserve("optional","COEFFS","read-in the coefficents from files.");
  //
  keys.reserve("numbered","TARGET_DISTRIBUTION","the target distribution to be used.");
    //
  keys.reserve("optional","BIAS_FACTOR","the bias factor to be used for the well-tempered target distribution.");
  //
  keys.reserve("optional","GRID_BINS","the number of bins used for the grid. The default value is 100 bins per dimension.");
  keys.reserve("optional","GRID_MIN","the lower bounds used for the grid.");
  keys.reserve("optional","GRID_MAX","the upper bounds used for the grid.");
  //
  keys.add("optional","BIAS_FILENAME","filename of the file on which the bias should be written out. By default it is bias.LABEL.data");
  keys.add("optional","FES_FILENAME","filename of the file on which the FES should be written out. By default it is fes.LABEL.data");
  keys.add("optional","TARGETDIST_FILENAME","filename of the file on which the target distribution info should be written out. By default it is targetdist.LABEL.data");
  keys.add("optional","TARGETDIST_AVERAGES_FILENAME","filename of the file for writing out the averages over the target distribution. By default it is targetdist-averages.LABEL.data");
  //
  keys.reserve("optional","BIAS_CUTOFF","cutoff the bias such that it only fills the free energy surface up to certain level F_cutoff, here you should give the value of the F_cutoff.");
  keys.reserve("optional","BIAS_CUTOFF_FERMI_LAMBDA","the lambda value used in the Fermi switching function for the bias cutoff (BIAS_CUTOFF). Lambda is by default 1.0.");
  //
  keys.reserve("numbered","PROJ_ARG","arguments for doing projections of the FES or the target distribution.");
}


void VesBias::useInitialCoeffsKeywords(Keywords& keys) {
  keys.use("COEFFS");
}


void VesBias::useTargetDistributionKeywords(Keywords& keys) {
  keys.use("TARGET_DISTRIBUTION");
}


void VesBias::useWellTemperdKeywords(Keywords& keys) {
  keys.use("BIAS_FACTOR");
}


void VesBias::useGridBinKeywords(Keywords& keys) {
  keys.use("GRID_BINS");
}


void VesBias::useGridLimitsKeywords(Keywords& keys) {
  keys.use("GRID_MIN");
  keys.use("GRID_MAX");
}


void VesBias::useBiasCutoffKeywords(Keywords& keys) {
  keys.use("BIAS_CUTOFF");
  keys.use("BIAS_CUTOFF_FERMI_LAMBDA");
}


void VesBias::useProjectionArgKeywords(Keywords& keys) {
  keys.use("PROJ_ARG");
}


void VesBias::addCoeffsSet(const std::vector<std::string>& dimension_labels,const std::vector<unsigned int>& indices_shape) {
  CoeffsVector* coeffs_pntr_tmp = new CoeffsVector("coeffs",dimension_labels,indices_shape,comm,true);
  initializeCoeffs(coeffs_pntr_tmp);
}


void VesBias::addCoeffsSet(std::vector<Value*>& args,std::vector<BasisFunctions*>& basisf) {
  CoeffsVector* coeffs_pntr_tmp = new CoeffsVector("coeffs",args,basisf,comm,true);
  initializeCoeffs(coeffs_pntr_tmp);
}


void VesBias::addCoeffsSet(CoeffsVector* coeffs_pntr_in) {
  initializeCoeffs(coeffs_pntr_in);
}


void VesBias::initializeCoeffs(CoeffsVector* coeffs_pntr_in) {
  //
  coeffs_pntr_in->linkVesBias(this);
  //
  std::string label;
  if(!use_multiple_coeffssets_ && ncoeffssets_==1){
    plumed_merror("you are not allowed to use multiple coefficient sets");
  }
  //
  label = getCoeffsSetLabelString("coeffs",ncoeffssets_);
  coeffs_pntr_in->setLabels(label);

  coeffs_pntrs_.push_back(coeffs_pntr_in);
  CoeffsVector* aver_ps_tmp = new CoeffsVector(*coeffs_pntr_in);
  label = getCoeffsSetLabelString("targetdist_averages",ncoeffssets_);
  aver_ps_tmp->setLabels(label);
  aver_ps_tmp->setValues(0.0);
  targetdist_averages_pntrs_.push_back(aver_ps_tmp);
  //
  CoeffsVector* gradient_tmp = new CoeffsVector(*coeffs_pntr_in);
  label = getCoeffsSetLabelString("gradient",ncoeffssets_);
  gradient_tmp->setLabels(label);
  gradient_pntrs_.push_back(gradient_tmp);
  //
  label = getCoeffsSetLabelString("hessian",ncoeffssets_);
  CoeffsMatrix* hessian_tmp = new CoeffsMatrix(label,coeffs_pntr_in,comm,diagonal_hessian_);
  hessian_pntrs_.push_back(hessian_tmp);
  //
  std::vector<double> aver_sampled_tmp;
  aver_sampled_tmp.assign(coeffs_pntr_in->numberOfCoeffs(),0.0);
  sampled_averages.push_back(aver_sampled_tmp);
  //
  std::vector<double> cov_sampled_tmp;
  cov_sampled_tmp.assign(hessian_tmp->getSize(),0.0);
  sampled_covariance.push_back(cov_sampled_tmp);
  //
  ncoeffssets_++;
}


void VesBias::clearCoeffsPntrsVector() {
  coeffs_pntrs_.clear();
}


void VesBias::readCoeffsFromFiles() {
  plumed_assert(ncoeffssets_>0);
  plumed_massert(keywords.exists("COEFFS"),"you are not allowed to use this function as the COEFFS keyword is not enabled");
  if(coeffs_fnames.size()>0){
    plumed_massert(coeffs_fnames.size()==ncoeffssets_,"COEFFS keyword is of the wrong size");
    if(ncoeffssets_==1){
      log.printf("  Read in coefficents from file ");
    }
    else{
      log.printf("  Read in coefficents from files:\n");
    }
    for(unsigned int i=0; i<ncoeffssets_; i++){
      IFile ifile;
      ifile.link(*this);
      ifile.open(coeffs_fnames[i]);
      if(!ifile.FieldExist(coeffs_pntrs_[i]->getDataLabel())){
        std::string error_msg = "Problem with reading coefficents from file " + ifile.getPath() + ": no field with name " + coeffs_pntrs_[i]->getDataLabel() + "\n";
        plumed_merror(error_msg);
      }
      size_t ncoeffs_read = coeffs_pntrs_[i]->readFromFile(ifile,false,false);
      coeffs_pntrs_[i]->setIterationCounterAndTime(0,getTime());
      if(ncoeffssets_==1){
        log.printf("%s (read %zu of %zu values)\n", ifile.getPath().c_str(),ncoeffs_read,coeffs_pntrs_[i]->numberOfCoeffs());
      }
      else{
        log.printf("   coefficent %u: %s (read %zu of %zu values)\n",i,ifile.getPath().c_str(),ncoeffs_read,coeffs_pntrs_[i]->numberOfCoeffs());
      }
      ifile.close();
    }
  }
}


void VesBias::updateGradientAndHessian() {
  for(unsigned int i=0; i<ncoeffssets_; i++){
    comm.Sum(sampled_averages[i]);
    comm.Sum(sampled_covariance[i]);
    Gradient(i) = TargetDistAverages(i) - sampled_averages[i];
    Hessian(i) = sampled_covariance[i];
    Hessian(i) *= getBeta();
    std::fill(sampled_averages[i].begin(), sampled_averages[i].end(), 0.0);
    std::fill(sampled_covariance[i].begin(), sampled_covariance[i].end(), 0.0);
  }
  aver_counter=0.0;
}


void VesBias::clearGradientAndHessian() {}


void VesBias::addToSampledAverages(const std::vector<double>& values, const unsigned c_id) {
  /*
  use the following online equation to calculate the average and covariance (see wikipedia)
      xm[n+1] = xm[n] + (x[n+1]-xm[n])/(n+1)
      cov(x,y)[n+1] = ( cov(x,y)[n]*n + (n/(n+1))*(x[n+1]-xm[n])*(y[n+1]-ym[n]) ) / (n+1)
                    = cov(x,y)[n]*(n/(n+1)) + ( n * (x[n+1]-xm[n])/(n+1) * (y[n+1]-ym[n])/(n+1) );
      n starts at 0.
  */
  size_t ncoeffs = numberOfCoeffs(c_id);
  std::vector<double> deltas(ncoeffs,0.0);
  size_t stride = comm.Get_size();
  size_t rank = comm.Get_rank();
  // update average and diagonal part of Hessian
  for(size_t i=rank; i<ncoeffs;i+=stride){
    size_t midx = getHessianIndex(i,i,c_id);
    deltas[i] = (values[i]-sampled_averages[c_id][i])/(aver_counter+1); // (x[n+1]-xm[n])/(n+1)
    sampled_averages[c_id][i] += deltas[i];
    sampled_covariance[c_id][midx] = sampled_covariance[c_id][midx] * ( aver_counter / (aver_counter+1) ) + aver_counter*deltas[i]*deltas[i];
  }
  comm.Sum(deltas);
  // update off-diagonal part of the Hessian
  if(!diagonal_hessian_){
    for(size_t i=rank; i<ncoeffs;i+=stride){
      for(size_t j=(i+1); j<ncoeffs;j++){
        size_t midx = getHessianIndex(i,j,c_id);
        sampled_covariance[c_id][midx] = sampled_covariance[c_id][midx] * ( aver_counter / (aver_counter+1) ) + aver_counter*deltas[i]*deltas[j];
      }
    }
  }
  // NOTE: the MPI sum for sampled_averages and sampled_covariance is done later
  //aver_counter += 1.0;
}


void VesBias::setTargetDistAverages(const std::vector<double>& coeffderivs_aver_ps, const unsigned coeffs_id) {
  TargetDistAverages(coeffs_id) = coeffderivs_aver_ps;
  TargetDistAverages(coeffs_id).setIterationCounterAndTime(this->getIterationCounter(),this->getTime());
}


void VesBias::setTargetDistAverages(const CoeffsVector& coeffderivs_aver_ps, const unsigned coeffs_id) {
  TargetDistAverages(coeffs_id) = coeffderivs_aver_ps;
  TargetDistAverages(coeffs_id).setIterationCounterAndTime(this->getIterationCounter(),this->getTime());
}


void VesBias::setTargetDistAveragesToZero(const unsigned coeffs_id) {
  TargetDistAverages(coeffs_id).setAllValuesToZero();
  TargetDistAverages(coeffs_id).setIterationCounterAndTime(this->getIterationCounter(),this->getTime());
}


void VesBias::checkThatTemperatureIsGiven() {
  if(kbt_==0.0){
    std::string err_msg = "VES bias " + getLabel() + " of type " + getName() + ": the temperature is needed so you need to give it using the TEMP keyword as the MD engine does not pass it to PLUMED.";
    plumed_merror(err_msg);
  }
}


unsigned int VesBias::getIterationCounter() const {
  unsigned int iteration = 0;
  if(optimizeCoeffs()){
    iteration = getOptimizerPntr()->getIterationCounter();
  }
  return iteration;
}


void VesBias::linkOptimizer(Optimizer* optimizer_pntr_in) {
  //
  if(optimizer_pntr_==NULL){
    optimizer_pntr_ = optimizer_pntr_in;
  }
  else {
    std::string err_msg = "VES bias " + getLabel() + " of type " + getName() + " has already been linked with optimizer " + optimizer_pntr_->getLabel() + " of type " + optimizer_pntr_->getName() + ". You cannot link two optimizer to the same VES bias.";
    plumed_merror(err_msg);
  }
  checkThatTemperatureIsGiven();
  optimize_coeffs_ = true;
}


void VesBias::enableHessian(const bool diagonal_hessian) {
  compute_hessian_=true;
  diagonal_hessian_=diagonal_hessian;
  sampled_covariance.clear();
  for (unsigned int i=0; i<ncoeffssets_; i++){
    delete hessian_pntrs_[i];
    std::string label = getCoeffsSetLabelString("hessian",i);
    hessian_pntrs_[i] = new CoeffsMatrix(label,coeffs_pntrs_[i],comm,diagonal_hessian_);
    std::vector<double> cov_sampled_tmp;
    cov_sampled_tmp.assign(hessian_pntrs_[i]->getSize(),0.0);
    sampled_covariance.push_back(cov_sampled_tmp);
  }
}


void VesBias::disableHessian() {
  compute_hessian_=false;
  diagonal_hessian_=true;
  sampled_covariance.clear();
  for (unsigned int i=0; i<ncoeffssets_; i++){
    delete hessian_pntrs_[i];
    std::string label = getCoeffsSetLabelString("hessian",i);
    hessian_pntrs_[i] = new CoeffsMatrix(label,coeffs_pntrs_[i],comm,diagonal_hessian_);
    std::vector<double> cov_sampled_tmp;
    cov_sampled_tmp.assign(hessian_pntrs_[i]->getSize(),0.0);
    sampled_covariance.push_back(cov_sampled_tmp);
  }
}


void VesBias::apply() {
  Bias::apply();
  aver_counter += 1.0;
}


std::string VesBias::getCoeffsSetLabelString(const std::string& type, const unsigned int coeffs_id) const {
  std::string label_prefix = getLabel() + ".";
  std::string label_postfix = "";
  if(use_multiple_coeffssets_){
    Tools::convert(coeffs_id,label_postfix);
    label_postfix = "-" + label_postfix;
  }
  return label_prefix+type+label_postfix;
}


void VesBias::updateTargetDistributions() {}


void VesBias::writeTargetDistAveragesToFile(const bool append) {
  for(unsigned int i=0; i<ncoeffssets_; i++){
    std::string fname = getCoeffsSetFilenameSuffix(i);
    getTargetDistAveragesPntr(i)->setIterationCounterAndTime(this->getIterationCounter(),this->getTime());
    fname = getTargetDistAveragesOutputFilename(fname);
    getTargetDistAveragesPntr(i)->writeToFile(fname,true,append,static_cast<Action*>(this));
  }
}


void VesBias::setGridBins(const std::vector<unsigned int>& grid_bins_in) {
  plumed_massert(grid_bins_in.size()==getNumberOfArguments(),"the number of grid bins given doesn't match the number of arguments");
  grid_bins_=grid_bins_in;
}


void VesBias::setGridBins(const unsigned int nbins) {
  std::vector<unsigned int> grid_bins_in(getNumberOfArguments(),nbins);
  grid_bins_=grid_bins_in;
}


void VesBias::setGridMin(const std::vector<double>& grid_min_in) {
  plumed_massert(grid_min_in.size()==getNumberOfArguments(),"the number of lower bounds given for the grid doesn't match the number of arguments");
  grid_min_=grid_min_in;
}


void VesBias::setGridMax(const std::vector<double>& grid_max_in) {
  plumed_massert(grid_max_in.size()==getNumberOfArguments(),"the number of upper bounds given for the grid doesn't match the number of arguments");
  grid_max_=grid_max_in;
}


std::string VesBias::getCurrentOutputFilename(const std::string& base_filename, const std::string& suffix) const {
  std::string filename = base_filename;
  if(suffix.size()>0){
    filename = FileBase::appendSuffix(filename,"."+suffix);
  }
  if(optimizeCoeffs()){
    filename = FileBase::appendSuffix(filename,"."+getIterationFilenameSuffix());
  }
  return filename;
}


std::string VesBias::getCurrentTargetDistOutputFilename(const std::string& suffix) const {
  std::string filename = targetdist_filename_;
  if(suffix.size()>0){
    filename = FileBase::appendSuffix(filename,"."+suffix);
  }
  if(optimizeCoeffs() && dynamicTargetDistribution()){
    filename = FileBase::appendSuffix(filename,"."+getIterationFilenameSuffix());
  }
  return filename;
}


std::string VesBias::getIterationFilenameSuffix() const {
  std::string iter_str;
  Tools::convert(getOptimizerPntr()->getIterationCounter(),iter_str);
  iter_str = "iter-" + iter_str;
  return iter_str;
}


std::string VesBias::getCoeffsSetFilenameSuffix(const unsigned int coeffs_id) const {
  std::string suffix = "";
  if(use_multiple_coeffssets_){
    Tools::convert(coeffs_id,suffix);
    suffix = coeffs_id_prefix_ + suffix;
  }
  return suffix;
}


std::string VesBias::getTargetDistAveragesOutputFilename(const std::string& suffix) const {
  std::string filename = targetdist_averages_filename_;
  if(suffix.size()>0){
    filename = FileBase::appendSuffix(filename,"."+suffix);
  }
  return filename;
}


void VesBias::setupBiasCutoff(const double bias_cutoff_value, const double fermi_lambda) {
  //
  double fermi_exp_max = 100.0;
  //
  std::string str_bias_cutoff_value; VesTools::convertDbl2Str(bias_cutoff_value,str_bias_cutoff_value);
  std::string str_fermi_lambda; VesTools::convertDbl2Str(fermi_lambda,str_fermi_lambda);
  std::string str_fermi_exp_max; VesTools::convertDbl2Str(fermi_exp_max,str_fermi_exp_max);
  std::string swfunc_keywords = "FERMI R_0=" + str_bias_cutoff_value + " FERMI_LAMBDA=" + str_fermi_lambda + " FERMI_EXP_MAX=" + str_fermi_exp_max;
  //
  if(bias_cutoff_swfunc_pntr_!=NULL){delete bias_cutoff_swfunc_pntr_;}
  std::string swfunc_errors="";
  bias_cutoff_swfunc_pntr_ = new FermiSwitchingFunction();
  bias_cutoff_swfunc_pntr_->set(swfunc_keywords,swfunc_errors);
  if(swfunc_errors.size()>0){
    plumed_merror("problem with setting up Fermi switching function: " + swfunc_errors);
  }
  //
  bias_cutoff_value_=bias_cutoff_value;
  bias_cutoff_active_=true;
  enableDynamicTargetDistribution();
}


double VesBias::getBiasCutoffSwitchingFunction(const double bias, double& deriv_factor) const {
  plumed_massert(bias_cutoff_active_,"The bias cutoff is not active so you cannot call this function");
  double arg = -(bias-bias_current_max_value);
  double deriv=0.0;
  double value = bias_cutoff_swfunc_pntr_->calculate(arg,deriv);
  // as FermiSwitchingFunction class has different behavior from normal SwitchingFunction class
  // I was having problems with NaN as it was dividing with zero
  // deriv *= arg;
  deriv_factor = value-bias*deriv;
  return value;
}


}
}
