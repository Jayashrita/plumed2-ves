/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2016-2017 The ves-code team
   (see the PEOPLE-VES file at the root of this folder for a list of names)

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

#include "TargetDistribution.h"
#include "TargetDistributionRegister.h"
#include "GridIntegrationWeights.h"
#include "VesTools.h"

#include "tools/Keywords.h"
#include "tools/Grid.h"
#include "core/Value.h"
#include "tools/File.h"


namespace PLMD {
namespace ves {


//+PLUMEDOC VES_TARGETDIST GRID_DIST
/*
Target distribution from an external grid file (static).

Using this keyword you can use a target distribution that is read from an
external grid file that is in the proper PLUMED file format. You do not to
give any information about the external grid file as all relevant information
should be automatically detected. It is assumed that the distribution read-in
from the grid is a proper probability distribution, i.e. always non-negative
and can be normalized.

By default the target distribution from the external grid is always normalized
inside the code. You can disable this normalization by using DO_NOT_NORMALIZE
keyword. However, be warned that this will generally lead to the wrong
behavior if the distribution from the external grid is not properly
normalized to 1.

If the distribution from the external grid file has for some reason
negative values can you use the SHIFT keyword to shift the distribution
by a given value. Another option is to use
the SHIFT_TO_ZERO keyword to shift the minimum of the distribution to zero.

Note that the number of grid bins used in the external grid file do not have
to be the same as used in the bias or action where the target distribution is
employed as the code will employ a spline interpolation to calculate
values.

It can happen that the intervals on which the target distribution is defined is
larger than the intervals covered by the external grid file. In this case the
default option is to consider the target distribution as continuous such that
values outside the boundary of the external grid file are the same as at
the boundary. This can be changed by using the ZERO_OUTSIDE keyword which
will make values outside to be taken as zero.

\par Examples

Generally you only need to provide the the filename of the external grid
file. The target distribution is then automatically normalized to 1 inside the code.
\verbatim
TARGET_DISTRIBUTION={GRID_DIST
                     FILE=input-grid.data}
\endverbatim

*/
//+ENDPLUMEDOC


class TD_Grid : public TargetDistribution {
  Grid* distGrid_;
  std::vector<double> minima_;
  std::vector<double> maxima_;
  std::vector<bool> periodic_;
  bool zero_outside_;
  double shift_;
public:
  static void registerKeywords( Keywords&);
  explicit TD_Grid( const TargetDistributionOptions& to );
  ~TD_Grid();
  double getValue(const std::vector<double>&) const ;
};


VES_REGISTER_TARGET_DISTRIBUTION(TD_Grid,"GRID_DIST")


void TD_Grid::registerKeywords(Keywords& keys) {
  TargetDistribution::registerKeywords(keys);
  keys.add("compulsory","FILE","The name of the external grid file to be used as a target distribution.");
  // keys.addFlag("NOSPLINE",false,"specifies that no spline interpolation is to be used when calculating the target distribution");
  keys.add("optional","SHIFT","Shift the grid read-in by some constant value. Due to normalization the final shift in the target distribution will generally not be the same as the value given here");
  keys.addFlag("ZERO_OUTSIDE",false,"By default the target distribution is continuous such that values outside the boundary of the external grid file are the same as at the boundary. This can be changed by using this flag which will make values outside to be taken as zero.");
  keys.addFlag("DO_NOT_NORMALIZE",false,"By default the target distribution from the external grid is always normalized inside the code. You can use this flag to disable this normalization. However, be warned that this will generally lead to the wrong behavior if the distribution from the external grid is not properly normalized to 1.");
  keys.use("BIAS_CUTOFF");
  keys.use("WELLTEMPERED_FACTOR");
  keys.use("SHIFT_TO_ZERO");
}

TD_Grid::~TD_Grid() {
  if(distGrid_!=NULL) {
    delete distGrid_;
  }
}


TD_Grid::TD_Grid(const TargetDistributionOptions& to):
  TargetDistribution(to),
  distGrid_(NULL),
  minima_(0),
  maxima_(0),
  zero_outside_(false),
  shift_(0.0)
{
  std::string filename;
  parse("FILE",filename);
  parse("SHIFT",shift_,true);
  if(shift_!=0.0) {
    if(isTargetDistGridShiftedToZero()) {plumed_merror(getName() + ": using both SHIFT and SHIFT_TO_ZERO is not allowed.");}
  }
  parseFlag("ZERO_OUTSIDE",zero_outside_);
  bool no_spline=false;
  // parseFlag("NOSPLINE",no_spline);
  bool use_spline = !no_spline;

  bool do_not_normalize=false;
  parseFlag("DO_NOT_NORMALIZE",do_not_normalize);
  if(do_not_normalize && shift_!=0.0) {plumed_merror(getName() + ": using both SHIFT and DO_NOT_NORMALIZE is not allowed.");}
  if(!do_not_normalize) {setForcedNormalization();}

  checkRead();

  std::string gridlabel;
  std::vector<std::string> arglabels;
  std::vector<std::string> argmin;
  std::vector<std::string> argmax;
  std::vector<bool> argperiodic;
  std::vector<unsigned int> argnbins;
  bool has_deriv = false;
  unsigned int nargs = VesTools::getGridFileInfo(filename,gridlabel,arglabels,argmin,argmax,argperiodic,argnbins,has_deriv);
  if(nargs==0) {
    plumed_merror(getName() + ": problem in parsing information from grid file");
  }

  setDimension(arglabels.size());
  std::vector<Value*> arguments(arglabels.size());
  for(unsigned int i=0; i < arglabels.size(); i++) {
    arguments[i]= new Value(NULL,arglabels[i],false);
    if(argperiodic[i]) {
      arguments[i]->setDomain(argmin[i],argmax[i]);
    }
    else {
      arguments[i]->setNotPeriodic();
    }
  }

  IFile gridfile; gridfile.open(filename);
  if(has_deriv) {
    distGrid_=Grid::create(gridlabel,arguments,gridfile,false,use_spline,true);
  }
  else {
    distGrid_=Grid::create(gridlabel,arguments,gridfile,false,false,false);
    if(use_spline) {distGrid_->enableSpline();}
  }
  gridfile.close();


  minima_.resize(getDimension());
  maxima_.resize(getDimension());
  periodic_.resize(getDimension());
  for (unsigned int i=0; i < getDimension(); i++) {
    Tools::convert(distGrid_->getMin()[i],minima_[i]);
    Tools::convert(distGrid_->getMax()[i],maxima_[i]);
    periodic_[i] = argperiodic[i];
    if(periodic_[i]) {maxima_[i]-=distGrid_->getDx()[i];}
  }

  for(unsigned int i=0; i < arguments.size(); i++) {delete arguments[i];}
  arguments.clear();
}


double TD_Grid::getValue(const std::vector<double>& argument) const {
  double outside = 0.0;
  std::vector<double> arg = argument;
  for(unsigned int k=0; k<getDimension(); k++) {
    if(zero_outside_ && (argument[k] < minima_[k] || argument[k] > maxima_[k])) {
      return outside;
    }
    else if(argument[k] < minima_[k]) {
      arg[k] = minima_[k];
    }
    else if(argument[k] > maxima_[k]) {
      arg[k] =maxima_[k];
    }
  }
  return distGrid_->getValue(arg)+shift_;
}


}
}
