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
#include "Optimizer.h"
#include "ves_tools/CoeffsVector.h"
#include "ves_tools/CoeffsMatrix.h"

#include "core/ActionRegister.h"


namespace PLMD{

class BachAveragedSGD : public Optimizer {
private:
public:
  static void registerKeywords(Keywords&);
  explicit BachAveragedSGD(const ActionOptions&);
  void coeffsUpdate(const unsigned int c_id = 0);
};


PLUMED_REGISTER_ACTION(BachAveragedSGD,"AVERAGED_SGD")


void BachAveragedSGD::registerKeywords(Keywords& keys){
  Optimizer::registerKeywords(keys);
  Optimizer::useFixedStepSizeKeywords(keys);
  Optimizer::useMultipleWalkersKeywords(keys);
  Optimizer::useHessianKeywords(keys);
  Optimizer::useMaskKeywords(keys);
  Optimizer::useRestartKeywords(keys);
  Optimizer::useMonitorAveragesKeywords(keys);
  Optimizer::useDynamicTargetDistributionKeywords(keys);
  }


BachAveragedSGD::BachAveragedSGD(const ActionOptions&ao):
PLUMED_OPTIMIZER_INIT(ao)
{
  turnOnHessian();
  checkRead();
}


void BachAveragedSGD::coeffsUpdate(const unsigned int c_id) {
  double aver_decay = 1.0 / ( getIterationCounterDbl() + 1.0 );
  AuxCoeffs(c_id) = AuxCoeffs(c_id) - StepSize(c_id)*CoeffsMask(c_id) * ( Gradient(c_id) + Hessian(c_id)*(AuxCoeffs(c_id)-Coeffs(c_id)) );
  //AuxCoeffs() = AuxCoeffs() - StepSize() * ( Gradient() + Hessian()*(AuxCoeffs()-Coeffs()) );
  Coeffs(c_id) += aver_decay * ( AuxCoeffs(c_id)-Coeffs(c_id) );
}


}
