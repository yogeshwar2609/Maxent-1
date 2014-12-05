/*****************************************************************************
 *
 * ALPS Project Applications
 *
 * Copyright (C) 2010 by Sebastian Fuchs <fuchs@comp-phys.org>
 *                       Thomas Pruschke <pruschke@comp-phys.org>
 *                       Matthias Troyer <troyer@comp-phys.org>
 *
 * This software is part of the ALPS Applications, published under the ALPS
 * Application License; you can use, redistribute it and/or modify it under
 * the terms of the license, either version 1 or (at your option) any later
 * version.
 * 
 * You should have received a copy of the ALPS Application License along with
 * the ALPS Applications; see the file LICENSE.txt. If not, the license is also
 * available from http://alps.comp-phys.org/.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT 
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE 
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, 
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************/


#include "default_model.hpp"





///This class deals with tabulated model functions
TabFunction::TabFunction(const alps::params& p, std::string const& name){
  std::string p_name = p[name].cast<std::string>();
    std::ifstream defstream(p_name.c_str());
    if (!defstream)
      boost::throw_exception(std::invalid_argument("could not open default model file: "+p[name]));
    double om, D;
    std::string line;
    while (getline(defstream, line)) {
      if(line.size()>0 && line[0]=='#') continue;
      std::stringstream linestream(line);
      linestream>>om>>D;
      Omega.push_back(om);
      Def.push_back(D);
    }
    double omega_max = p["OMEGA_MAX"]; 
    double omega_min(static_cast<double>(p["OMEGA_MIN"]|-omega_max)); //we had a 0 here in the bosonic case. That's not a good idea if you're continuing symmetric functions like chi(omega)/omega. Change omega_min to zero manually if you need it.
    if (Omega[0]!=omega_min || Omega.back()!=omega_max){
      std::cout<<"Omega[ 0] "<<Omega[0]<<" omega min: "<<omega_min<<std::endl;
      std::cout<<"Omega[-1] "<<Omega.back()<<" omega max: "<<omega_max<<std::endl;
    }
}

  //regturn value of default model. If INSIDE interval we have data in: return linearly interpolated data. Otherwise: return zero.
  double TabFunction::operator()(const double omega) {
    //for values outside the grid point: return zero:
    if(omega < Omega[0] || omega > Omega.back())
      return 0.;

    //otherwise linear interpolation
    std::vector<double>::const_iterator ub = std::upper_bound(Omega.begin(), Omega.end(), omega);
    int index = ub - Omega.begin();
    double om1 = Omega[index-1];
    double om2 = Omega[index];
    double D1 = Def[index-1];
    double D2 = Def[index];
    return -(D2-D1)/(om2-om1)*(om2-omega)+D2;      
  }


  GeneralDefaultModel::GeneralDefaultModel(const alps::params& p, boost::shared_ptr<Model> mod)
: DefaultModel(p)
, Mod(mod)
, ntab(5001)
, xtab(ntab) {
    double sum = norm();
    for (int o=0; o<ntab; ++o) {
      xtab[o] *= 1./sum;
    }
}

  ///given a number x between 0 and 1, find the frequency omega belonging to x.
  double GeneralDefaultModel::omega(const double x) const {
    //range check for x
    if(x>1. || x<0.)
      throw std::logic_error("parameter x is out of bounds!");
    std::vector<double>::const_iterator ub = std::upper_bound(xtab.begin(), xtab.end(), x);
    int omega_index = ub - xtab.begin();
    if (ub==xtab.end())
      omega_index = xtab.end() - xtab.begin() - 1;
    double om1 = omega_min + (omega_index-1)*(omega_max-omega_min)/(ntab-1);
    double om2 = omega_min + omega_index*(omega_max-omega_min)/(ntab-1);
    double x1 = xtab[omega_index-1];
    double x2 = xtab[omega_index];
    return -(om2-om1)/(x2-x1)*(x2-x)+om2;      
  }

  /// returns the value of the model function at frequency omega
  double GeneralDefaultModel::D(const double omega) const {
    return (*Mod)(omega);
  }

  //I have no idea what this does.
  double GeneralDefaultModel::x(const double t) const {
    if(t>1. || t<0.) throw std::logic_error("parameter t is out of bounds!");
    int od = (int)(t*(ntab-1));
    if (od==(ntab-1)) 
      return 1.;
    double x1 = xtab[od];
    double x2 = xtab[od+1];
    return -(x2-x1)*(od+1-t*ntab)+x2;      
  }

  double GeneralDefaultModel::norm() {
    double sum = 0;
    xtab[0] = 0.;
    //this is an evaluation on an equidistant grid; sum integrated by trapezoidal rule
    double delta_omega = (omega_max - omega_min) / (ntab - 1);
    for (int o = 1; o < ntab; ++o) {
      double omega1 = omega_min + (o - 1) * delta_omega;
      double omega2 = omega_min + o * delta_omega;
      sum += ((*Mod)(omega1) + (*Mod)(omega2)) / 2. * delta_omega;
      xtab[o] = sum;
    }
    return sum;
  }

boost::shared_ptr<DefaultModel> make_default_model(const alps::params& parms, std::string const& name){
  std::string p_name = parms[name]|"flat";
  if (p_name == "flat") {
    std::cout << "Using flat default model" << std::endl;
    return boost::shared_ptr<DefaultModel>(new FlatDefaultModel(parms));
  }
  else if (p_name == "gaussian") {
    std::cout << "Using Gaussian default model" << std::endl;
    boost::shared_ptr<Model> Mod(new Gaussian(parms));
    return boost::shared_ptr<DefaultModel>(new GeneralDefaultModel(parms, Mod));
  }
  else if (p_name == "twogaussians") {
    std::cout << "Using sum of two Gaussians default model" << std::endl;
    boost::shared_ptr<Model> Mod(new TwoGaussians(parms));
    return boost::shared_ptr<DefaultModel>(new GeneralDefaultModel(parms, Mod));
  }
  else if (p_name == "shifted gaussian") {
    std::cout << "Using shifted Gaussian default model" << std::endl;
    boost::shared_ptr<Model> Mod(new ShiftedGaussian(parms));
    return boost::shared_ptr<DefaultModel>(new GeneralDefaultModel(parms, Mod));
  }
  else if (p_name == "double gaussian") {
    std::cout << "Using double Gaussian default model" << std::endl;
    boost::shared_ptr<Model> Mod(new DoubleGaussian(parms));
    return boost::shared_ptr<DefaultModel>(new GeneralDefaultModel(parms, Mod));
  }
  else if (p_name == "general double gaussian") {
    std::cout << "Using general double Gaussian default model" << std::endl;
    boost::shared_ptr<Model> Mod(new GeneralDoubleGaussian(parms));
    return boost::shared_ptr<DefaultModel>(new GeneralDefaultModel(parms, Mod));
  }
  else if (p_name == "linear rise exp decay") {
    std::cout << "Using linear rise exponential decay default model" << std::endl;
    boost::shared_ptr<Model> Mod(new LinearRiseExpDecay(parms));
    return boost::shared_ptr<DefaultModel>(new GeneralDefaultModel(parms, Mod));
  }
  else if (p_name == "quadratic rise exp decay") {
    std::cout << "Using quadratic rise exponential decay default model" << std::endl;
    boost::shared_ptr<Model> Mod(new QuadraticRiseExpDecay(parms));
    return boost::shared_ptr<DefaultModel>(new GeneralDefaultModel(parms, Mod));
  }
  else { 
    std::cout << "Using tabulated default model" << std::endl;
    boost::shared_ptr<Model> Mod(new TabFunction(parms, name));
    return boost::shared_ptr<DefaultModel>(new GeneralDefaultModel(parms, Mod));
  }
        }



