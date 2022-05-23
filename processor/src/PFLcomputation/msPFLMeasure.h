#ifndef MSPFLMEASURE_H
#define MSPFLMEASURE_H

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
using namespace std;

//line from norms table in FaceBase
struct PFLTableLine
{
	int n;
	float sd;
	float pfl;
	float age;
};

class msPFLMeasure
{

public:
	msPFLMeasure();
	~msPFLMeasure();

	std::vector<PFLTableLine> rt_male;
	std::vector<PFLTableLine> rt_female;
	std::vector<PFLTableLine> lf_male;
	std::vector<PFLTableLine> lf_female;
	std::vector<PFLTableLine> PFL_female;
	std::vector<PFLTableLine> PFL_male;
	
	//Get right PFL male table
	std::vector<PFLTableLine> GetRightMalePFLTable()   { return this->rt_male; }
	//Get left PFL male table
	std::vector<PFLTableLine> GetLeftMalePFLTable()    { return this->lf_male; }
	//Get right PFL female table
	std::vector<PFLTableLine> GetRightFemalePFLTable() { return this->rt_female; }
	//Get left PFL male table
	std::vector<PFLTableLine> GetLeftFemalePFLTable()  { return this->lf_female; }
	//Get female PFL table
	std::vector<PFLTableLine> GetFemalePFLTable()      { return this->PFL_female; }
	//Get male PFL table
	std::vector<PFLTableLine> GetMalePFLTable()        { return this->PFL_male;	}

	std::vector<std::pair<float, float>> GetRunningMean(std::vector<PFLTableLine> pfl_info_table);

	//returns percentile value five a zscore using implementation of efoc() 
	double CalculatePercentile(double zScore);

	double CalculateZScore(std::vector<PFLTableLine> pfl_info_table, int age, double pfl);
};

#endif // MSPFLMEASURE_H 