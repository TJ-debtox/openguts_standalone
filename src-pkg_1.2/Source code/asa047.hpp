/*------------------------------------------------------------------------------

	This code is licensed under the GNU Lesser General Public License (Version 3)
	Authors:	Original FORTRAN77 version by R ONeill
				C++ version by John Burkardt
				Modifications by Tjalling Jager
	You should have received a copy of the LGPLv3 along with this software.
	For detailed information, have a look at the license files.

------------------------------------------------------------------------------*/

void nelmin ( double fn ( double x[] ), int n, double start[], double xmin[],
  double *ynewlo, double reqmin, double step[], int konvge, int kcount,
  int *icount, int *numres, int *ifault );
void timestamp ( );
