//----------------------------------------------------------------------------
//    $Id$
//    Version: $Name$
//
//    Copyright (C) 2000 - 2007, 2010 by the deal.II authors
//
//    This file is subject to QPL and may not be  distributed
//    without copyright and license information. Please refer
//    to the file deal.II/doc/license.html for the  text  and
//    further information on this license.
//
//----------------------------------------------------------------------------


#include "../tests.h"
#include <base/logstream.h>
#include <lac/vector.h>
#include <lac/block_vector.h>
#include <grid/tria.h>
#include <grid/grid_generator.h>
#include <dofs/dof_renumbering.h>
#include <dofs/dof_tools.h>
#include <fe/fe_dgp.h>
#include <fe/fe_dgq.h>
#include <fe/fe_q.h>
#include <fe/fe_raviart_thomas.h>
#include <fe/fe_system.h>
#include <multigrid/mg_dof_handler.h>
#include <multigrid/mg_transfer_block.h>
#include <multigrid/mg_tools.h>
#include <multigrid/mg_level_object.h>

#include <fstream>
#include <iomanip>
#include <iomanip>
#include <algorithm>
#include <numeric>

using namespace std;

template <int dim, typename number, int spacedim>
void
reinit_vector_by_blocks (
  const dealii::MGDoFHandler<dim,spacedim> &mg_dof,
  MGLevelObject<BlockVector<number> > &v,
  const std::vector<bool> &sel,
  std::vector<std::vector<unsigned int> >& ndofs)
{
  std::vector<bool> selected=sel;
				   // Compute the number of blocks needed
  const unsigned int n_selected
    = std::accumulate(selected.begin(),
		      selected.end(),
		      0U);

  if (ndofs.size() == 0)
    {
      std::vector<std::vector<unsigned int> >
	new_dofs(mg_dof.get_tria().n_levels(),
		 std::vector<unsigned int>(selected.size()));
      std::swap(ndofs, new_dofs);
      MGTools::count_dofs_per_block (mg_dof, ndofs);
    }

  for (unsigned int level=v.get_minlevel();
       level<=v.get_maxlevel();++level)
    {
      v[level].reinit(n_selected, 0);
      unsigned int k=0;
      for (unsigned int i=0;i<selected.size() && (k<v[level].n_blocks());++i)
	{
	  if (selected[i])
	    {
	      v[level].block(k++).reinit(ndofs[level][i]);
	    }
	  v[level].collect_sizes();
	}
    }
}


template <int dim>
void check_select(const FiniteElement<dim>& fe, unsigned int selected)
{
  deallog << fe.get_name()
	  << " select " << selected << std::endl;

  Triangulation<dim> tr;
  GridGenerator::hyper_cube(tr);
  tr.refine_global(2);

  MGDoFHandler<dim> mgdof(tr);
  DoFHandler<dim>& dof=mgdof;
  mgdof.distribute_dofs(fe);
  DoFRenumbering::component_wise(mgdof);
  vector<unsigned int> ndofs(fe.n_blocks());
  DoFTools::count_dofs_per_block(mgdof, ndofs);

  for (unsigned int l=0;l<tr.n_levels();++l)
    DoFRenumbering::component_wise(mgdof, l);
  std::vector<std::vector<unsigned int> > mg_ndofs(mgdof.get_tria().n_levels());
  MGTools::count_dofs_per_block(mgdof, mg_ndofs);

  deallog << "Global  dofs:";
  for (unsigned int i=0;i<ndofs.size();++i)
    deallog << ' ' << ndofs[i];
  deallog << std::endl;
  for (unsigned int l=0;l<mg_ndofs.size();++l)
    {
      deallog << "Level " << l << " dofs:";
      for (unsigned int i=0;i<mg_ndofs[l].size();++i)
	deallog << ' ' << mg_ndofs[l][i];
      deallog << std::endl;
    }

  MGTransferBlockSelect<double> transfer;
  transfer.build_matrices(dof, mgdof, selected);

				   // First, prolongate the constant
				   // function from the coarsest mesh
				   // to the finer ones. Since this is
				   // the embedding, we obtain the
				   // constant one and the l2-norm is
				   // the number of degrees of freedom.
  MGLevelObject< Vector<double> > u(0, tr.n_levels()-1);

  reinit_vector_by_blocks(mgdof, u, selected, mg_ndofs);

  u[0] = 1;
  transfer.prolongate(1,u[1],u[0]);
  transfer.prolongate(2,u[2],u[1]);
  deallog << "u0\t" << (int) (u[0]*u[0]+.4) << std::endl
	  << "u1\t" << (int) (u[1]*u[1]+.4) << std::endl
	  << "u2\t" << (int) (u[2]*u[2]+.4) << std::endl;
				   // Now restrict the same vectors.
  u[1] = 0.;
  u[0] = 0.;
  transfer.restrict_and_add(2,u[1],u[2]);
  transfer.restrict_and_add(1,u[0],u[1]);
  deallog << "u1\t" << (int) (u[1]*u[1]+.5) << std::endl
	  << "u0\t" << (int) (u[0]*u[0]+.5) << std::endl;

				   // Check copy to mg and back
				   // Fill a global vector by counting
				   // from one up
  BlockVector<double> v;
  v.reinit (ndofs);
  for (unsigned int i=0;i<v.size();++i)
    v(i) = i+1;

				   // See what part gets copied to mg
  u.resize(0, tr.n_levels()-1);
  reinit_vector_by_blocks(mgdof, u, selected, mg_ndofs);

  transfer.copy_to_mg(mgdof, u, v);
  for (unsigned int i=0; i<u[2].size();++i)
    deallog << ' ' << (int) u[2](i);
  deallog << std::endl;

				   // Now do the opposite: fill a
				   // multigrid vector counting the
				   // dofs and see where the numbers go
  for (unsigned int i=0;i<u[2].size();++i)
    u[2](i) = i+1;
  v = 0.;
  transfer.copy_from_mg(mgdof, v, u);
  for (unsigned int i=0; i<v.size();++i)
    deallog << ' ' << (int) v(i);
  deallog << std::endl;
  v.equ(-1., v);
  transfer.copy_from_mg_add(mgdof, v, u);
  deallog << "diff " << v.l2_norm() << std::endl;
}


int main()
{
  std::ofstream logfile("transfer_block_select/output");
  deallog << std::setprecision(3);
  deallog.attach(logfile);
  deallog.depth_console(0);
  deallog.threshold_double(1.e-10);

  FE_DGQ<2> q0(0);
  FE_DGQ<2> q1(1);
  FE_RaviartThomasNodal<2> rt0(0);
  FE_RaviartThomasNodal<2> rt1(1);

  FESystem<2> fe0(rt1, 1, q1, 1);
  FESystem<2> fe1(rt0, 2, q0, 2);

  check_select(fe0, 0);
  check_select(fe0, 1);

  check_select(fe1, 0);
  check_select(fe1, 1);
  check_select(fe1, 2);
  check_select(fe1, 3);
}
