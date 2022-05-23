#include "msPFLMeasure.h"
#include "vtkMath.h"

msPFLMeasure::msPFLMeasure()
{
	//this could/should be replaced by dynamic loading for race-specific tables. 
	const auto PFL_N = 35;

	//Age values (same for both)
	const float AGE[] = { 3.f,3.5f,4.f,4.5f,5.f,6.f,7.f,8.f,9.f,10.f,11.f,12.f,13.f,14.f,15.f,16.f,17.f,18.f,19.f,20.f,21.f,
		22.f,23.f,24.f,25.f,26.f,27.f,28.f,29.f,30.f,31.5f,33.5f,35.5f,37.5f,39.5f
	};

	//right male SD values
	const float rt_male_SD[] = { 1.91f,1.46f,1.61f,0.97f,1.39f,1.22f,2.25f,1.53f,1.5f,1.75f,2.21f,1.66f,2.21f,1.71f,2.26f,2.39f,
		2.34f,1.55f,2.12f,2.19f,1.65f,1.96f,1.6f,1.86f,1.78f,1.62f,2.11f,1.76f,1.91f,1.8f,2.2f,2.02f,1.89f,1.93f,2.12f
	};

	//right female SD values
	const float rt_female_SD[] = { 1.55f,2.03f,1.41f,0.7f,1.91f,1.52f,1.93f,1.71f,1.67f,1.18f,1.82f,1.88f,1.93f,1.8f,2.49f,1.7f,2.49f,
		2.03f,2.02f,1.78f,1.75f,1.82f,1.9f,1.83f,1.73f,1.7f,1.79f,1.74f,2.14f,2.49f,1.89f,1.89f,1.8f,2.f,2.33f
	};
 
	//left male SD values
	const float lf_male_SD[] = { 1.57f,1.43f,1.45f,1.18f,1.59f,1.35f,1.95f,1.61f,1.54f,2.26f,2.02f,1.75f,2.1f,2.14f,2.27f,
		2.27f,2.3f,1.72f,2.12f,2.22f,1.8f,1.92f,1.66f,1.87f,1.84f,1.8f,2.08f,1.74f,2.24f,1.98f,2.07f,1.61f,1.72f,1.92f,2.02f
	};
 
	//left female SD values
	const float lf_female_SD[] = { 1.54f,1.93f,1.47f,1.44f,1.87f,1.7f,1.78f,1.6f,1.16f,1.29f,2.05f,1.93f,2.16f,1.9f,2.28f,1.75f,2.9f,2.07f,
		2.18f,1.94f,1.86f,2.07f,1.85f,2.05f,1.87f,1.79f,1.8f,2.1f,2.21f,2.21f,2.11f,2.09f,1.89f,1.95f,2.12f
	};

	//Male N values
	const int male_N[] = { 10,18,12,10,24,21,27,20,16,17,23,23,20,16,15,10,5,30,29,28,41,40,71,47,58,49,37,28,33,37,
		28,19,24,25,21 };

	//female N values
	const int female_N[] = { 13,18,13,6,20,23,13,16,24,18,21,22,27,14,15,7,10,48,70,69,73,85,104,87,81,
		64,70,58,39,48,87,66,59,52,40 };
 
	//right male mean PFL values
	const float rt_male_MN[] = { 24.11f,24.28f,24.12f,24.43f,24.43f,24.75f,25.99f,25.82f,26.56f,27.11f,27.51f,27.54f,28.51f,28.63f,
		28.44f,27.84f,28.04f,29.55f,29.03f,29.01f,29.48f,29.02f,28.86f,28.54f,28.95f,28.39f,29.1f,29.03f,29.43f,
		28.9f,28.94f,28.99f,28.71f,29.06f,27.86f 
	};
 
	//right female mean PFL values
	const float rt_female_MN[] = { 23.22f,23.45f,24.55f,24.37f,24.15f,24.72f,26.2f,24.93f,25.93f,25.55f,26.97f,26.41f,27.65f,27.91f,27.52f,
		27.55f,28.65f,28.62f,28.7f,28.53f,28.57f,27.73f,27.83f,28.08f,28.2f,27.85f,27.87f,28.05f,27.86f,27.46f,
		27.66f,27.76f,27.7f,27.47f,27.69f 
	};

	//left male mean pfl values
	const float lf_male_MN[] = {23.39f,24.07f,23.64f,23.79f,24.59f,24.8f,25.86f,25.81f,26.22f,26.95f,26.67f,27.1f,28.77f,
		28.48f,28.65f,28.03f,27.04f,29.14f,28.93f,28.71f,29.3f,28.79f,28.47f,28.31f,28.71f,28.38f,28.66f,28.44f,28.95f,28.51f,
		28.37f,29.07f,28.77f,28.8f,27.67f 
	};

	//left female mean pfl values
	const float lf_female_MN[] = { 22.73f,23.14f,23.88f,23.96f,23.62f,24.49f,25.44f,24.49f,25.87f,25.17f,26.81f,25.87f,27.66f,27.66f,27.64f,27.53f,
		28.68f,28.14f,28.47f,28.04f,28.06f,27.51f,27.65f,27.85f,27.82f,27.59f,27.49f,27.87f,27.76f,27.17f,27.42f,27.68f,27.7f,27.21f,27.55f	
	};
 
	//intialise vector arrays
	for(int i = 0 ; i < PFL_N; i++)
	{
		PFLTableLine rt_m, lf_m, rt_f, lf_f, female, male;

		//initialise age
		rt_m.age = AGE[i]; 
		lf_m.age = AGE[i]; 
		rt_f.age = AGE[i]; 
		lf_f.age = AGE[i]; //initialise age
		female.age = AGE[i];
		male.age = AGE[i];

		//initialise sds
		rt_m.sd = rt_male_SD[i]; 
		lf_m.sd = lf_male_SD[i]; 
		rt_f.sd = rt_female_SD[i]; 
		lf_f.sd = lf_female_SD[i]; 
		female.sd =(lf_female_SD[i]+ rt_female_SD[i])/2.f;
		male.sd =	(lf_male_SD[i]+ rt_male_SD[i])/2.f;
 
		//initialise means
		rt_m.pfl = rt_male_MN[i]; 
		lf_m.pfl = lf_male_MN[i]; 
		rt_f.pfl = rt_female_MN[i]; 
		lf_f.pfl = lf_female_MN[i];
		female.pfl = (lf_female_MN[i]+ rt_female_MN[i])/2.f;
		male.pfl =(lf_male_MN[i]+ rt_male_MN[i])/2.f;

		//initialise n
		rt_m.n = male_N[i]; 
		lf_m.n = male_N[i]; 
		rt_f.n = female_N[i]; 
		lf_f.n = female_N[i]; 
		female.n = female_N[i];
		male.n = male_N[i];
		
		PFL_female.push_back(female);
		PFL_male.push_back(male);
		rt_male.push_back(rt_m);
		lf_male.push_back(lf_m);
		rt_female.push_back(rt_f);
		lf_female.push_back(lf_f);
	}
	  
}


msPFLMeasure::~msPFLMeasure()
{
}

//calculates age vs running mean
std::vector<std::pair<float, float>> msPFLMeasure::GetRunningMean(std::vector<PFLTableLine> pfl_info_table)
{
	std::vector<std::pair<float, float>> running_mean;
	running_mean.reserve(pfl_info_table.size());
	running_mean.resize(pfl_info_table.size());

	const int n_ma = 5;	//moving average size,
	//ASSERT(n_ma % 2); //must be odd number
	int pos = ceil(float(n_ma)/2.f); //centre index in n_ma -1
	
	for(int i = 0; i < pfl_info_table.size(); i++)
	{
		float sum = 0;
		
		//Add target 
		sum+=pfl_info_table.at(i).pfl;
		int n = 1;
		for(int j = 1; j < pos; j++)
		{
			if(i-j >= 0){
				sum += pfl_info_table.at(i-j).pfl;
				n++;
			}
			if(i+j < pfl_info_table.size()){
				sum += pfl_info_table.at(i+j).pfl;
				n++;
			}
		}
		running_mean[i] = std::make_pair(pfl_info_table.at(i).age, sum/n);
	}
	
	return running_mean;
}

//calculates Z score
double msPFLMeasure::CalculateZScore(std::vector<PFLTableLine> pfl_info_table, int age, double pfl)
{
	//match age
	//Find closest age in table
	float MIN=FLT_MAX;
	for (int i = 0; i < pfl_info_table.size(); ++i) 
	{ 
		if (abs(pfl_info_table.at(i).age - age) < MIN)
		{
			MIN = abs(pfl_info_table.at(i).age - age);
		}
	};
	PFLTableLine pfl_line;
	for (int i = 0; i < pfl_info_table.size(); ++i) 
	{ 
		if (abs(pfl_info_table.at(i).age - age) == MIN)
		{
			pfl_line = pfl_info_table.at(i);
		}
	}
	
	const float SD = pfl_line.sd;
	const float MN = pfl_line.pfl;

	return (pfl - MN)/SD;
}

double msPFLMeasure::CalculatePercentile(double zScore)
{
	const double x = (zScore / sqrt(2.0));    
	const double PI = vtkMath::Pi();
	const double a = (8.0 * (PI - 3.0)) / (3.0 * PI * (4.0 - PI));
	const double x2 = x * x;

	const double ax2 = a * x2;
	const double num = (4.0 / PI) + ax2;
	const double denom = 1.0 + ax2;
	const double inner = (-x2) * num / denom;
	const double erf2 = 1 - exp(inner);

	const double erf = sqrt(erf2);

	return zScore < 0 ? ((1.0 - erf) / 2.0) * 100.0 : ((1.0 + erf) / 2.0) * 100.0;
}