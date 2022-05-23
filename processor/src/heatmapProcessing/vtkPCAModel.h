// vtkPCAModel.h

#ifndef __vtkPCAModel_h
#define __vtkPCAModel_h

#include <stdio.h>

class vtkDoubleArray;

class vtkPCAModel
{
public:
	  
    virtual vtkDoubleArray* GetEvals() = 0;

    virtual int GetModesRequiredFor(float proportion) = 0;

    virtual int GetTotalNumModes() = 0;

    virtual void GetModesForInput(int i,vtkDoubleArray *b,int b_size)=0;

    virtual bool SaveToFile(FILE*,float) { return false; }

    virtual bool LoadFromFile(FILE*) { return false; }
 
 
};

#endif
