#include "crlTensorGlyph.h"

#include "vtkCell.h"
#include "vtkCellArray.h"
#include "vtkDataSet.h"
#include "vtkExecutive.h"
#include "vtkFloatArray.h"
#include "vtkMath.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkTransform.h"

vtkCxxRevisionMacro(crlTensorGlyph, "$Revision: 1.0 $");
vtkStandardNewMacro(crlTensorGlyph);

crlTensorGlyph::crlTensorGlyph()
{
  this->Scaling = 1;
  this->ScaleFactor = 1.0;
  this->ExtractEigenvalues = 1;
  this->ColorGlyphs = 1;
  this->ColorMode = COLOR_BY_SCALARS;
  this->ClampScaling = 0;
  this->MaxScaleFactor = 100;
  this->ThreeGlyphs = 0;
  this->Symmetric = 0;
  this->Length = 1.0;

  this->SetNumberOfInputPorts(2);
}

crlTensorGlyph::~crlTensorGlyph()
{
}

int crlTensorGlyph::RequestData(
  vtkInformation *vtkNotUsed(request),
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  // get the info objects
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation *sourceInfo = inputVector[1]->GetInformationObject(0);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  // get the input and ouptut
  vtkDataSet *input = vtkDataSet::SafeDownCast(
    inInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkPolyData *source = vtkPolyData::SafeDownCast(
    sourceInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkPolyData *output = vtkPolyData::SafeDownCast(
    outInfo->Get(vtkDataObject::DATA_OBJECT()));

  vtkDataArray *inTensors;
  double tensor[9];
  vtkDataArray *inScalars;
  vtkIdType numPts, numSourcePts, numSourceCells, inPtId, i;
  int j;
  vtkPoints *sourcePts;
  vtkDataArray *sourceNormals;
  vtkCellArray *sourceCells, *cells;
  vtkPoints *newPts;
  // newScalars is a vtkFloatArray in the original vtkTensorGlyph, but vtkPolyDataMapper does not accept
  // float scalars as colors, only unsigned char
  vtkUnsignedCharArray *newScalars=NULL;
  vtkFloatArray *newNormals=NULL;
  double x[3], s, s2[4];
  vtkTransform *trans;
  vtkCell *cell;
  vtkIdList *cellPts;
  int npts;
  vtkIdType *pts;
  vtkIdType ptIncr, cellId;
  vtkIdType subIncr;
  int numDirs, dir, eigen_dir, symmetric_dir;
  vtkMatrix4x4 *matrix;
  double *m[3], w[3], *v[3];
  double m0[3], m1[3], m2[3];
  double v0[3], v1[3], v2[3];
  double xv[3], yv[3], zv[3];
  double maxScale;
  vtkPointData *pd, *outPD;

  numDirs = (this->ThreeGlyphs?3:1)*(this->Symmetric+1);

  pts = new vtkIdType[source->GetMaxCellSize()];
  trans = vtkTransform::New();
  matrix = vtkMatrix4x4::New();

  // set up working matrices
  m[0] = m0; m[1] = m1; m[2] = m2;
  v[0] = v0; v[1] = v1; v[2] = v2;

  vtkDebugMacro(<<"Generating tensor glyphs");

  pd = input->GetPointData();
  outPD = output->GetPointData();
  inTensors = pd->GetTensors();
  inScalars = pd->GetScalars();
  numPts = input->GetNumberOfPoints();

  if ( !inTensors || numPts < 1 )
    {
    vtkErrorMacro(<<"No data to glyph!");
    return 1;
    }
  //
  // Allocate storage for output PolyData
  //
  sourcePts = source->GetPoints();
  numSourcePts = sourcePts->GetNumberOfPoints();
  numSourceCells = source->GetNumberOfCells();

  newPts = vtkPoints::New();
  newPts->Allocate(numDirs*numPts*numSourcePts);

  // Setting up for calls to PolyData::InsertNextCell()
  if ( (sourceCells=source->GetVerts())->GetNumberOfCells() > 0 )
    {
    cells = vtkCellArray::New();
    cells->Allocate(numDirs*numPts*sourceCells->GetSize());
    output->SetVerts(cells);
    cells->Delete();
    }
  if ( (sourceCells=this->GetSource()->GetLines())->GetNumberOfCells() > 0 )
    {
    cells = vtkCellArray::New();
    cells->Allocate(numDirs*numPts*sourceCells->GetSize());
    output->SetLines(cells);
    cells->Delete();
    }
  if ( (sourceCells=this->GetSource()->GetPolys())->GetNumberOfCells() > 0 )
    {
    cells = vtkCellArray::New();
    cells->Allocate(numDirs*numPts*sourceCells->GetSize());
    output->SetPolys(cells);
    cells->Delete();
    }
  if ( (sourceCells=this->GetSource()->GetStrips())->GetNumberOfCells() > 0 )
    {
    cells = vtkCellArray::New();
    cells->Allocate(numDirs*numPts*sourceCells->GetSize());
    output->SetStrips(cells);
    cells->Delete();
    }

  // only copy scalar data through
  pd = this->GetSource()->GetPointData();
  // generate scalars if eigenvalues are chosen or if scalars exist.
  if (this->ColorGlyphs &&
      ((this->ColorMode == COLOR_BY_EIGENVALUES) ||
       (inScalars && (this->ColorMode == COLOR_BY_SCALARS)) ) )
    {
    newScalars = vtkUnsignedCharArray::New();
    newScalars->SetNumberOfComponents(4);
    newScalars->Allocate(numDirs*numPts*numSourcePts*4);
    }
  else
    {
    outPD->CopyAllOff();
    outPD->CopyScalarsOn();
    outPD->CopyAllocate(pd,numDirs*numPts*numSourcePts);
    }
  if ( (sourceNormals = pd->GetNormals()) )
    {
    newNormals = vtkFloatArray::New();
    newNormals->SetNumberOfComponents(3);
    newNormals->Allocate(numDirs*3*numPts*numSourcePts);
    }
  //
  // First copy all topology (transformation independent)
  //
  for (inPtId=0; inPtId < numPts; inPtId++)
    {
    ptIncr = numDirs * inPtId * numSourcePts;
    for (cellId=0; cellId < numSourceCells; cellId++)
      {
      cell = this->GetSource()->GetCell(cellId);
      cellPts = cell->GetPointIds();
      npts = cellPts->GetNumberOfIds();
      for (dir=0; dir < numDirs; dir++)
        {
        // This variable may be removed, but that
        // will not improve readability
        subIncr = ptIncr + dir*numSourcePts;
        for (i=0; i < npts; i++)
          {
          pts[i] = cellPts->GetId(i) + subIncr;
          }
        output->InsertNextCell(cell->GetCellType(),npts,pts);
        }
      }
    }
  //
  // Traverse all Input points, transforming glyph at Source points
  //
  trans->PreMultiply();

  for (inPtId=0; inPtId < numPts; inPtId++)
    {
    ptIncr = numDirs * inPtId * numSourcePts;

    // Translation is postponed

    inTensors->GetTuple(inPtId, tensor);

    // compute orientation vectors and scale factors from tensor
    if ( this->ExtractEigenvalues ) // extract appropriate eigenfunctions
      {
      for (j=0; j<3; j++)
        {
        for (i=0; i<3; i++)
          {
          m[i][j] = tensor[i+3*j];
          }
        }
      vtkMath::Jacobi(m, w, v);

      //copy eigenvectors
      xv[0] = v[0][0]; xv[1] = v[1][0]; xv[2] = v[2][0];
      yv[0] = v[0][1]; yv[1] = v[1][1]; yv[2] = v[2][1];
      zv[0] = v[0][2]; zv[1] = v[1][2]; zv[2] = v[2][2];
      }
    else //use tensor columns as eigenvectors
      {
      for (i=0; i<3; i++)
        {
        xv[i] = tensor[i];
        yv[i] = tensor[i+3];
        zv[i] = tensor[i+6];
        }
      w[0] = vtkMath::Normalize(xv);
      w[1] = vtkMath::Normalize(yv);
      w[2] = vtkMath::Normalize(zv);
      }

    // compute scale factors
    w[0] *= this->ScaleFactor;
    w[1] *= this->ScaleFactor;
    w[2] *= this->ScaleFactor;

    if ( this->ClampScaling )
      {
      for (maxScale=0.0, i=0; i<3; i++)
        {
        if ( maxScale < fabs(w[i]) )
          {
          maxScale = fabs(w[i]);
          }
        }
      if ( maxScale > this->MaxScaleFactor )
        {
        maxScale = this->MaxScaleFactor / maxScale;
        for (i=0; i<3; i++)
          {
          w[i] *= maxScale; //preserve overall shape of glyph
          }
        }
      }

    // normalization is postponed

    // make sure scale is okay (non-zero) and scale data
    for (maxScale=0.0, i=0; i<3; i++)
      {
      if ( w[i] > maxScale )
        {
        maxScale = w[i];
        }
      }
    if ( maxScale == 0.0 )
      {
      maxScale = 1.0;
      }
    for (i=0; i<3; i++)
      {
      if ( w[i] == 0.0 )
        {
        w[i] = maxScale * 1.0e-06;
        }
      }

    // Now do the real work for each "direction"

    for (dir=0; dir < numDirs; dir++)
      {
      eigen_dir = dir%(this->ThreeGlyphs?3:1);
      symmetric_dir = dir/(this->ThreeGlyphs?3:1);

      // Remove previous scales ...
      trans->Identity();

      // translate Source to Input point
      input->GetPoint(inPtId, x);
      trans->Translate(x[0], x[1], x[2]);

      // normalized eigenvectors rotate object for eigen direction 0
      matrix->Element[0][0] = xv[0];
      matrix->Element[0][1] = yv[0];
      matrix->Element[0][2] = zv[0];
      matrix->Element[1][0] = xv[1];
      matrix->Element[1][1] = yv[1];
      matrix->Element[1][2] = zv[1];
      matrix->Element[2][0] = xv[2];
      matrix->Element[2][1] = yv[2];
      matrix->Element[2][2] = zv[2];
      trans->Concatenate(matrix);

      if (eigen_dir == 1)
        {
        trans->RotateZ(90.0);
        }

      if (eigen_dir == 2)
        {
        trans->RotateY(-90.0);
        }

      if (this->ThreeGlyphs)
        {
        trans->Scale(w[eigen_dir], this->ScaleFactor, this->ScaleFactor);
        }
      else
        {
        trans->Scale(w[0], w[1], w[2]);
        }

      // Mirror second set to the symmetric position
      if (symmetric_dir == 1)
        {
        trans->Scale(-1.,1.,1.);
        }

      // if the eigenvalue is negative, shift to reverse direction.
      // The && is there to ensure that we do not change the
      // old behaviour of vtkTensorGlyphs (which only used one dir),
      // in case there is an oriented glyph, e.g. an arrow.
      if (w[eigen_dir] < 0 && numDirs > 1)
        {
        trans->Translate(-this->Length, 0., 0.);
        }

      // multiply points (and normals if available) by resulting
      // matrix
      trans->TransformPoints(sourcePts,newPts);

      // Apply the transformation to a series of points,
      // and append the results to outPts.
      if ( newNormals )
        {
        trans->TransformNormals(sourceNormals,newNormals);
        }

        // Copy point data from source
      if ( this->ColorGlyphs && inScalars &&
           (this->ColorMode == COLOR_BY_SCALARS) )
        {
	  //s = inScalars->GetComponent(inPtId, 0);
        s2[0] = inScalars->GetComponent(inPtId, 0);
        s2[1] = inScalars->GetComponent(inPtId, 1);
        s2[2] = inScalars->GetComponent(inPtId, 2);
	s2[3] = inScalars->GetComponent(inPtId, 3);
        for (i=0; i < numSourcePts; i++)
          {
	    newScalars->InsertTuple(ptIncr+i, s2);
          }
        }
      else if (this->ColorGlyphs &&
               (this->ColorMode == COLOR_BY_EIGENVALUES) )
        {
        // If ThreeGlyphs is false we use the first (largest)
        // eigenvalue as scalar.
        s = w[eigen_dir];
        for (i=0; i < numSourcePts; i++)
          {
          newScalars->InsertTuple(ptIncr+i, &s);
          }
        }
      else
        {
        for (i=0; i < numSourcePts; i++)
          {
          outPD->CopyData(pd,i,ptIncr+i);
          }
        }
      ptIncr += numSourcePts;
      }
    }
  vtkDebugMacro(<<"Generated " << numPts <<" tensor glyphs");
  //
  // Update output and release memory
  //
  delete [] pts;

  output->SetPoints(newPts);
  newPts->Delete();

  if ( newScalars )
    {
      //int idx = outPD->AddArray(newScalars);
      //outPD->SetActiveAttribute(idx, vtkDataSetAttributes::SCALARS);
      outPD->SetScalars(newScalars);
    newScalars->Delete();
    }

  if ( newNormals )
    {
    outPD->SetNormals(newNormals);
    newNormals->Delete();
    }

  output->Squeeze();
  trans->Delete();
  matrix->Delete();

  return 1;
}
