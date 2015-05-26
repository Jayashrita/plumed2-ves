/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2014 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#include "../function/Function.h"
#include "core/ActionRegister.h"
#include "core/ActionSet.h"
#include "core/PlumedMain.h"
#include "BasisFunctions.h"
#include "Coeffs.h"
#include "tools/File.h"
#include "LinearBiasExpansion.h"
#include "tools/Communicator.h"


// using namespace std;

namespace PLMD{
namespace function{

//+PLUMEDOC FUNCTION TEST_BASISFUNCTIONS
/*

*/
//+ENDPLUMEDOC


class TestBFs :
  public Function
{
  BasisFunctions* bf_pointer;
  Coeffs* coeffs;
  Coeffs* coeffs2;
  LinearBiasExpansion* bias_expansion;
  unsigned int bf_order_;
public:
  TestBFs(const ActionOptions&);
  void calculate();
  static void registerKeywords(Keywords& keys);
};


PLUMED_REGISTER_ACTION(TestBFs,"TEST_BASISFUNCTIONS")

void TestBFs::registerKeywords(Keywords& keys){
  Function::registerKeywords(keys);
  keys.addOutputComponent("value","default","value of basis function");
  keys.addOutputComponent("deriv","default","deriative of basis function");
  keys.addOutputComponent("arg","default","input argument");
  keys.addOutputComponent("argT","default","translated input argument");
  keys.addOutputComponent("inside","default","1.0 if inside interval, otherwise 0.0");
  keys.use("ARG");
  keys.add("compulsory","BASIS_SET","the label of the basis set that you want to use");
  keys.add("compulsory","N","the number of the basis function you want to test");
}

TestBFs::TestBFs(const ActionOptions&ao):
Action(ao),
Function(ao)
{
  // if(getNumberOfArguments()>1){error("only one argument allowed");}
  std::string basisset_label="";
  parse("BASIS_SET",basisset_label);
  bf_pointer=plumed.getActionSet().selectWithLabel<BasisFunctions*>(basisset_label);
  // bf_pointer->printInfo();
  parse("N",bf_order_);
  addComponent("value"); componentIsNotPeriodic("value");
  addComponent("deriv"); componentIsNotPeriodic("deriv");
  addComponent("arg"); componentIsNotPeriodic("arg");
  addComponent("argT"); componentIsNotPeriodic("argT");
  addComponent("inside"); componentIsNotPeriodic("inside");
  checkRead();
  log.printf("  using the %d order basis function from the %s basis set\n",bf_order_,basisset_label.c_str());

  std::vector<std::string> bf1;
  bf1.push_back("BF_FOURIER");
  bf1.push_back("ORDER=10");
  bf1.push_back("LABEL=bf2");
  bf1.push_back("INTERVAL_MIN=-pi");
  bf1.push_back("INTERVAL_MAX=pi");
  plumed.readInputWords(bf1);
  BasisFunctions* bf_pointer2=plumed.getActionSet().selectWithLabel<BasisFunctions*>("bf2");

  std::vector<BasisFunctions*> bf; bf.resize(2); bf[0]=bf_pointer; bf[1]=bf_pointer2;
  std::vector<Value*> args; args.resize(2); args[0]=getArguments()[0]; args[1]=getArguments()[1];
  bias_expansion = new LinearBiasExpansion("bla",args,bf,comm);
  coeffs = bias_expansion->getPointerToBiasCoeffs();
  // coeffs = new Coeffs("Test",args,bf,true,true);
  // coeffs->setValueAndAux(0,1000.0,0.0); 
  std::vector<unsigned int> nbins(2,300);
  bias_expansion->setupGrid(nbins);
  coeffs->setValueAndAux(20,1.0,0.0);
  coeffs->setValueAndAux(30,2.0,0.0);
  coeffs->setValueAndAux(100,2.0,0.0);
  bias_expansion->updateBiasGrid();
  bias_expansion->writeBiasGridToFile("bias.data",false);
  bias_expansion->writeBiasGridToFile("bias2.data",false);
  bias_expansion->writeBiasGridToFile("bias2.data",true);

  // coeffs->setValueAndAux(10,2.0000001, 400.0); 
  coeffs->writeToFile("TEST.data");
  // coeffs->writeToFile("TEST.data");
  // coeffs2 = Coeffs::createFromFile("TEST.data");
  // coeffs2->setCounter(100);
  // coeffs2->writeToFile("TEST.data",true,true);
 
  // std::vector<std::string> bfk=Coeffs::getBasisFunctionKeywordsFromFile("TEST.data");
  // plumed.readInputWords(Tools::getWords(bfk[0]));
  // plumed.readInputWords(Tools::getWords(bfk[1]));


}

void TestBFs::calculate(){
  
  std::vector<double> values(bf_pointer->getNumberOfBasisFunctions());
  std::vector<double> derivs(bf_pointer->getNumberOfBasisFunctions());
  bool inside_interval=true;
  double arg=getArgument(0);
  double argT=0.0;
  bf_pointer->getAllValues(arg,argT,inside_interval,values,derivs);

  getPntrToComponent("value")->set(values[bf_order_]);
  getPntrToComponent("deriv")->set(derivs[bf_order_]);
  getPntrToComponent("argT")->set(argT);
  getPntrToComponent("arg")->set(arg);
  if(inside_interval){getPntrToComponent("inside")->set(1.0);}
  else{getPntrToComponent("inside")->set(0.0);}
}

}
}


