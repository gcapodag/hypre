/*BHEADER**********************************************************************
 * (c) 1999   The Regents of the University of California
 *
 * See the file COPYRIGHT_and_DISCLAIMER for a complete copyright
 * notice, contact person, and disclaimer.
 *
 * $Revision$
 *********************************************************************EHEADER*/
/******************************************************************************
 *
 * IJMatrix_ParCSR interface
 *
 *****************************************************************************/
 
#include "headers.h"

/******************************************************************************
 *
 * hypre_IJMatrixSetLocalSizeParCSR
 *
 * sets local number of rows and number of columns of diagonal matrix on
 * current processor.
 *
 *****************************************************************************/

int
hypre_IJMatrixSetLocalSizeParCSR(hypre_IJMatrix *matrix,
			   	 int     	 local_m,
			   	 int     	 local_n)
{
   int ierr = 0;
   hypre_AuxParCSRMatrix *aux_matrix;
   aux_matrix = hypre_IJMatrixTranslator(matrix);
   if (aux_matrix)
   {
      hypre_AuxParCSRMatrixLocalNumRows(aux_matrix) = local_m;
      hypre_AuxParCSRMatrixLocalNumCols(aux_matrix) = local_n;
   }
   else
   {
      hypre_AuxParCSRMatrixCreate(&aux_matrix,local_m,local_n,NULL);
      hypre_IJMatrixTranslator(matrix) = aux_matrix;
   }
   return ierr;
}

/******************************************************************************
 *
 * hypre_IJMatrixCreateParCSR
 *
 * creates AuxParCSRMatrix and ParCSRMatrix if necessary,
 * generates arrays row_starts and col_starts using either previously
 * set data local_m and local_n (user defined) or generates them evenly
 * distributed if not previously defined by user.
 *
 *****************************************************************************/
int
hypre_IJMatrixCreateParCSR(hypre_IJMatrix *matrix)
{
   MPI_Comm comm = hypre_IJMatrixContext(matrix);
   int global_m = hypre_IJMatrixM(matrix); 
   int global_n = hypre_IJMatrixN(matrix); 
   hypre_AuxParCSRMatrix *aux_matrix = hypre_IJMatrixTranslator(matrix);
   int local_m;   
   int local_n;   
   int ierr = 0;


   int *row_starts;
   int *col_starts;
   int num_cols_offd = 0;
   int num_nonzeros_diag = 0;
   int num_nonzeros_offd = 0;
   int num_procs, my_id;
   int equal;
   int i;
   MPI_Comm_size(comm, &num_procs);
   MPI_Comm_rank(comm, &my_id);

   if (aux_matrix)
   {
      local_m = hypre_AuxParCSRMatrixLocalNumRows(aux_matrix);   
      local_n = hypre_AuxParCSRMatrixLocalNumCols(aux_matrix);
   }
   else
   {
      hypre_AuxParCSRMatrixCreate(&aux_matrix,-1,-1,NULL);
      local_m = -1;
      local_n = -1;
      hypre_IJMatrixTranslator(matrix) = aux_matrix;
   }

   if (local_m < 0)
   {
      row_starts = NULL;
   }
   else
   {
      row_starts = hypre_CTAlloc(int,num_procs+1);

      if (num_procs > 1)
         MPI_Allgather(&local_m,1,MPI_INT,&row_starts[1],1,MPI_INT,comm);
      else
         row_starts[1] = global_m;
   }
   if (local_n < 0)
   {
      col_starts = NULL;
   }
   else
   {
      col_starts = hypre_CTAlloc(int,num_procs+1);

      if (num_procs > 1)
         MPI_Allgather(&local_n,1,MPI_INT,&col_starts[1],1,MPI_INT,comm);
      else
         col_starts[1] = global_n;
   }

   if (row_starts && col_starts)
   {
      equal = 1;
      for (i=0; i < num_procs; i++)
      {
         row_starts[i+1] += row_starts[i];
         col_starts[i+1] += col_starts[i];
         if (row_starts[i+1] != col_starts[i+1])
	 equal = 0;
      }
      if (equal)
      {
         hypre_TFree(col_starts);
         col_starts = row_starts;
      }
   }

   hypre_IJMatrixLocalStorage(matrix) = hypre_ParCSRMatrixCreate(comm,global_m,
		global_n,row_starts, col_starts, num_cols_offd, 
		num_nonzeros_diag, num_nonzeros_offd);
   return ierr;
}

/******************************************************************************
 *
 * hypre_SetIJMatrixRowSizesParcsr
 *
 *****************************************************************************/
int
hypre_IJMatrixSetRowSizesParCSR(hypre_IJMatrix *matrix,
			      	const int      *sizes)
{
   int *row_space;
   int local_num_rows;
   int i;
   hypre_AuxParCSRMatrix *aux_matrix;
   aux_matrix = hypre_IJMatrixTranslator(matrix);
   if (aux_matrix)
      local_num_rows = hypre_AuxParCSRMatrixLocalNumRows(aux_matrix);
   else
      return -1;
   
   row_space =  hypre_AuxParCSRMatrixRowSpace(aux_matrix);
   if (!row_space)
      row_space = hypre_CTAlloc(int, local_num_rows);
   for (i = 0; i < local_num_rows; i++)
      row_space[i] = sizes[i];
   hypre_AuxParCSRMatrixRowSpace(aux_matrix) = row_space;
   return 0;
}

/******************************************************************************
 *
 * hypre_IJMatrixSetDiagRowSizesParCSR
 * sets diag_i inside the diag part of the ParCSRMatrix,
 * requires exact row sizes for diag
 *
 *****************************************************************************/
int
hypre_IJMatrixSetDiagRowSizesParCSR(hypre_IJMatrix *matrix,
			      	    const int	   *sizes)
{
   int local_num_rows;
   int i, ierr = 0;
   hypre_ParCSRMatrix *par_matrix;
   hypre_AuxParCSRMatrix *aux_matrix = hypre_IJMatrixTranslator(matrix);
   hypre_CSRMatrix *diag;
   int *diag_i;
   if (!hypre_IJMatrixLocalStorage(matrix))
      ierr = hypre_IJMatrixCreateParCSR(matrix);
   if (ierr) return ierr;
   par_matrix = hypre_IJMatrixLocalStorage(matrix);
   
   hypre_AuxParCSRMatrixNeedAux(aux_matrix) = 0;
   diag =  hypre_ParCSRMatrixDiag(par_matrix);
   diag_i =  hypre_CSRMatrixI(diag);
   local_num_rows = hypre_CSRMatrixNumRows(diag);
   if (!diag_i)
      diag_i = hypre_CTAlloc(int, local_num_rows+1);
   for (i = 0; i < local_num_rows; i++)
      diag_i[i+1] = diag_i[i] + sizes[i];
   hypre_CSRMatrixI(diag) = diag_i;
   hypre_CSRMatrixNumNonzeros(diag) = diag_i[local_num_rows];
   return 0;
}

/******************************************************************************
 *
 * hypre_IJMatrixSetOffDiagRowSizesParCSR
 * sets offd_i inside the offd part of the ParCSRMatrix,
 * requires exact row sizes for offd
 *
 *****************************************************************************/
int
hypre_IJMatrixSetOffDiagRowSizesParCSR(hypre_IJMatrix *matrix,
			      	       const int      *sizes)
{
   int local_num_rows;
   int i, ierr = 0;
   hypre_AuxParCSRMatrix *aux_matrix = hypre_IJMatrixTranslator(matrix);
   hypre_ParCSRMatrix *par_matrix;
   hypre_CSRMatrix *offd;
   int *offd_i;
   par_matrix = hypre_IJMatrixLocalStorage(matrix);
   if (!par_matrix)
      ierr = hypre_IJMatrixCreateParCSR(matrix);
   if (ierr) return ierr;
   
   hypre_AuxParCSRMatrixNeedAux(aux_matrix) = 0;
   offd =  hypre_ParCSRMatrixOffd(par_matrix);
   offd_i =  hypre_CSRMatrixI(offd);
   local_num_rows = hypre_CSRMatrixNumRows(offd);
   if (!offd_i)
      offd_i = hypre_CTAlloc(int, local_num_rows+1);
   for (i = 0; i < local_num_rows; i++)
      offd_i[i+1] = offd_i[i] + sizes[i];
   hypre_CSRMatrixI(offd) = offd_i;
   hypre_CSRMatrixNumNonzeros(offd) = offd_i[local_num_rows];
   return 0;
}

/******************************************************************************
 *
 * hypre_IJMatrixInitializeParCSR
 *
 * initializes AuxParCSRMatrix and ParCSRMatrix as necessary
 *
 *****************************************************************************/

int
hypre_IJMatrixInitializeParCSR(hypre_IJMatrix *matrix)
{
   int ierr = 0;
   hypre_ParCSRMatrix *par_matrix = hypre_IJMatrixLocalStorage(matrix);
   hypre_AuxParCSRMatrix *aux_matrix = hypre_IJMatrixTranslator(matrix);
   int local_num_rows = hypre_AuxParCSRMatrixLocalNumRows(aux_matrix);
   int local_num_cols = hypre_AuxParCSRMatrixLocalNumCols(aux_matrix);

   if (par_matrix)
   {
      if (local_num_rows < 0)
         hypre_AuxParCSRMatrixLocalNumRows(aux_matrix) = 
		hypre_CSRMatrixNumRows(hypre_ParCSRMatrixDiag(par_matrix));
      if (local_num_cols < 0)
         hypre_AuxParCSRMatrixLocalNumCols(aux_matrix) = 
		hypre_CSRMatrixNumCols(hypre_ParCSRMatrixDiag(par_matrix));
   }
   else
   {
      ierr = hypre_IJMatrixCreateParCSR(matrix);
      par_matrix = hypre_IJMatrixLocalStorage(matrix);
   }
   ierr += hypre_ParCSRMatrixInitialize(par_matrix);
   ierr += hypre_AuxParCSRMatrixInitialize(aux_matrix);
   return ierr;
}

/******************************************************************************
 *
 * hypre_IJMatrixInsertBlockParCSR
 *
 * inserts a block of values into an IJMatrix, currently it just uses
 * InsertIJMatrixRowParCSR
 *
 *****************************************************************************/
int
hypre_IJMatrixInsertBlockParCSR(hypre_IJMatrix *matrix,
		       	        int	        m,
		                int	        n,
		                const int      *rows,
		                const int      *cols,
		                const double   *coeffs)
{
   int ierr = 0;
   int i, in;
   for (i=0; i < m; i++)
   {
      in = i*n;
      hypre_IJMatrixInsertRowParCSR(matrix,n,rows[i],&cols[in],&coeffs[in]);
   }
   return ierr;
}
/******************************************************************************
 *
 * hypre_IJMatrixAddToBlockParCSR
 *
 * adds a block of values to an IJMatrix, currently it just uses
 * IJMatrixAddToRowParCSR
 *
 *****************************************************************************/

int
hypre_IJMatrixAddToBlockParCSR(hypre_IJMatrix *matrix,
		       	       int	       m,
		               int	       n,
		               const int      *rows,
		               const int      *cols,
		               const double   *coeffs)
{
   int ierr = 0;
   int i, in;
   for (i=0; i < m; i++)
   {
      in = i*n;
      hypre_IJMatrixAddToRowParCSR(matrix,n,rows[i],&cols[in],&coeffs[in]);
   }
   return ierr;
}

/******************************************************************************
 *
 * hypre_IJMatrixInsertRowParCSR
 *
 * inserts a row into an IJMatrix, 
 * if diag_i and offd_i are known, those values are inserted directly
 * into the ParCSRMatrix,
 * if they are not known, an auxiliary structure, AuxParCSRMatrix is used
 *
 *****************************************************************************/
int
hypre_IJMatrixInsertRowParCSR(hypre_IJMatrix *matrix,
		              int	      n,
		              int	      row,
		              const int	     *indices,
		              const double   *coeffs)
{
   int ierr = 0;
   hypre_ParCSRMatrix *par_matrix;
   hypre_AuxParCSRMatrix *aux_matrix;
   int *row_starts;
   int *col_starts;
   MPI_Comm comm = hypre_IJMatrixContext(matrix);
   int num_procs, my_id;
   int row_local;
   int col_0, col_n;
   int i;
   /* double temp; */
   int *indx_diag, *indx_offd;
   int **aux_j;
   int *local_j;
   double **aux_data;
   double *local_data;
   int diag_space, offd_space;
   int *row_length, *row_space;
   int need_aux;
   int indx_0, indx_offd_0;
   int diag_indx, offd_indx;

   hypre_CSRMatrix *diag;
   int *diag_i;
   int *diag_j;
   double *diag_data;

   hypre_CSRMatrix *offd;
   int *offd_i;
   int *offd_j;
   double *offd_data;

   MPI_Comm_size(comm, &num_procs);
   MPI_Comm_rank(comm, &my_id);
   par_matrix = hypre_IJMatrixLocalStorage( matrix );
   aux_matrix = hypre_IJMatrixTranslator(matrix);
   row_space = hypre_AuxParCSRMatrixRowSpace(aux_matrix);
   row_length = hypre_AuxParCSRMatrixRowLength(aux_matrix);
   row_starts = hypre_ParCSRMatrixRowStarts(par_matrix);
   col_starts = hypre_ParCSRMatrixColStarts(par_matrix);
   col_0 = col_starts[my_id];
   col_n = col_starts[my_id+1]-1;
   need_aux = hypre_AuxParCSRMatrixNeedAux(aux_matrix);

   if (row >= row_starts[my_id] && row < row_starts[my_id+1])
   {
      row_local = row - row_starts[my_id]; /* compute local row number */
      if (need_aux)
      {
         aux_j = hypre_AuxParCSRMatrixAuxJ(aux_matrix);
         aux_data = hypre_AuxParCSRMatrixAuxData(aux_matrix);
            
         row_length[row_local] = n;
         
         if ( row_space[row_local] < n)
         {
   	    aux_j[row_local] = hypre_TReAlloc(aux_j[row_local],int,n);
   	    aux_data[row_local] = hypre_TReAlloc(aux_data[row_local],double,n);
            row_space[row_local] = n;
         }
         
         local_j = aux_j[row_local];
         local_data = aux_data[row_local];
         for (i=0; i < n; i++)
         {
   	    local_j[i] = indices[i];
   	    local_data[i] = coeffs[i];
         }
   
      /* sort data according to column indices */

         qsort1(local_j,local_data,0,n-1);
 
      }
      else /* insert immediately into data into ParCSRMatrix structure */
      {
	 diag = hypre_ParCSRMatrixDiag(par_matrix);
	 offd = hypre_ParCSRMatrixOffd(par_matrix);
         diag_i = hypre_CSRMatrixI(diag);
         diag_j = hypre_CSRMatrixJ(diag);
         diag_data = hypre_CSRMatrixData(diag);
         offd_i = hypre_CSRMatrixI(offd);
         offd_j = hypre_CSRMatrixJ(offd);
         offd_data = hypre_CSRMatrixData(offd);
	 offd_indx = offd_i[row_local];
	 indx_0 = diag_i[row_local];
	 diag_indx = indx_0;
	 indx_offd_0 = offd_indx;
	 
  	 for (i=0; i < n; i++)
	 {
	    if (indices[i] < col_0 || indices[i] > col_n)/* insert into offd */	
	    {
	       offd_j[offd_indx] = indices[i];
	       offd_data[offd_indx++] = coeffs[i];
	    }
	    else  /* insert into diag */
	    {
	       diag_j[diag_indx] = indices[i] - col_0;
	       diag_data[diag_indx++] = coeffs[i];
	    }
	 }
	 qsort1(offd_j, offd_data, indx_offd_0, offd_indx-1);
	 qsort1(diag_j, diag_data, indx_0, diag_indx-1);

	 hypre_AuxParCSRMatrixIndxDiag(aux_matrix)[row_local] = diag_indx;
	 hypre_AuxParCSRMatrixIndxOffd(aux_matrix)[row_local] = offd_indx;
      }
   }
   return ierr;
}

/******************************************************************************
 *
 * hypre_IJMatrixAddToRowParCSR
 *
 * adds a row to an IJMatrix before assembly, 
 * 
 *****************************************************************************/
int
hypre_IJMatrixAddToRowParCSR(hypre_IJMatrix *matrix,
	                   int	           n,
		           int	           row,
		           const int      *indices,
		           const double   *coeffs)
{
   int ierr = 0;
   hypre_ParCSRMatrix *par_matrix;
   hypre_CSRMatrix *diag, *offd;
   hypre_AuxParCSRMatrix *aux_matrix;
   int *row_starts;
   int *col_starts;
   MPI_Comm comm = hypre_IJMatrixContext(matrix);
   int num_procs, my_id;
   int row_local;
   int col_0, col_n;
   int i;
   int *indx_diag, *indx_offd;
   int **aux_j;
   int *local_j;
   int *tmp_j, *tmp2_j;
   double **aux_data;
   double *local_data;
   double *tmp_data, *tmp2_data;
   int diag_space, offd_space;
   int *row_length, *row_space;
   int need_aux;
   int tmp_indx, indx;
   int size, old_size;
   int cnt, cnt_diag, cnt_offd, indx_0;
   int offd_indx, diag_indx;
   int *diag_i;
   int *diag_j;
   double *diag_data;
   int *offd_i;
   int *offd_j;
   double *offd_data;
   int *tmp_diag_i;
   int *tmp_diag_j;
   double *tmp_diag_data;
   int *tmp_offd_i;
   int *tmp_offd_j;
   double *tmp_offd_data;

   MPI_Comm_size(comm, &num_procs);
   MPI_Comm_rank(comm, &my_id);
   par_matrix = hypre_IJMatrixLocalStorage( matrix );
   aux_matrix = hypre_IJMatrixTranslator(matrix);
   row_space = hypre_AuxParCSRMatrixRowSpace(aux_matrix);
   row_length = hypre_AuxParCSRMatrixRowLength(aux_matrix);
   row_starts = hypre_ParCSRMatrixRowStarts(par_matrix);
   col_starts = hypre_ParCSRMatrixColStarts(par_matrix);
   col_0 = col_starts[my_id];
   col_n = col_starts[my_id+1]-1;
   need_aux = hypre_AuxParCSRMatrixNeedAux(aux_matrix);

   if (row >= row_starts[my_id] && row < row_starts[my_id+1])
   {
      row_local = row - row_starts[my_id]; /* compute local row number */
      if (need_aux)
      {
         aux_j = hypre_AuxParCSRMatrixAuxJ(aux_matrix);
         aux_data = hypre_AuxParCSRMatrixAuxData(aux_matrix);
         local_j = aux_j[row_local];
         local_data = aux_data[row_local];
	 tmp_j = hypre_CTAlloc(int,n);
	 tmp_data = hypre_CTAlloc(double,n);
	 tmp_indx = 0;
         for (i=0; i < n; i++)
	 {
	    tmp_j[tmp_indx] = indices[i];
	    tmp_data[tmp_indx++] = coeffs[i];
	 }
	 qsort1(tmp_j,tmp_data,0,tmp_indx-1);
	 indx = 0;
	 size = row_length[row_local]; 
	 for (i=1; i < row_length[row_local]; i++)
	 {
	    while (indx < tmp_indx && local_j[i] > tmp_j[indx])
	    {
	       size++;
	       indx++;
	    }
	    if (indx == tmp_indx) break;
	    if (local_j[i] == tmp_j[indx])
	    {
	       indx++;
	    }
	 }
	 size += tmp_indx-indx;
	    
         old_size = row_length[row_local];   
         row_length[row_local] = size;
         
         if ( row_space[row_local] < size)
         {
	    aux_j[row_local] = hypre_TReAlloc(aux_j[row_local],int,size);
	    aux_data[row_local] = hypre_TReAlloc(aux_data[row_local],
					double,size);
            row_space[row_local] = size;
            local_j = aux_j[row_local];
            local_data = aux_data[row_local];
         }
        /* merge local and tmp into local */

         indx = 0; 
	 cnt = old_size; 

	 for (i=1; i < old_size; i++)
	 {
	    while (indx < tmp_indx && local_j[i] > tmp_j[indx])
	    {
	       local_j[cnt] = tmp_j[indx];
	       local_data[cnt++] = tmp_data[indx++];
	    }
	    if (indx == tmp_indx) break;
	    if (local_j[i] == tmp_j[indx])
	    {
	       local_j[i] = tmp_j[indx];
	       local_data[i] += tmp_data[indx++];
	    }
	 }
         for (i=indx; i < tmp_indx; i++)
         {
   	    local_j[cnt] = tmp_j[i];
   	    local_data[cnt++] = tmp_data[i];
         }
   
      /* sort data according to column indices */

         qsort1(local_j,local_data,0,cnt-1);
	 hypre_TFree(tmp_j); 
	 hypre_TFree(tmp_data); 
      }
      else /* insert immediately into data into ParCSRMatrix structure */
      {
	 offd_indx = hypre_AuxParCSRMatrixIndxOffd(aux_matrix)[row_local];
	 diag_indx = hypre_AuxParCSRMatrixIndxDiag(aux_matrix)[row_local];
         diag = hypre_ParCSRMatrixDiag(par_matrix);
         diag_i = hypre_CSRMatrixI(diag);
         diag_j = hypre_CSRMatrixJ(diag);
         diag_data = hypre_CSRMatrixData(diag);
         offd = hypre_ParCSRMatrixOffd(par_matrix);
         offd_i = hypre_CSRMatrixI(offd);
         offd_j = hypre_CSRMatrixJ(offd);
         offd_data = hypre_CSRMatrixData(offd);

	 indx_0 = diag_i[row_local];
	 
	 tmp_diag_j = hypre_CTAlloc(int,n);
	 tmp_diag_data = hypre_CTAlloc(double,n);
	 cnt_diag = 0;
	 tmp_offd_j = hypre_CTAlloc(int,n);
	 tmp_offd_data = hypre_CTAlloc(double,n);
	 cnt_offd = 0;
  	 for (i=0; i < n; i++)
	 {
	    if (indices[i] < col_0 || indices[i] > col_n)/* insert into offd */	
	    {
	       tmp_offd_j[cnt_offd] = indices[i];
	       tmp_offd_data[cnt_offd++] = coeffs[i];
	    }
	    /* diagonal element */
	    else  /* insert into diag */
	    {
	       tmp_diag_j[cnt_diag] = indices[i] - col_0;
	       tmp_diag_data[cnt_diag++] = coeffs[i];
	    }
	 }
	 qsort1(tmp_diag_j,tmp_diag_data,0,cnt_diag-1);
	 qsort1(tmp_offd_j,tmp_offd_data,0,cnt_offd-1);

         diag_indx = hypre_AuxParCSRMatrixIndxDiag(aux_matrix)[row_local];
	 if (diag_indx == 0)
	    diag_indx = indx_0;
	 cnt = diag_indx;
	 indx = 0;
	 for (i=diag_i[row_local]; i < diag_indx; i++)
	 {
	    while (indx < cnt_diag && diag_j[i] > tmp_diag_j[indx])
	    {
	       diag_j[cnt] = tmp_diag_j[indx];
	       diag_data[cnt++] = tmp_diag_data[indx++];
	    }
	    if (indx == cnt_diag) break;
	    if (diag_j[i] == tmp_diag_j[indx])
	    {
	       diag_j[i] = tmp_diag_j[indx];
	       diag_data[i] += tmp_diag_data[indx++];
	    }
	 }
         for (i=indx; i < cnt_diag; i++)
         {
   	    diag_j[cnt] = tmp_diag_j[i];
   	    diag_data[cnt++] = tmp_diag_data[i];
         }
   
      /* sort data according to column indices */

         qsort1(diag_j,diag_data,indx_0,cnt-1);
	 hypre_TFree(tmp_diag_j); 
	 hypre_TFree(tmp_diag_data); 

	 hypre_AuxParCSRMatrixIndxDiag(aux_matrix)[row_local] = cnt;

         offd_indx = hypre_AuxParCSRMatrixIndxOffd(aux_matrix)[row_local];
	 if (offd_indx == 0)
	    offd_indx = offd_i[row_local];
	 cnt = offd_indx;
	 indx = 0;
	 for (i=offd_i[row_local]; i < offd_indx; i++)
	 {
	    while (indx < cnt_offd && offd_j[i] > tmp_offd_j[indx])
	    {
	       offd_j[cnt] = tmp_offd_j[indx];
	       offd_data[cnt++] = tmp_offd_data[indx++];
	    }
	    if (indx == cnt_offd) break;
	    if (offd_j[i] == tmp_offd_j[indx])
	    {
	       offd_j[i] = tmp_offd_j[indx];
	       offd_data[i] += tmp_offd_data[indx++];
	    }
	 }
         for (i=indx; i < cnt_offd; i++)
         {
   	    offd_j[cnt] = tmp_offd_j[i];
   	    offd_data[cnt++] = tmp_offd_data[i];
         }
   
      /* sort data according to column indices */

         qsort1(offd_j,offd_data,offd_i[row_local],cnt-1);
	 hypre_TFree(tmp_offd_j); 
	 hypre_TFree(tmp_offd_data); 

	 hypre_AuxParCSRMatrixIndxOffd(aux_matrix)[row_local] = cnt;
      }
   }
   return ierr;
}

/******************************************************************************
 *
 * hypre_IJMatrixAssembleParCSR
 *
 * assembles IJMAtrix from AuxParCSRMatrix auxiliary structure
 *****************************************************************************/
int
hypre_IJMatrixAssembleParCSR(hypre_IJMatrix *matrix)
{
   int ierr = 0;
   MPI_Comm comm = hypre_IJMatrixContext(matrix);
   hypre_ParCSRMatrix *par_matrix = hypre_IJMatrixLocalStorage(matrix);
   hypre_AuxParCSRMatrix *aux_matrix = hypre_IJMatrixTranslator(matrix);
   hypre_CSRMatrix *diag = hypre_ParCSRMatrixDiag(par_matrix);
   hypre_CSRMatrix *offd = hypre_ParCSRMatrixOffd(par_matrix);
   int *diag_i = hypre_CSRMatrixI(diag);
   int *offd_i = hypre_CSRMatrixI(offd);
   int *diag_j;
   int *offd_j;
   double *diag_data;
   double *offd_data;
   int *row_starts = hypre_ParCSRMatrixRowStarts(par_matrix);
   int *col_starts = hypre_ParCSRMatrixColStarts(par_matrix);
   int j_indx, cnt, i, j, j0, diag_pos;
   int num_cols_offd;
   int *col_map_offd;
   int *row_length;
   int *row_space;
   int **aux_j;
   double **aux_data;
   int *indx_diag;
   int *indx_offd;
   int need_aux = hypre_AuxParCSRMatrixNeedAux(aux_matrix);
   int my_id, num_procs;
   int num_rows;
   int i_diag, i_offd;
   int *local_j;
   double *local_data;
   int col_0, col_n;
   int nnz_offd;
   int *aux_offd_j;
   double temp; 

   MPI_Comm_size(comm, &num_procs); 
   MPI_Comm_rank(comm, &my_id);
   num_rows = row_starts[my_id+1] - row_starts[my_id]; 
/* move data into ParCSRMatrix if not there already */ 
   if (need_aux)
   {
      aux_j = hypre_AuxParCSRMatrixAuxJ(aux_matrix);
      aux_data = hypre_AuxParCSRMatrixAuxData(aux_matrix);
      row_length = hypre_AuxParCSRMatrixRowLength(aux_matrix);
      col_0 = col_starts[my_id];
      col_n = col_starts[my_id+1]-1;
      i_diag = 0;
      i_offd = 0;
      for (i=0; i < num_rows; i++)
      {
	 local_j = aux_j[i];
	 local_data = aux_data[i];
	 diag_pos = -1;
	 for (j=0; j < row_length[i]; j++)
	 {
	    if (local_j[j] < col_0 || local_j[j] > col_n)
	       i_offd++;
	    else
	    {
	       i_diag++;
	       if (local_j[j]-col_0 == i) diag_pos = j;
	    }
	 }
	 /* move diagonal element into first space */
	 if (diag_pos > -1)
	 {
	    local_j[diag_pos] = local_j[0];
	    local_j[0] = i+col_0;
	    temp = local_data[0];
	    local_data[0] = local_data[diag_pos];
	    local_data[diag_pos] = temp;
	    
	 }
	 diag_i[i+1] = i_diag;
	 offd_i[i+1] = i_offd;
      }
      diag_j = hypre_CTAlloc(int,i_diag);
      diag_data = hypre_CTAlloc(double,i_diag);
      offd_j = hypre_CTAlloc(int,i_offd);
      offd_data = hypre_CTAlloc(double,i_offd);
      i_diag = 0;
      i_offd = 0;
      for (i=0; i < num_rows; i++)
      {
	 local_j = aux_j[i];
	 local_data = aux_data[i];
	 for (j=0; j < row_length[i]; j++)
	 {
	    if (local_j[j] < col_0 || local_j[j] > col_n)
	    {
	       offd_j[i_offd] = local_j[j];
	       offd_data[i_offd++] = local_data[j];
	    }
	    else
	    {
	       diag_j[i_diag] = local_j[j] - col_0;
	       diag_data[i_diag++] = local_data[j];
	    }
	 }
      }
      hypre_CSRMatrixJ(diag) = diag_j;      
      hypre_CSRMatrixData(diag) = diag_data;      
      hypre_CSRMatrixNumNonzeros(diag) = diag_i[num_rows];      
      hypre_CSRMatrixJ(offd) = offd_j;      
      hypre_CSRMatrixData(offd) = offd_data;      
      hypre_CSRMatrixNumNonzeros(offd) = offd_i[num_rows];      
   }
   else
   {
      /* move diagonal elements into first space */

      diag_j = hypre_CSRMatrixJ(diag);
      diag_data = hypre_CSRMatrixData(diag);
      for (i=0; i < num_rows; i++)
      {
	 j0 = diag_i[i];
         for (j=j0; j < diag_i[i+1]; j++)
         {
	    if (diag_j[j] == i)
	    {
	       diag_j[j] = diag_j[j0];
	       diag_j[j0] = i;
	       temp = diag_data[j];
	       diag_data[j] = diag_data[j0];
	       diag_data[j0] = temp;
	       break;
	    }
	 }
      }

      offd_j = hypre_CSRMatrixJ(offd);
   }

/*  generate col_map_offd */
   nnz_offd = offd_i[num_rows];
   if (nnz_offd)
   {
      aux_offd_j = hypre_CTAlloc(int, nnz_offd);
      for (i=0; i < nnz_offd; i++)
         aux_offd_j[i] = offd_j[i];
      qsort0(aux_offd_j,0,nnz_offd-1);
      num_cols_offd = 1;
      for (i=0; i < nnz_offd-1; i++)
      {
         if (aux_offd_j[i+1] > aux_offd_j[i])
            num_cols_offd++;
      }
      col_map_offd = hypre_CTAlloc(int,num_cols_offd);
      col_map_offd[0] = aux_offd_j[0];
      cnt = 0;
      for (i=1; i < nnz_offd; i++)
      {
         if (aux_offd_j[i] > col_map_offd[cnt])
         {
	    cnt++;
	    col_map_offd[cnt] = aux_offd_j[i];
         }
      }
      for (i=0; i < nnz_offd; i++)
      {
         offd_j[i] = hypre_BinarySearch(col_map_offd,offd_j[i],num_cols_offd);
      }
      hypre_ParCSRMatrixColMapOffd(par_matrix) = col_map_offd;    
      hypre_CSRMatrixNumCols(offd) = num_cols_offd;    
      hypre_TFree(aux_offd_j);
   }

   hypre_AuxParCSRMatrixDestroy(aux_matrix);

   hypre_IJMatrixTranslator(matrix) = NULL;

   return ierr;
}

/******************************************************************************
 *
 * hypre_IJMatrixDistributeParCSR
 *
 * takes an IJMatrix generated for one processor and distributes it
 * across many processors according to row_starts and col_starts,
 * if row_starts and/or col_starts NULL, it distributes them evenly.
 *
 *****************************************************************************/
int
hypre_IJMatrixDistributeParCSR(hypre_IJMatrix *matrix,
			       const int      *row_starts,
			       const int      *col_starts)
{
   int ierr = 0;
   hypre_ParCSRMatrix *old_matrix = hypre_IJMatrixLocalStorage(matrix);
   hypre_ParCSRMatrix *par_matrix;
   hypre_CSRMatrix *diag = hypre_ParCSRMatrixDiag(old_matrix);
   par_matrix = hypre_CSRMatrixToParCSRMatrix(hypre_ParCSRMatrixComm(old_matrix)
		, diag, (int *) row_starts, (int *) col_starts);
   ierr = hypre_ParCSRMatrixDestroy(old_matrix);
   hypre_IJMatrixLocalStorage(matrix) = par_matrix;
   return ierr;
}

/******************************************************************************
 *
 * hypre_IJMatrixApplyParCSR
 *
 * NOT IMPLEMENTED YET
 *
 *****************************************************************************/
int
hypre_IJMatrixApplyParCSR(hypre_IJMatrix  *matrix,
		    	  hypre_ParVector *x,
		          hypre_ParVector *b)
{
   int ierr = 0;

   return ierr;
}

/******************************************************************************
 *
 * hypre_IJMatrixDestroyParCSR
 *
 * frees an IJMatrix
 *
 *****************************************************************************/
int
hypre_IJMatrixDestroyParCSR(hypre_IJMatrix *matrix)
{
   return hypre_ParCSRMatrixDestroy(hypre_IJMatrixLocalStorage(matrix));
}

/******************************************************************************
 *
 * hypre_IJMatrixGetRowPartitioningParCSR
 *
 * returns a pointer to the row partitioning
 *
 *****************************************************************************/
int
hypre_IJMatrixGetRowPartitioningParCSR(hypre_IJMatrix *matrix,
			   	       const int     **row_partitioning)
{
   int ierr = 0;
   hypre_ParCSRMatrix *par_matrix;
   par_matrix = hypre_IJMatrixLocalStorage(matrix);
   if (!par_matrix)
   {
      return -1;
   }
   *row_partitioning = hypre_ParCSRMatrixRowStarts(par_matrix);
   return ierr;
}

/******************************************************************************
 *
 * hypre_IJMatrixGetColPartitioningParCSR
 *
 * returns a pointer to the column partitioning
 *
 *****************************************************************************/
int
hypre_IJMatrixGetColPartitioningParCSR(hypre_IJMatrix *matrix,
			   	       const int     **col_partitioning)
{
   int ierr = 0;
   hypre_ParCSRMatrix *par_matrix;
   par_matrix = hypre_IJMatrixLocalStorage(matrix);
   if (!par_matrix)
   {
      return -1;
   }
   *col_partitioning = hypre_ParCSRMatrixColStarts(par_matrix);
   return ierr;
}
