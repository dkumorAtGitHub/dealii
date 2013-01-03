//----------------------------------------------------------------------
//    $Id: block_info.cc 27473 2012-11-07 20:01:48Z heister $
//
//    Copyright (C) 2009, 2010, 2011, 2012 by the deal.II authors
//
//    This file is subject to QPL and may not be  distributed
//    without copyright and license information. Please refer
//    to the file deal.II/doc/license.html for the  text  and
//    further information on this license.
//
//----------------------------------------------------------------------

// same as block_info, but here we use the new DoFHandler instead
// of the MGDoFHandler

#include "../tests.h"
#include <deal.II/dofs/block_info.h>
#include <deal.II/dofs/dof_handler.h>
#include <deal.II/multigrid/mg_dof_handler.h>
#include <deal.II/grid/tria.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_system.h>
#include <fstream>


template <int dim>
void test_grid(const Triangulation<dim>& tr,
	       const FiniteElement<dim>& fe)
{
  DoFHandler<dim> mgdof(tr);
  mgdof.distribute_dofs(fe);
  mgdof.distribute_mg_dofs(fe);
  BlockInfo bi;
  bi.initialize(mgdof, false, false);
  bi.initialize_local(mgdof);
  
  deallog << "Global dofs    " << mgdof.n_dofs() << std::endl;
  deallog << "Global blocks ";
  for (unsigned int i=0;i<bi.global().size();++i)
      deallog << ' ' << bi.global().block_size(i);
  deallog << std::endl;

  for (unsigned int l=0;l<tr.n_levels();++l)
    {
      deallog << "Level dofs     " << mgdof.n_dofs(l) << std::endl;
      deallog << "Level block[" << l << ']';
      for (unsigned int i=0;i<bi.level(l).size();++i)
	deallog << ' ' << bi.level(l).block_size(i);
      deallog << std::endl;
    }
  
  deallog << "Local blocks  ";
  for (unsigned int i=0;i<bi.local().size();++i)
      deallog << ' ' << bi.local().block_size(i);
  deallog << std::endl;

  std::vector<unsigned int> renumbered(fe.dofs_per_cell);
  
  deallog << "Renumbering   ";
  for (unsigned int i=0;i<fe.dofs_per_cell;++i)
    {
      deallog << ' ' << bi.renumber(i);
      renumbered[bi.renumber(i)] = i;
    }
  deallog << std::endl;
  
  deallog << "Inverse       ";
  for (unsigned int i=0;i<fe.dofs_per_cell;++i)
    deallog << ' ' << renumbered[i];
  deallog << std::endl;
}


template<int dim>
void test_fe (const FiniteElement<dim>& fe)
{
  Triangulation<dim> tr;
  GridGenerator::hyper_cube(tr);
  tr.refine_global(5-dim);
  test_grid(tr, fe);
}


int main () 
{
  initlog(__FILE__);
  
  FE_Q<2> q21(1);
  FE_Q<2> q22(2);
  FESystem<2> s2(q21, 3, q22, 2);
  
  test_fe(q21);
  test_fe(q22);
  test_fe(s2);
  
  FE_Q<3> q31(1);
  FE_Q<3> q32(2);
  FESystem<3> s3(q31, 3, q32, 2);
  
  test_fe(q31);
  test_fe(q32);
  test_fe(s3);
}
