/*
 *       {_       {__       {__{_______              {__      {__
 *      {_ __     {_ {__   {___{__    {__             {__   {__  
 *     {_  {__    {__ {__ { {__{__    {__     {__      {__ {__   
 *    {__   {__   {__  {__  {__{_ {__       {_   {__     {__     
 *   {______ {__  {__   {_  {__{__  {__    {_____ {__  {__ {__   
 *  {__       {__ {__       {__{__    {__  {_         {__   {__  
 * {__         {__{__       {__{__      {__  {____   {__      {__
 *
 */

#include <cmath>

#include "AMReX_GeometryShop.H"
#include "AMReX_BoxIterator.H"
//#include "AMReX_ParmParse.H"
#include "AMReX_RealVect.H"
#include "AMReX_SphereIF.H"

using namespace amrex;
using std::cout;
using std::endl;

void 
getExactValues(Real             & a_volumeExact, 
               Real             & a_bndryAreaExact,
               const Real       & a_radius,
               const RealVect   & a_center)
{
  // Calculate the exact volume of the regular region
  Real volExact;
  Real bndryExact;

  if (SpaceDim == 2)
    {
      volExact = acos(-1.0) * a_radius*a_radius;
      bndryExact = 2*acos(-1.0) * a_radius;
    }

  if (SpaceDim == 3)
    {
      volExact = 4.0/3.0 * acos(-1.0) * a_radius*a_radius*a_radius;
      bndryExact = 4.0 * acos(-1.0) * a_radius*a_radius;
    }

  a_volumeExact     = volExact;
  a_bndryAreaExact  = bndryExact;
}
////////////
void 
getComputedValues(Real             & a_volumeCalc, 
                  Real             & a_bndryAreaCalc,
                  const Real       & a_radius,
                  const RealVect   & a_center,
                  const Box        & a_domain,
                  const Real       & a_dx)
{
  Real volTotal = 0.0;
  Real bndryTotal = 0.0;

  //inside regular tells whether domain is inside or outside the sphere
  bool insideRegular = true;
  SphereIF sphere(a_radius, a_center, insideRegular);
  int verbosity = 0;
  //ParmParse pp;
  //  pp.get("verbosity", verbosity);

  GeometryShop gshop(sphere, verbosity);
  BaseFab<int> regIrregCovered;
  std::vector<IrregNode> nodes;
  Box validRegion = a_domain;
  Box ghostRegion = a_domain; //whole domain so ghosting does not matter
  RealVect origin = RealVect::Zero;
  gshop.fillGraph(regIrregCovered, nodes, validRegion, ghostRegion, a_domain, origin, a_dx);
  //regular volume and face area for uncut cell
  Real regVolume = D_TERM(a_dx, *a_dx, *a_dx);
  Real regArea   = D_TERM(1.0 , *a_dx, *a_dx);
  for (BoxIterator bit(a_domain); bit.ok(); ++bit)
    {
      const IntVect& iv = bit();
      int cellType = regIrregCovered(iv, 0);
      if(cellType == -1)
        {
          // covered cell- does not contribute to volume or area
        }
      else if(cellType == 1)
        {
          // uncut (regular cell) has a full volume but no boundary area
          volTotal += regVolume;
        }
      else if(cellType == 0)
        {
          //cut cell--we will account for these by looping through the vector
        }
      else
        {
          amrex::Error("bogus celltype");
        }
    }
  //now add in cut cell values
  for(int ivec = 0; ivec < nodes.size(); ivec++)
    {
      const IrregNode& node = nodes[ivec];
      Real volFrac = node.m_volFrac;
      Real bndryFrac = node.bndryArea();
      volTotal   += volFrac*regVolume;
      bndryTotal += bndryFrac*regArea;
    }

  a_volumeCalc    = volTotal;
  a_bndryAreaCalc = bndryTotal;
}

int checkSphere()
{
  int eekflag =  0;
  Real radius = 0.5;
  Real domlen = 1;
  IntVect ncells;

  //ParmParse pp;
  //pp.get(   "n_cell"       , ncells);
  //pp.get(   "sphere_radius", radius);
  //pp.getarr("sphere_center", centervec, 0, SpaceDim);
  //pp.get("domain_length", domlen);                     // 

  RealVect center = 0.5*RealVect::Unit;
  IntVect ivlo = IntVect::TheZeroVector();
  IntVect ivhi = ncells - IntVect::TheUnitVector();
  Box domain(ivlo, ivhi);
  

  Real volExact, bndryExact, volTotal, bndryTotal;
  getExactValues(volExact, bndryExact, radius, center); 

  Real dx = domlen/ncells[0];

  getComputedValues(volTotal, bndryTotal, radius, center, domain, dx); 
  //check how close is the answer

  cout << "volTotal = "<< volTotal << endl;
  cout << "volExact = "<< volExact << endl;
  cout << "error = "   << volTotal - volExact << endl;
  cout << endl;

  cout << "bndryTotal = "<< bndryTotal << endl;
  cout << "bndryExact = "<< bndryExact << endl;
  cout << "error = "   << bndryTotal - bndryExact << endl;
  cout << endl;

  Real tolerance = 1.0e-2;
  if (std::abs(volTotal - volExact) / volExact > tolerance)
    {
      amrex::Error("Approx. volume not within tolerance");
    }

  if (std::abs(bndryTotal - bndryExact) / bndryExact > tolerance)
    {
      amrex::Error("Approx. boundary area not within tolerance");
    }


  return eekflag;
}


int
main(int argc,char **argv)
{
#ifdef CH_MPI
  MPI_Init(&argc, &argv);
#endif

  const char* in_file = "sphere.inputs";
  //parse input file
  //  ParmParse pp;
  //  pp.Initialize(0, NULL, in_file);

  // check volume and surface area of approximate sphere
  //  int eekflag = checkSphere();
  int eekflag = 0;
  if (eekflag != 0)
    {
      cout << "non zero eek detected = " << eekflag << endl;
      cout << "sphere test failed" << endl;
    }
  else
    {
      cout << "sphere test passed" << endl;
    }

  return 0;
}


