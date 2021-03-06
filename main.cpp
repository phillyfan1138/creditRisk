#define _USE_MATH_DEFINES
#include <iostream>
#include "FangOost.h"
#include <cmath>
#include "CreditUtilities.h"
#include <ctime>
#include <chrono> //for accurate multithreading time using std::chrono
#include "document.h" //rapidjson
#include "writer.h" //rapidjson
#include "stringbuffer.h" //rapidjson
#include "FunctionalUtilities.h"
#include "Vasicek.h"

template<typename Container, typename Range>
void printJson(const Container& myContainer, const Range& mn, const Range& dx){
	std::cout<<"{\"y\":[";
	auto n=myContainer.size();
	for(int i=0; i<n-1;++i){
		std::cout<<myContainer[i]<<",";
	}
	std::cout<<myContainer[n-1];
	std::cout<<"],\"xmin\":"<<mn<<",\"dx\":"<<dx<<"}";
}
struct loan{
	double pd;
	//std::function<Complex(const Complex&)> lgdCF;//characteristic function
	double exposure;
	std::vector<double> w;
	loan(double pd_, double exposure_, std::vector<double>&& w_){
			pd=pd_;
			exposure=exposure_;
			w=w_;
	}
	loan(){
    }
};

double getXmin(double expectedTotalExposure, double bL, double maxP, double tau ){
	double midPointPApprox=.5;
	double arbitraryConstantThatWorksWellInExperiments=5;
	return -expectedTotalExposure*bL*maxP*midPointPApprox*arbitraryConstantThatWorksWellInExperiments*tau;
}
double getRoughTotalExposure(double minLoanSize, double maxLoanSize, int numLoans){
	double midPointLApprox=.5;
	return (minLoanSize+midPointLApprox*(maxLoanSize-minLoanSize))*numLoans;
}
template<typename RAND>
auto getP(double minP, double maxP,const RAND& rand){
	return maxP*rand()/RAND_MAX+minP;
}
template<typename RAND>
auto getExposure(double minLoan, double maxLoan, const RAND& rand){
	return (maxLoan-minLoan)*rand()/RAND_MAX+minLoan;
}
template<typename RAND>
auto getNormalizedWeights(int numWeights, const RAND& rand){
	double totalW=0;
	return futilities::for_each_parallel(futilities::for_each_parallel(0, numWeights, [&](const auto& index){
		auto w=1.0*rand();
		totalW+=w;
		return w;
	}), [&](const auto& val, const auto& index){
		return val/totalW;
	});
}
int main(int argc, char* argv[]){
	int xNum=1024;
	int uNum=256;
  	int numLoans=100000;
	int m=1;
	double tau=1;
	double qUnscaled=.05;
	double lambdaUnscaled=.05;
	const double minLoanSize=10000;
	const double maxLoanSize=50000;
	const double minP=.0001;
	double maxP=.09;
	std::vector<double> alpha(m, .2);
	std::vector<double> sigma(m, .3);
	std::vector<double> y0(m, .5);
	std::vector< std::vector<double> > rho(m, std::vector<double> (m, 1.0));
	//These are parameters for the LGD CF function
  	const double alphL=.2;
	const double bL=.5;
	const double sigL=.2;
	srand(5);
	if(argc>1){
		rapidjson::Document parms;
		parms.Parse(argv[1]);//yield data
		if(parms.FindMember("xNum")!=parms.MemberEnd()){
			xNum=parms["xNum"].GetInt();
		}
		if(parms.FindMember("uNum")!=parms.MemberEnd()){
			uNum=parms["uNum"].GetInt();
		}
		if(parms.FindMember("n")!=parms.MemberEnd()){
			numLoans=parms["n"].GetInt();
		}
		if(parms.FindMember("q")!=parms.MemberEnd()){
			qUnscaled=parms["q"].GetDouble();
		}
		if(parms.FindMember("lambda")!=parms.MemberEnd()){
			lambdaUnscaled=parms["lambda"].GetDouble();
		}
		if(parms.FindMember("alpha")!=parms.MemberEnd()){
			alpha[0]=parms["alpha"].GetDouble();
		}
		if(parms.FindMember("sigma")!=parms.MemberEnd()){
			sigma[0]=parms["sigma"].GetDouble();
		}
		if(parms.FindMember("t")!=parms.MemberEnd()){
			tau=parms["t"].GetDouble();
		}
		if(parms.FindMember("maxP")!=parms.MemberEnd()){
			maxP=parms["maxP"].GetDouble();
		}
		if(parms.FindMember("x0")!=parms.MemberEnd()){
			y0[0]=parms["x0"].GetDouble();
		}
	}
	const auto loans=futilities::for_each_parallel(0, numLoans, [&](const auto& index){
		return loan(getP( minP, maxP, rand), getExposure(minLoanSize, maxLoanSize, rand), getNormalizedWeights(m, rand));
	});

	const double expectedTotalExposure=getRoughTotalExposure(minLoanSize, maxLoanSize, numLoans);
	const double lambda=lambdaUnscaled*expectedTotalExposure; //proxy for n*exposure*lambda
	const double q=qUnscaled/lambda;


	const double xmax=0;
	const double xmin=getXmin(expectedTotalExposure, bL, maxP, tau);
	const auto expectation=vasicek::computeIntegralExpectationLongRunOne(y0, alpha, alpha.size(), tau);
	const auto variance=vasicek::computeIntegralVarianceVasicek(alpha, sigma, rho, alpha.size(), tau);

	const auto density=fangoost::computeInv(xNum, uNum,  xmin, xmax, [&](const auto& u){
		return vasicek::getVasicekMFGFn(expectation, variance)(
			creditutilities::logLPMCF(
				creditutilities::getLiquidityRisk(u, lambda, q),
				loans,
				m, 
				[&](const auto& u, const auto& l){
					return creditutilities::lgdCF(u, l.exposure, alphL, bL, sigL, tau, bL);
				},
				[](const auto& loan){
					return loan.pd;
				},
				[](const auto& loan, const auto& index){
					return loan.w[index];
				}
			)
		);
	});
	const auto dx=fangoost::computeDX(xNum, xmin, xmax);
	printJson(density, xmin, dx);

}
