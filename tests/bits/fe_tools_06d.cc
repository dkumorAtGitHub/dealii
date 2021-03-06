// ---------------------------------------------------------------------
//
// Copyright (C) 2003 - 2017 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------


#include <deal.II/lac/sparsity_pattern.h>

#include "../tests.h"
#include "fe_tools_common.h"

// check
//   FETools::back_interpolate(6)
// for hp::DoFHandler without hanging nodes



template <int dim>
void
check_this(const FiniteElement<dim> &fe1, const FiniteElement<dim> &fe2)
{
  // only check if both elements have
  // support points. otherwise,
  // interpolation doesn't really
  // work
  if ((fe1.get_unit_support_points().size() == 0) ||
      (fe2.get_unit_support_points().size() == 0))
    return;
  //  likewise for non-primitive elements
  if (!fe1.is_primitive() || !fe2.is_primitive())
    return;
  // we need to have dof_constraints
  // for this test
  if (!fe1.constraints_are_implemented() || !fe2.constraints_are_implemented())
    return;

  Triangulation<dim> tria;
  GridGenerator::hyper_cube(tria, 0., 1.);
  tria.refine_global(2);

  hp::FECollection<dim>                hp_fe1(fe1);
  std::unique_ptr<hp::DoFHandler<dim>> hp_dof1(
    make_hp_dof_handler(tria, hp_fe1));

  Vector<double> in(hp_dof1->n_dofs());
  for (unsigned int i = 0; i < in.size(); ++i)
    in(i) = i;
  Vector<double> out(hp_dof1->n_dofs());

  FETools::back_interpolate(*hp_dof1, in, fe2, out);
  output_vector(out);
}
