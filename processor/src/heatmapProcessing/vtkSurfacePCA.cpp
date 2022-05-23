#include <vtkObjectFactory.h>
#include <vtkTransformFilter.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkPolyData.h>
#include <vtkMath.h>
#include <vtkDoubleArray.h>
#include <vtkCellLocator.h>
#include <vtkIterativeClosestPointTransform.h>
#include <vtkLandmarkTransform.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkOpenGLProperty.h>
#include <vtkThinPlateSplineTransform.h>
#include <vtkSphereSource.h>
#include <vtkGlyph3D.h>
#include <vtkGenericCell.h>
#include <vtkCell.h>
#include <vtkCellArray.h>
#include <vtkPointData.h>
#include <vtkTriangleFilter.h>
#include <vtkSmartPointer.h>
#include <vtkFloatArray.h> //for resample surface

#include "vtkSurfacePCA.h"
#include "../mathUtils/faceScreenMath.h"

#define vtkErrorMacro_pca(X) std::cout << "In vtkSurfacePCA.cpp: VTK error message: " X << endl;
#define vtkDebugMacro_pca(X) std::cout << "In vtkSurfacePCA.cpp: VTK debug message: " X << endl;
#define mfcGUImessage_pca(X) std::cout << "In vtkSurfacePCA.cpp: GUI message: " << X << endl;

// TODO: required ??  vtkCxxRevisionMacro(vtkSurfacePCA, "$Revision: 1.5 $");
vtkStandardNewMacro(vtkSurfacePCA);
 
using faceScreenMath::CHI_SQUARE_97PT5;
using faceScreenMath::NewMatrix;
using faceScreenMath::DeleteMatrix;
using faceScreenMath::NewVector;
using faceScreenMath::DeleteVector;
using faceScreenMath::SmallCovarianceMatrix;
using faceScreenMath::NormaliseColumns;
using faceScreenMath::SubtractMeanColumn;
using faceScreenMath::MatrixMultiply;

//----------------------------------------------------------------------------
// protected
vtkSurfacePCA::vtkSurfacePCA()
{
    this->Evals = vtkDoubleArray::New();
    this->evecMat2 = NULL;
    this->meanshape = NULL;
    this->modes_for_inputs = NULL;
    this->tcoords = NULL;
    this->polys = NULL;
    
    this->example_landmarks = NULL;
    this->example_surface = NULL;
    this->mean_surface_landmarks = NULL;
	this->mean_landmarks = NULL;
	this->current_landmarks = NULL;
	this->pseudo_landmarks_loaded = false;
	this->pseudo_landmark_indexes = NULL;
}

//----------------------------------------------------------------------------
// protected
vtkSurfacePCA::~vtkSurfacePCA()
{
    if (this->modes_for_inputs) {
        for(int i=0;i<this->Evals->GetNumberOfTuples();i++)
        {
            this->modes_for_inputs[i]->Delete();
        }
        delete []this->modes_for_inputs;
        this->modes_for_inputs = NULL;
    }

    
    if (this->Evals) {
        this->Evals->Delete();
    }
    if (this->evecMat2) {
        DeleteMatrix(this->evecMat2);
        this->evecMat2 = NULL;
    }
    if (this->meanshape) {
        DeleteVector(this->meanshape);
        this->meanshape = NULL;
    }
    if(this->tcoords) 
        delete []tcoords;
    if(this->polys) 
        delete []polys;
    if(this->mean_surface_landmarks)
        delete []this->mean_surface_landmarks;
    if(this->example_landmarks)        this->example_landmarks->Delete();
    if(this->example_surface)	       this->example_surface->Delete();
	if(this->mean_landmarks)	       delete []this->mean_landmarks; //for some reason crashes everythign!? Even though it's not deleted anywhere else.
    if(this->pseudo_landmark_indexes)  delete []this->pseudo_landmark_indexes;
	if(this->current_landmarks)        delete []this->current_landmarks;
}

void vtkSurfacePCA::Update()
{
	// TODO tidy up
	//vtkAlgorithm::
		Update(); //Call (vtkSource)->vtkAlgorithm update
}
 
void vtkSurfacePCA::InitialiseParameterisedShape(vtkSmartPointer<vtkPolyData> shape)
{
	
	shape->Allocate(this->n_cells);
	for (int i = 0; i < this->n_cells; i++)
	{
		vtkIdType cell[3]; 
		cell[0] = this->polys[i * 3];
		cell[1] = this->polys[i * 3 + 1];
		cell[2] = this->polys[i * 3 + 2];
		shape->InsertNextCell(VTK_TRIANGLE, 3, cell);
	}
	
	vtkNew<vtkDoubleArray> textureCoords;
	textureCoords->SetNumberOfComponents(2);
	textureCoords->SetNumberOfTuples(this->N);
	for (int i = 0; i < this->N; i++)
	{
		textureCoords->SetTuple(i, &this->tcoords[i * 2]);
	}
	shape->GetPointData()->SetTCoords(textureCoords);

	vtkNew<vtkPoints> points;
	points->SetNumberOfPoints(this->N);
	for (int i = 0; i < this->N; i++) 
	{
		points->SetPoint(i, meanshape[i * 3], meanshape[i * 3 + 1], meanshape[i * 3 + 2]);
	}
	shape->SetPoints(points);

}

// Replacement (without case b.empty()==true) for: void vtkSurfacePCA::GetParameterisedShape(vtkDoubleArray *b, vtkPolyData* shape)
void vtkSurfacePCA::ParameteriseShape(vtkSmartPointer<vtkDoubleArray> b, vtkSmartPointer<vtkPolyData> shape)
{
	// rh: It should be possible to take this data struct directly from the mean shape, rather than creating it again!
	if (!shape->GetPolys() || shape->GetPolys()->GetNumberOfCells() != this->n_cells)
	{
		shape->Allocate(this->n_cells);
		for (int i = 0; i < this->n_cells; i++)
		{
			shape->InsertNextCell(VTK_TRIANGLE, 3, &this->polys[i * 3]);
		}
	}

	if (!shape->GetPointData()->GetTCoords() || shape->GetPointData()->GetTCoords()->GetNumberOfTuples() != this->N)
	{
		vtkNew<vtkDoubleArray> textureCoords;
		textureCoords->SetNumberOfComponents(2);
		textureCoords->SetNumberOfTuples(this->N);
		for (int i = 0; i < this->N; i++)
		{
			textureCoords->SetTuple(i, &this->tcoords[i * 2]);
		}
		shape->GetPointData()->SetTCoords(textureCoords);
	}

	const int bsize = b->GetNumberOfTuples();
	std::vector<double> weights;
	weights.resize(bsize);
	for (int i = 0; i < bsize; i++) 
	{
		const double eigenValue = this->Evals->GetValue(i);
		weights[i] = sqrt(eigenValue) * b->GetValue(i);
	}
	
	std::vector<double> shapeVector;
	shapeVector.resize(N * 3);
	for (int j = 0; j < this->N * 3; j++) {
		shapeVector[j] = meanshape[j];
		for (int i = 0; i < bsize; i++) {
			const double eval = this->Evals->GetValue(i);
			if (fabs(eval) > 1E-5) // Use only eigenvectors with non-zero eigenvalues
				shapeVector[j] += weights[i] * evecMat2[j][i]; 
		}
	}

	vtkNew<vtkPoints> points;
	points->SetNumberOfPoints(this->N);
	for (int i = 0; i < this->N; i++) {
		points->SetPoint(i, shapeVector[i * 3], shapeVector[i * 3 + 1], shapeVector[i * 3 + 2]);
	}
	shape->SetPoints(points);
}

// TODO : Consider removing this function: It appears to be a duplicate of vtkSurfacePCA::ParameteriseShape above.
void vtkSurfacePCA::GetParameterisedShape(vtkDoubleArray *b, vtkPolyData* shape)
{
    if(shape->GetNumberOfPoints() != this->N) 
    {
        vtkNew<vtkPoints> points;
        points->SetNumberOfPoints(this->N);
        shape->SetPoints(points);
    }

    if(!shape->GetPolys() || shape->GetPolys()->GetNumberOfCells()!=this->n_cells)
    {
        shape->Allocate(this->n_cells);
        for(int i=0;i<this->n_cells;i++)
        {
            shape->InsertNextCell(VTK_TRIANGLE,3,&this->polys[i*3]);
        }
    }

    if(!shape->GetPointData()->GetTCoords() || shape->GetPointData()->GetTCoords()->GetNumberOfTuples()!=this->N)
    {
        vtkNew<vtkDoubleArray> textureCoords;
	textureCoords->SetNumberOfComponents(2);
	textureCoords->SetNumberOfTuples(this->N);
        for(int i=0;i<this->N;i++)
        {
		textureCoords->SetTuple(i,&this->tcoords[i*2]);
        }
        shape->GetPointData()->SetTCoords(textureCoords);
    }
    
    // b is weighted by the eigenvals
    // make weigth vector for speed reasons
    double *shapevec = NewVector(this->N*3);
    const int bsize = std::min(b->GetNumberOfTuples(),this->Evals->GetNumberOfTuples());
    double *w = NewVector(bsize);
    double eval;
    for (int i = 0; i < bsize; i++) {
        eval = this->Evals->GetValue(i);
        w[i] = sqrt(eval) * b->GetValue(i);
    }

    for (int j = 0; j < this->N*3; j++) {
        shapevec[j] = meanshape[j];
        for (int i = 0; i < bsize; i++) {
            eval = this->Evals->GetValue(i);
            if(fabs(eval)>1E-5) // only use evecs with non-zero evals
                shapevec[j] += w[i] * evecMat2[j][i];
        }
    }
    
    // Copy shape 
    for (int i = 0; i < this->N; i++) {
        shape->GetPoints()->SetPoint(i,shapevec[i*3  ], shapevec[i*3+1], shapevec[i*3+2]);
    }

    DeleteVector(shapevec);
    DeleteVector(w);
}

void vtkSurfacePCA::GetParameterisedLandmarks(vtkPolyData* surface, vtkPolyData* landmarks)
{
   // const int bsize = min(b->GetNumberOfTuples(),this->Evals->GetNumberOfTuples());
	//show landmarks
	if(landmarks->GetNumberOfPoints() != this->Getnlandmarks()) 
	{
        vtkNew<vtkPoints> points;
        points->SetNumberOfPoints(this->Getnlandmarks());
        landmarks->SetPoints(points);
    }

	vtkIdType cellId;
	int subId;
	double dist2,p[3];
	double pcoords[3];
	double weights[3];

	if(!pseudo_landmarks_loaded)
	{
		pseudo_landmark_indexes = new vtkIdType[this->n_landmarks];
	
		vtkSmartPointer<vtkCellLocator> locator = vtkSmartPointer<vtkCellLocator>::New();
		vtkSmartPointer<vtkPolyData> mean_surface = vtkSmartPointer<vtkPolyData>::New();
		vtkDoubleArray *b = vtkDoubleArray::New();
		b->SetNumberOfValues(0);
		this->GetParameterisedShape(b, mean_surface);
		locator->SetDataSet(mean_surface);
		locator->Update();

		vtkSmartPointer<vtkGenericCell> cell = vtkSmartPointer<vtkGenericCell>::New();
 
		for(int i=0;i<this->n_landmarks;i++)
		{
			p[0]=this->mean_landmarks[i*3+0];
			p[1]=this->mean_landmarks[i*3+1];
			p[2]=this->mean_landmarks[i*3+2];
			locator->FindClosestPoint(p,p,cell,cellId,subId,dist2);
			// find the dataset coords for point p
			cell->EvaluatePosition(p,p,subId,pcoords,dist2,weights);
			pseudo_landmark_indexes[i]=cellId;
		}
		pseudo_landmarks_loaded = true;		
	}

	for(int i=0;i<this->n_landmarks;i++)
	{	//cell->EvaluatePosition(p,p,subId,pcoords,dist2,weights);
		surface->GetCell(pseudo_landmark_indexes[i])->EvaluatePosition(p,p,subId,pcoords,dist2,weights);
		landmarks->GetPoints()->SetPoint(i,p[0], p[1], p[2]);
	}

	this->SetCurrentLandmarks(landmarks);
}

void vtkSurfacePCA::GetShapeParameters(vtkPolyData *triangularSubjectMesh, vtkDoubleArray *b, int nmodes, int rigid_body)
{ 
    vtkNew<vtkPoints> mean_points;
    mean_points->SetNumberOfPoints(this->N);

	//#pragma omp parallel for
	for (int i = 0; i < this->N; i++)
	{
		mean_points->SetPoint(i, this->meanshape[i * 3 + 0], this->meanshape[i * 3 + 1], this->meanshape[i * 3 + 2]);
	}
	
    vtkNew<vtkLandmarkTransform> ls;
    ls->SetSourceLandmarks(triangularSubjectMesh->GetPoints());
    ls->SetTargetLandmarks(mean_points);
	if (rigid_body)
	{
		ls->SetModeToRigidBody();
	}
	else
	{
		ls->SetModeToSimilarity();
	}

    vtkNew<vtkTransformFilter> transformSubjectToMeanMesh;
	transformSubjectToMeanMesh->SetTransform(ls);
	transformSubjectToMeanMesh->SetInputData(triangularSubjectMesh);
	transformSubjectToMeanMesh->Update();
     
	if(transformSubjectToMeanMesh->GetOutput()->GetNumberOfPoints() != this->N) {
        vtkErrorMacro_pca(<<"In vtkSurfacePCA::GetShapeParameters: Subject triangle mesh does not have the correct number of points. Comparison: "
			<< "transformSubjectToMeanMesh->GetOutput()->GetNumberOfPoints(): " << transformSubjectToMeanMesh->GetOutput()->GetNumberOfPoints() << " , this->N: " << this->N);
        return;
    }
    
    // Copy shape and subtract mean shape
	double* shapevec = NewVector(this->N * 3);
    for (int i = 0; i < this->N; i++) {
        //double *p = trans->GetOutput()->GetPoint(i); //old
		double p[3];
		transformSubjectToMeanMesh->GetOutput()->GetPoint(i, p);
        shapevec[i*3  ] = p[0] - meanshape[i*3];
        shapevec[i*3+1] = p[1] - meanshape[i*3+1];
        shapevec[i*3+2] = p[2] - meanshape[i*3+2];
    }
    
	// Local variant of b for fast access.
	const auto bsize = static_cast<vtkIdType>(nmodes);
	double* bloc = NewVector(bsize);
    for (int i = 0; i < bsize; i++) 
	{
        bloc[i] = 0;
        
        // Project the shape onto eigenvector i
        for (int j = 0; j < this->N*3; j++) 
		{
            bloc[i] += shapevec[j] * evecMat2[j][i];
        }
    }
  
    // Return b in number of standard deviations
	vtkIdType idt = bsize;
    b->SetNumberOfValues(idt);
    for (int i = 0; i < bsize; i++) 
	{
        if (this->Evals->GetValue(i)) 
		{
            b->SetValue(i, bloc[i]/sqrt(this->Evals->GetValue(i)));
		}
		else
		{
			b->SetValue(i, 0);
		}
	}
    
    DeleteVector(shapevec);
    DeleteVector(bloc);
}

vtkPolyData* vtkSurfacePCA::GetInput(int idx) 
{
    //if(idx<0 || idx>=this->vtkProcessObject::GetNumberOfInputs()) {  CHECK rh-->   rh: use (undocumented) function InputPortIndexInRange(...) instead? 
	if (idx < 0 || idx >= GetNumberOfInputPorts() ) {
        vtkErrorMacro_pca(<<"Index out of bounds in GetInput!");
        return NULL;
    }
    
    // return static_cast<vtkPolyData*>(this->vtkProcessObject::Inputs[idx]); rh CHECK-->
	return static_cast<vtkPolyData*>(GetInputDataObject(idx, 0));
}

void vtkSurfacePCA::PrintSelf(ostream& os, vtkIndent indent)
{
    this->Superclass::PrintSelf(os,indent);
    this->Evals->PrintSelf(os,indent);
}

int vtkSurfacePCA::GetModesRequiredFor(float proportion)
{
    // be aware that we only store some (usually 98%) of the eigenvalues and therefore 
    // any request greater than 98% will only return the total number of modes
    // available. 
	float running_total = 0.0F;
    for(int i=0; i<this->Evals->GetNumberOfTuples(); i++)
    {
        running_total += this->Evals->GetValue(i);
        if(running_total/this->eigen_total>=proportion)
        {
            return i+1;
        }
    }
    return Evals->GetNumberOfTuples();  
}

bool vtkSurfacePCA::SaveToFile(FILE *out,float proportion)
{
    if(!out) return false;    
    // we need to save all the members of this class for later restoration
    
    const int FILE_VERSION=6;
    fprintf(out,"%d\n",FILE_VERSION);
    
    // File format history:
    // 1: Evals, evecMat2, example_landmarks, example_surface, meanshape
    // 2: added eigen_total (need this since we are only storing a % of the modes)
    // 3: scrapped example_landmarks and example_surface, added tcoords and polys
    // 4: added mean_surface_landmarks
    // 5: added mean_landmarks
    // 6: removed mean_surface_landmarks
    
    // save: eigen_total, Evals, evecMat2, tcoords, polys, meanshape, 
    //       mean_landmarks
    
    int j;
    
    // eigen_total
    fprintf(out,"%f\n",this->eigen_total);
    const int N_MODES = this->GetModesRequiredFor(proportion);
   
    // Evals
    fprintf(out,"%d\n",N_MODES);
    for(int i=0;i<N_MODES;i++)
        fprintf(out,"%lf\n",this->Evals->GetValue(i)>=0.0F?this->Evals->GetValue(i):0.0F);
    
    // evecMat2
    fprintf(out,"%d\n%d\n",this->N,N_MODES);
    for(int i=0;i<N_MODES;i++)
    {
        for(j=0;j<this->N;j++)
        {
            fprintf(out,"%lf,%lf,%lf\n",
                this->evecMat2[j*3+0][i],
                this->evecMat2[j*3+1][i],
                this->evecMat2[j*3+2][i]);
        }
    }
    
    // tcoords
    fprintf(out,"%d\n",this->N);
    for(int i=0;i<this->N;i++)
    {
        fprintf(out,"%f,%f\n",
            this->tcoords[i*2+0],
            this->tcoords[i*2+1]);
    }
    
    // polys
    fprintf(out,"%d\n",this->n_cells);
    for(int i=0;i<this->n_cells;i++)
    {
        fprintf(out,"%lld,%lld,%lld\n",
            this->polys[i*3+0],
            this->polys[i*3+1],
            this->polys[i*3+2]);
    }
    
    // meanshape
    fprintf(out,"%d\n",this->N);
    for(int i=0;i<this->N;i++)
    {
        fprintf(out,"%lf,%lf,%lf\n",meanshape[i*3+0],meanshape[i*3+1],meanshape[i*3+2]);
    }
    /*
    // mean_surface_landmarks
    fprintf(out,"%d\n",this->n_landmarks);
    for(int i=0;i<this->n_landmarks;i++)
    {
        fprintf(out,"%f,%f,%f\n",
            this->mean_surface_landmarks[i*3+0],
            this->mean_surface_landmarks[i*3+1],
            this->mean_surface_landmarks[i*3+2]);
    }*/
    
    // mean_landmarks
    fprintf(out,"%d\n",this->n_landmarks);
    for(int i=0;i<this->n_landmarks;i++)
    {
        fprintf(out,"%f,%f,%f\n",
            this->mean_landmarks[i*3+0],
            this->mean_landmarks[i*3+1],
            this->mean_landmarks[i*3+2]);
    }
    delete []mean_landmarks;
    return true;
}

bool vtkSurfacePCA::LoadFile(const std::string model_FileName)
{
	FILE* modelFile = fopen(model_FileName.c_str(), "rt");
	if (!modelFile)
	{
		return false;
	}
	bool ret = LoadFromFile(modelFile);
	fclose(modelFile);
	return ret;
}

bool vtkSurfacePCA::LoadFromFile(FILE *in)
{
    if(!in) 
    {
        vtkErrorMacro_pca(<< "Model file error: Cannot open file.");    
        return false;	
    }
    
    int FILE_VERSION;
    const int numReturnedValues = fscanf(in,"%d",&FILE_VERSION);
    if (numReturnedValues != 1) 
    {
        vtkErrorMacro_pca(<< "Error in model file: Cannot read version information.");
        return false;
    }       
    
    bool successfully_read=false;
    
    switch(FILE_VERSION)
    {
    case 1:
        {
            mfcGUImessage_pca("This is an old format of model file that cannot be read (1).");
            successfully_read=false;
        }
        break;
    case 2:
        {
            mfcGUImessage_pca("This is an old format of model file that cannot be read (2).");
            successfully_read=false;
        }
        break;
    case 3:
        {
            mfcGUImessage_pca("This is an old format of model file that cannot be read (3).");
            successfully_read = false;
        }
        break;
/*
     case 4:
        {
            mfcGUImessage_pca("This is an old format of model file (4). The results may not be ideal.");

            float val;
            vtkIdType j,n;
            
            int ret;
            
            // eigen_total
            ret = fscanf(in,"%f",&this->eigen_total);
            if(ret!=1) { vtkErrorMacro_pca(<< "Error in input file!"); return false;}
       
            // Evals
            fscanf(in,"%lld",&n);
            this->Evals->SetNumberOfValues(n);
            for(int i=0;i<n;i++)
            {
                ret = fscanf(in,"%f",&val);
                if(ret!=1) { vtkErrorMacro_pca(<< "Error in input file!"); return false;}
                this->Evals->SetValue(i,val);
            }
            
            // evecMat2
            fscanf(in,"%d %d",&this->N,&this->S);
            if(this->evecMat2) DeleteMatrix(this->evecMat2);
            this->evecMat2 = NewMatrix(3*this->N, this->S);
            for(int i=0;i<this->S;i++)
            {
                for(j=0;j<this->N;j++)
                {
                    ret = fscanf(in,"%lf,%lf,%lf\n",
                        &this->evecMat2[j*3+0][i],
                        &this->evecMat2[j*3+1][i],
                        &this->evecMat2[j*3+2][i]);
                    if(ret!=3) 
                        { vtkErrorMacro_pca(<< "Error in input file!"); return false;}
                }
            }
            
            // tcoords
            fscanf(in,"%lld\n",&n);
            if(tcoords) delete []tcoords;
            tcoords = new double[2*n];
			float f1,f2;
            for(int i=0;i<n;i++)
            {
               
                fscanf(in,"%f,%f\n",
					&f1,&f2);
				this->tcoords[i*2+0] = f1;
				this->tcoords[i*2+1] = f2;
            }
            
            // polys
            fscanf(in,"%d\n",&this->n_cells);
            if(polys) delete []polys;
            polys = new vtkIdType[3*n_cells];
            for(int i=0;i<this->n_cells;i++)
            {
                fscanf(in,"%lld,%lld,%lld\n",
                    &this->polys[i*3+0],
                    &this->polys[i*3+1],
                    &this->polys[i*3+2]);
            }
            
            // meanshape
            fscanf(in,"%lld\n",&n);
            if(this->meanshape) DeleteVector(this->meanshape);
            this->meanshape = NewVector(3*n);
            for(int i=0;i<n;i++)
            {
                ret = fscanf(in,"%lf,%lf,%lf\n",
                    &this->meanshape[i*3+0],
                    &this->meanshape[i*3+1],
                    &this->meanshape[i*3+2]);
                if(ret!=3) { vtkErrorMacro_pca(<< "Error in input file!"); return false;}
            }
            
            // mean_landmarks
            fscanf(in,"%d\n",&this->n_landmarks);
            //if(this->mean_surface_landmarks) delete []this->mean_surface_landmarks;
            //this->mean_surface_landmarks = new float[this->n_landmarks*3];
            if(this->mean_landmarks) delete []this->mean_landmarks;
            this->mean_landmarks = new float[this->n_landmarks*3];
            for(int i=0;i<this->n_landmarks;i++)
            {
                ret = fscanf(in,"%f,%f,%f\n",
                    &this->mean_landmarks[i*3+0],
                    &this->mean_landmarks[i*3+1],
                    &this->mean_landmarks[i*3+2]);
                if(ret!=3) { vtkErrorMacro_pca(<< "Error in input file!"); return false;}
            }

            // for(int i=0;i<this->n_landmarks;i++)
            // {
            //    this->mean_landmarks[i*3+0] = this->mean_surface_landmarks[i*3+0];
            //    this->mean_landmarks[i*3+1] = this->mean_surface_landmarks[i*3+1];
            //    this->mean_landmarks[i*3+2] = this->mean_surface_landmarks[i*3+2];
            // }
           
            successfully_read=true;
        }
        break;
    case 5:
        {
            float val;
            vtkIdType j,n;
            
            int ret;
            
            // eigen_total
            ret = fscanf(in,"%f",&this->eigen_total);
            if(ret!=1) { vtkErrorMacro_pca(<< "Error in input file!"); return false;}
            
            
            // Evals
            fscanf(in,"%lld",&n);
            this->Evals->SetNumberOfValues(n);
            for(int i=0;i<n;i++)
            {
                ret = fscanf(in,"%f",&val);
                if(ret!=1) { vtkErrorMacro_pca(<< "Error in input file!"); return false;}
                this->Evals->SetValue(i,val);
            }
            
            // evecMat2
            fscanf(in,"%d %d",&this->N,&this->S);
            if(this->evecMat2) DeleteMatrix(this->evecMat2);
            this->evecMat2 = NewMatrix(3*this->N, this->S);
            for(int i=0;i<this->S;i++)
            {
                for(j=0;j<this->N;j++)
                {
                    ret = fscanf(in,"%lf,%lf,%lf\n",
                        &this->evecMat2[j*3+0][i],
                        &this->evecMat2[j*3+1][i],
                        &this->evecMat2[j*3+2][i]);
                    if(ret!=3) 
                        { vtkErrorMacro_pca(<< "Error in input file!"); return false;}
                }
            }
            
            // tcoords
            fscanf(in,"%lld\n",&n);
            if(tcoords) delete []tcoords;
            tcoords = new double[2*n];
			//vtkFloatArray *fa = vtkFloatArray::New();
			
			float f1,f2;
            for(int i=0;i<n;i++)
            {
                
                fscanf(in,"%f,%f\n",
					&f1,&f2);
				this->tcoords[i*2+0] = f1;
				this->tcoords[i*2+1] = f2;
            }
            
            // polys
            fscanf(in,"%d\n",&this->n_cells);
            if(polys) delete []polys;
            polys = new vtkIdType[3*n_cells];
            for(int i=0;i<this->n_cells;i++)
            {
                fscanf(in,"%lld,%lld,%lld\n",
                    &this->polys[i*3+0],
                    &this->polys[i*3+1],
                    &this->polys[i*3+2]);
            }
            
            // meanshape
            fscanf(in,"%lld\n",&n);
            if(this->meanshape) DeleteVector(this->meanshape);
            this->meanshape = NewVector(3*n);
            for(int i=0;i<n;i++)
            {
                ret = fscanf(in,"%lf,%lf,%lf\n",
                    &this->meanshape[i*3+0],
                    &this->meanshape[i*3+1],
                    &this->meanshape[i*3+2]);
                if(ret!=3) { vtkErrorMacro_pca(<< "Error in input file!"); return false;}
            }
            
            // mean_surface_landmarks (same now as mean_landmarks)
            fscanf(in,"%d\n",&this->n_landmarks);
            if(this->mean_landmarks) delete []this->mean_landmarks;
            this->mean_landmarks = new float[this->n_landmarks*3];
			
		
            for(int i=0;i<this->n_landmarks;i++)
            {
                ret = fscanf(in,"%f,%f,%f\n",
                    &this->mean_landmarks[i*3+0],
                    &this->mean_landmarks[i*3+1],
                    &this->mean_landmarks[i*3+2]);
                if(ret!=3) { vtkErrorMacro_pca(<< "Error in input file!"); return false;}
            }
            
            // mean_landmarks
            fscanf(in,"%d\n",&this->n_landmarks);
            if(this->mean_landmarks) delete []this->mean_landmarks;
            this->mean_landmarks = new float[this->n_landmarks*3];
            for(int i=0;i<this->n_landmarks;i++)
            {
                ret = fscanf(in,"%f,%f,%f\n",
                    &this->mean_landmarks[i*3+0],
                    &this->mean_landmarks[i*3+1],
                    &this->mean_landmarks[i*3+2]);
                if(ret!=3) { vtkErrorMacro_pca(<< "Error in input file!"); return false;}
            }
            
            successfully_read=true;
        }
        break;
*/
    case 6:
        {
            float val(0.0F);
            int n(0); // rh: was vtkIdType, causing wrong value read in fscanf_s            
            int ret(0);

            ret = fscanf(in, "%f", &this->eigen_total);
            if(ret != 1) 
            {
            	vtkErrorMacro_pca(<< "Error in model file: Cannot read eigen total.");
            	return false;
            }
            
            // Evals
            ret = fscanf(in, "%d", &n);
            if (ret != 1) 
            {
            	vtkErrorMacro_pca(<< "Error in model file: Cannot read number of eigenvalues.");
            	return false;
            }            	
	 
            this->Evals->SetNumberOfValues(n);
            if(n < 1) 
            { 
            	vtkErrorMacro_pca(<< "Error in model file: Number of Eigen values < 1."); 
            	return false;
            }
            for(int i=0; i < n; i++)
            {
                ret = fscanf(in, "%f", &val);
                if(ret!=1) 
                { 
                	vtkErrorMacro_pca(<< "Error in model file: One of the Eigenvalues could not be read."); 
                	return false;
                }
                this->Evals->SetValue(i,val);
            }
            
            // evecMat2
            ret = fscanf(in,"%d %d", &this->N, &this->S);
            if (ret != 2) 
            {
            	vtkErrorMacro_pca(<< "Error in model file: Cannot read Eigenvector matrix dimensions.");
            	return false;
            }             
            if(this->evecMat2) DeleteMatrix(this->evecMat2);
            this->evecMat2 = NewMatrix(3*this->N, this->S);
            for(int i=0;i<this->S;i++)
            {
                for(int j=0;j<this->N;j++)
                {
                    ret = fscanf(in,"%lf,%lf,%lf\n",
                        &this->evecMat2[j*3+0][i],
                        &this->evecMat2[j*3+1][i],
                        &this->evecMat2[j*3+2][i]);
                    if(ret!=3) 
                    { 
                    	vtkErrorMacro_pca(<< "Error in model file: A 3D component of the Eigenvector matrix could not be read (Index[S,N]: [" << i << ", " << j << "] ) ."); 
                    	return false;
                    }
                }
            }
            
            // tcoords
            ret = fscanf(in,"%d\n",&n);
            if (ret != 1) 
            {
            	vtkErrorMacro_pca(<< "Error in model file: Cannot read number of texture coordinates.");
            	return false;
            }             
            if(tcoords) delete []tcoords;
            tcoords = new double[2*n];
            float textureCoordX(0.0F), textureCoordY(0.0F);
            for(int i=0; i<n; i++)
            {
                ret = fscanf(in,"%f,%f\n", &textureCoordX, &textureCoordY);
                if(ret!=2) 
                { 
                    vtkErrorMacro_pca(<< "Error in model file: A 2D texture coordinate could not be read."); 
                    return false;
                }                
                this->tcoords[i*2+0] = textureCoordX;
                this->tcoords[i*2+1] = textureCoordY;
            }
            
            // polys
            ret = fscanf(in,"%d\n",&this->n_cells);
            if (ret != 1) 
            {
            	vtkErrorMacro_pca(<< "Error in model file: Cannot read number of polygons/mesh faces.");
            	return false;
            }             
            if(polys) delete []polys;
            polys = new vtkIdType[3*n_cells];
            for(int i=0;i<this->n_cells;i++)
            {
				// rh: Point indices. Warning: Reading with fscanf directly into vtkIdType is unreliable!
				int v1(0), v2(0), v3(0); 
				ret = fscanf(in, "%d,%d,%d\n", &v1, &v2, &v3);
                               if (ret != 3) 
                               {
                                   vtkErrorMacro_pca(<< "Error in model file: Cannot read a line containing 3 vertex indices for a polygon.");
                                   return false;
                               } 				
				this->polys[i * 3 + 0] = v1;
				this->polys[i * 3 + 1] = v2;
				this->polys[i * 3 + 2] = v3;
            }
            
            // meanshape
	    // char* liner = new char[1000];
            // fgets(liner, 1000, in);
            ret = fscanf(in,"%d\n",&n);
            if (ret != 1) 
            {
            	vtkErrorMacro_pca(<< "Error in model file: Cannot read number of meanshape vector elements.");
            	return false;
            }                         
            if(this->meanshape) DeleteVector(this->meanshape);
            this->meanshape = NewVector(3*n);
            for(int i=0;i<n;i++)
            {
                ret = fscanf(in,"%lf,%lf,%lf\n",
                    &this->meanshape[i*3+0],
                    &this->meanshape[i*3+1],
                    &this->meanshape[i*3+2]);
                if (ret != 3) 
                { 
                    vtkErrorMacro_pca(<< "Error in model file: Cannot read one of the meanshape vector components."); 
                    return false;
                }
            }
            
            // mean_landmarks
            ret = fscanf(in,"%d\n",&this->n_landmarks);
            if (ret != 1) 
            {
            	vtkErrorMacro_pca(<< "Error in model file: Cannot read number of mean landmarks.");
            	return false;
            }                                     
            if(this->mean_landmarks) delete []this->mean_landmarks;
            this->mean_landmarks = new float[this->n_landmarks*3];
            for(int i=0;i<this->n_landmarks;i++)
            {
                ret = fscanf(in,"%f,%f,%f\n",
                    &this->mean_landmarks[i*3+0],
                    &this->mean_landmarks[i*3+1],
                    &this->mean_landmarks[i*3+2]);
                if(ret!=3) 
                { 
                    vtkErrorMacro_pca(<< "Error in model file: Cannot read one of the mean landmarks."); 
                    return false;
                }
            }
            
            successfully_read=true;
        }
        break;
   default:
        {
            vtkErrorMacro_pca(<< "Unknown model file format: " << FILE_VERSION);
            successfully_read=false;
        }
        break;
    }
    
    return successfully_read;
}

void vtkSurfacePCA::ApplyResampleSurfaceFilter(vtkSmartPointer<vtkPolyData> in, vtkSmartPointer<vtkPolyData> outputMesh,
	vtkSmartPointer<vtkPolyData> mean_surface, vtkSmartPointer<vtkThinPlateSplineTransform> tps)
{
	const int N_BASE_MESH_POINTS = mean_surface->GetNumberOfPoints();
	vtkNew<vtkFloatArray> distances;
	distances->SetNumberOfValues(N_BASE_MESH_POINTS);

	//vtkNew<vtkPolyData> outputMesh;
	outputMesh->DeepCopy(mean_surface);
	outputMesh->GetPointData()->SetScalars(distances);

	// -- warp the source mesh to the target mesh -- 
	vtkNew<vtkPolyData> copy;
	copy->ShallowCopy(in);

	vtkNew<vtkTransformPolyDataFilter> trans;
	trans->SetInputData(copy);
	trans->SetTransform(tps);
	trans->Update(); // (else locator thinks it has no input)
	// -- resample the warped mesh using the target mesh and then unwarp --

	// create a locator to help us find the closest point on the warped surface
	vtkNew<vtkCellLocator> locator;
	locator->SetDataSet(trans->GetOutput());
	locator->Update(); // else FindClosestPoint etc. don't work

	typedef double vtkFloatingPointType;
	// for each point in the target mesh:
	vtkFloatingPointType p[3], p2[3];
	vtkIdType id;
	int subid;
	vtkFloatingPointType dist2;
	vtkFloatingPointType pcoords[3];
	vtkFloatingPointType interpolationWeights[3];
	vtkNew<vtkGenericCell> cell;  // to avoid repeated allocation within FindClosestPoint inside loop below

	// #pragma omp parallel for
	for (int i = 0; i < N_BASE_MESH_POINTS; i++)
	{
		// -- resample the warped mesh using the target mesh --
		// retrieve the location of the point
		//p = this->GetOutput()->GetPoint(i);..// old !! dont use double pointers
		outputMesh->GetPoint(i, p);		// use this instead (double[])

		vtkFloatingPointType closestPoint[3];
		// find the closest point in the warped input mesh, assign it
		locator->FindClosestPoint(p, closestPoint, cell, id, subid, dist2);
		// store the squared distance as a scalar for this point
		distances->SetValue(i, dist2);

		// -- unwarp the point using global->dataset->global coordinates --
		cell->EvaluatePosition(closestPoint, p2, subid, pcoords, dist2, interpolationWeights); // (p2 should emerge = p)
		// find the global coords for the same cell on the original mesh
		//this->GetInput()->GetCell(id)->EvaluateLocation(subid, pcoords, p, weights); //rh: old
		in->GetCell(id)->EvaluateLocation(subid, pcoords, closestPoint, interpolationWeights);
		// rh: some trouble with line above here (due to OMP?: "in GOMP_parallel ()" ) :
		// ... in vtkPolyData::GetCell (this=0x7fff84005700, cellId=154136)
		//    at /home/rr/fasd/VTK/Common/DataModel/vtkPolyData.cxx:158
		// 158	  const TaggedCellId tag = this->Cells->GetTag(cellId);


		outputMesh->GetPoints()->SetPoint(i, closestPoint);
	}
}

//----------------------------------------------------------------------------
// public
// Transforms the subject mesh into triangular mesh with same number of points N as model.
void vtkSurfacePCA::Resample(vtkPolyData *in, vtkPointSet *landmarks, vtkPolyData *out)
{
    if(!this->mean_landmarks)
    {
        vtkErrorMacro_pca(<<"mean_landmarks not supplied!");
        return;
    }
    
    // get the mean surface 
	vtkNew<vtkPolyData> mean_surface;
    this->InitialiseParameterisedShape(mean_surface); 
 
	// ... and its landmarks
	vtkNew<vtkPoints> mean_landmark_points;
    {
        mean_landmark_points->SetNumberOfPoints(this->n_landmarks);
		//#pragma omp parallel for
        for(int i=0;i<this->n_landmarks;i++)
            mean_landmark_points->SetPoint(i,
                this->mean_landmarks[i*3+0],
                this->mean_landmarks[i*3+1],
                this->mean_landmarks[i*3+2]);
    }

    // now warp the input surface onto the mean_landmarks 
    vtkSmartPointer<vtkThinPlateSplineTransform> tps = vtkThinPlateSplineTransform::New();
    tps->SetSourceLandmarks(landmarks->GetPoints());
    tps->SetTargetLandmarks(mean_landmark_points);
    tps->SetBasisToR(); // since we're in 3D
    tps->Update();

	ApplyResampleSurfaceFilter(in, out, mean_surface, tps);
 }
 
void vtkSurfacePCA::GetApproximateShapeParameters(vtkPolyData *subjectMesh, vtkPointSet *landmarks,
                                                  vtkDoubleArray *b,int rigid_body)
{
	// certain bits of the resampling rely on the surface consisting of only triangles
	// tjh added Jan 2006
	vtkNew<vtkTriangleFilter> tri;
	tri->SetInputData(subjectMesh);
	tri->Update();

    // resample the supplied surface using the base mesh
    vtkNew<vtkPolyData> triangularSubjectMesh;
    Resample(tri->GetOutput(), landmarks, triangularSubjectMesh);
    
    // find the parameters that best model the resampled surface
	GetApproximateShapeParametersFromResampledSurface(triangularSubjectMesh, b, rigid_body);
}

void vtkSurfacePCA::GetApproximateShapeParametersFromResampledSurface(vtkPolyData *triangularSubjectMesh,
                                                  vtkDoubleArray *b,int rigid_body)
{
    const vtkIdType nmodes = this->GetTotalNumModes();
    b->SetNumberOfValues(nmodes);
    this->GetShapeParameters(triangularSubjectMesh, b, nmodes, rigid_body);
}

//----------------------------------------------------------------------------
// public
void vtkSurfacePCA::GetModesForInput(int i,vtkDoubleArray *b,int b_size)
{
    if(this->modes_for_inputs)
    {
        for(int j=0;j<b_size;j++)
        {
            b->SetValue(j,this->modes_for_inputs[i]->GetValue(j));
        }
    }
    else {
        vtkErrorMacro_pca(<<"modes_for_input not computed!");
    }
}

//----------------------------------------------------------------------------
// public
void vtkSurfacePCA::SetCurrentLandmarks(vtkPolyData *landmarks)
{
   	if(this->current_landmarks) 		delete []this->current_landmarks;
    const int N = landmarks->GetNumberOfPoints();
	this->current_landmarks = new float[landmarks->GetNumberOfPoints()*3];
   
    for(int i=0;i<N;i++)
    {
          this->current_landmarks[i*3+0] = landmarks->GetPoint(i)[0];
          this->current_landmarks[i*3+1] = landmarks->GetPoint(i)[1];
          this->current_landmarks[i*3+2] = landmarks->GetPoint(i)[2];
    }		 
}

void vtkSurfacePCA::GetCurrentLandmarks(vtkPolyData *landmarks)
{
    landmarks->Allocate(this->n_landmarks);
    landmarks->GetPoints()->SetNumberOfPoints(this->n_landmarks);
    for(int i=0;i<this->n_landmarks;i++)
        landmarks->GetPoints()->SetPoint(i,this->current_landmarks[i*3+0],
            this->current_landmarks[i*3+1],
            this->current_landmarks[i*3+2]);
}

void vtkSurfacePCA::GetMeanLandmarks(vtkPolyData *landmarks)
{
    landmarks->Allocate(this->n_landmarks);
    landmarks->GetPoints()->SetNumberOfPoints(this->n_landmarks);
    for(int i=0;i<this->n_landmarks;i++)
        landmarks->GetPoints()->SetPoint(i,this->mean_landmarks[i*3+0],
            this->mean_landmarks[i*3+1],
            this->mean_landmarks[i*3+2]);
}
