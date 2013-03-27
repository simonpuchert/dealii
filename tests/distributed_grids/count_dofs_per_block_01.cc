//---------------------------------------------------------------------------
//    $Id$
//    Version: $Name$
//
//    Copyright (C) 2008, 2010 by the deal.II authors
//
//    This file is subject to QPL and may not be  distributed
//    without copyright and license information. Please refer
//    to the file deal.II/doc/license.html for the  text  and
//    further information on this license.
//
//---------------------------------------------------------------------------


// Test DoFTools::count_dofs_per_block


#include "../tests.h"
#include <deal.II/base/logstream.h>
#include <deal.II/base/tensor.h>
#include <deal.II/distributed/tria.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/intergrid_map.h>
#include <deal.II/base/utilities.h>
#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_tools.h>
#include <deal.II/fe/fe_system.h>
#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_dgq.h>

#include <fstream>
#include <numeric>
#include <cstdlib>


template<int dim>
void test()
{
  parallel::distributed::Triangulation<dim>
    triangulation (MPI_COMM_WORLD,
		   Triangulation<dim>::limit_level_difference_at_vertices);

  FESystem<dim> fe (FE_Q<dim>(3),2,
		    FE_DGQ<dim>(1),1);

  DoFHandler<dim> dof_handler (triangulation);

  GridGenerator::hyper_cube(triangulation);
  triangulation.refine_global (2);

  const unsigned int n_refinements[] = { 0, 4, 3, 2 };
  for (unsigned int i=0; i<n_refinements[dim]; ++i)
    {
				       // refine one-fifth of cells randomly
      std::vector<bool> flags (triangulation.n_active_cells(), false);
      for (unsigned int k=0; k<flags.size()/5 + 1; ++k)
	flags[rand() % flags.size()] = true;
				       // make sure there's at least one that
				       // will be refined
      flags[0] = true;

				       // refine triangulation
      unsigned int index=0;
      for (typename Triangulation<dim>::active_cell_iterator
	     cell = triangulation.begin_active();
	   cell != triangulation.end(); ++cell, ++index)
	if (flags[index])
	  cell->set_refine_flag();
      Assert (index == triangulation.n_active_cells(), ExcInternalError());

				       // flag all other cells for coarsening
				       // (this should ensure that at least
				       // some of them will actually be
				       // coarsened)
      index=0;
      for (typename Triangulation<dim>::active_cell_iterator
	     cell = triangulation.begin_active();
	   cell != triangulation.end(); ++cell, ++index)
	if (!flags[index])
	  cell->set_coarsen_flag();

      triangulation.execute_coarsening_and_refinement ();
      dof_handler.distribute_dofs (fe);

      std::vector<unsigned int> dofs_per_block (fe.n_components());
      DoFTools::count_dofs_per_block (dof_handler, dofs_per_block);

      Assert (std::accumulate (dofs_per_block.begin(), dofs_per_block.end(), 0U)
	      == dof_handler.n_dofs(),
	      ExcInternalError());
      
      unsigned int myid = Utilities::MPI::this_mpi_process (MPI_COMM_WORLD);
      if (myid == 0)
	{
	  deallog << "Total number of dofs: " << dof_handler.n_dofs() << std::endl;
	  for (unsigned int i=0; i<dofs_per_block.size(); ++i)
	    deallog << "Block " << i << " has " << dofs_per_block[i] << " global dofs"
		    << std::endl;
	}
    }
}


int main(int argc, char *argv[])
{
#ifdef DEAL_II_WITH_MPI
  Utilities::MPI::MPI_InitFinalize mpi_initialization(argc, argv, 1);
#else
  (void)argc;
  (void)argv;
#endif

  unsigned int myid = Utilities::MPI::this_mpi_process (MPI_COMM_WORLD);
  if (myid == 0)
    {
      std::ofstream logfile("count_dofs_per_block_01/output");
      deallog.attach(logfile);
      deallog.depth_console(0);
      deallog.threshold_double(1.e-10);

      deallog.push("2d");
      test<2>();
      deallog.pop();

      deallog.push("3d");
      test<3>();
      deallog.pop();
    }
  else
    {
      test<2>();
      test<3>();
    }


}
