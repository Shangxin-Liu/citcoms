/*
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *<LicenseText>
 *
 * CitcomS by Louis Moresi, Shijie Zhong, Lijie Han, Eh Tan,
 * Clint Conrad, Michael Gurnis, and Eun-seo Choi.
 * Copyright (C) 1994-2005, California Institute of Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *</LicenseText>
 *
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#include "element_definitions.h"
#include "global_defs.h"
#include <math.h>
#ifdef CITCOM_ALLOW_ANISOTROPIC_VISC
#include "anisotropic_viscosity.h"
#endif

void set_mg_defaults(E)
     struct All_variables *E;
{
  void assemble_forces_iterative();
  void solve_constrained_flow_iterative();
  void mg_allocate_vars();

  E->control.NMULTIGRID = 1;
  E->build_forcing_term =   assemble_forces_iterative;
  E->solve_stokes_problem = solve_constrained_flow_iterative;
  E->solver_allocate_vars = mg_allocate_vars;
}

void mg_allocate_vars(E)
     struct All_variables *E;
{
}


/* =====================================================
   Function to inject data from fine to coarse grid (i.e.
   just dropping values at shared grid points.
   ===================================================== */

void inject_scalar(E,start_lev,AU,AD)
     struct All_variables *E;
     int start_lev;
     float *AU,*AD;  /* data on upper/lower mesh  */

{
    int i,m,el,node_coarse,node_fine,sl_minus,eqn,eqn_coarse;
    void gather_to_1layer_node ();

    const int dims = E->mesh.nsd;
    const int ends = enodes[dims];

    if(start_lev == E->mesh.levmin)   {
        fprintf(E->fp,"Warning, attempting to project below lowest level\n");
        return;
    }

    sl_minus = start_lev-1;


      for(el=1;el<=E->lmesh.NEL[sl_minus];el++)
        for(i=1;i<=ends;i++)       {
          node_coarse = E->IEN[sl_minus][CPPR][el].node[i];
          node_fine=E->IEN[start_lev][CPPR][E->EL[sl_minus][CPPR][el].sub[i]].node[i];
          AD[node_coarse] = AU[node_fine];
        }
}

void inject_vector(E,start_lev,AU,AD)
     struct All_variables *E;
     int start_lev;
     double *AU,*AD;  /* data on upper/lower mesh  */

{
    int i,j,m,el,node_coarse,node_fine,sl_minus,eqn_fine,eqn_coarse;

    const int dims = E->mesh.nsd;
    const int ends = enodes[dims];

    void gather_to_1layer_id ();

    if(start_lev == E->mesh.levmin)   {
        fprintf(E->fp,"Warning, attempting to project below lowest level\n");
        return;
    }

    sl_minus = start_lev-1;

      for(el=1;el<=E->lmesh.NEL[sl_minus];el++)
        for(i=1;i<=ends;i++)       {
          node_coarse = E->IEN[sl_minus][CPPR][el].node[i];
          node_fine=E->IEN[start_lev][CPPR][E->EL[sl_minus][CPPR][el].sub[i]].node[i];
          for (j=1;j<=dims;j++)    {
            eqn_fine   = E->ID[start_lev][CPPR][node_fine].doff[j];
            eqn_coarse = E->ID[sl_minus][CPPR][node_coarse].doff[j];
            AD[eqn_coarse] = AU[eqn_fine];
            }
          }
}


/* =====================================================
   Function to inject data from coarse to fine grid (i.e.
   just dropping values at shared grid points.
   ===================================================== */

void un_inject_vector(E,start_lev,AD,AU)

     struct All_variables *E;
     int start_lev;
     double *AU,*AD;  /* data on upper/lower mesh  */
{
    int i,m;
    int el,node,node_plus;
    int eqn1,eqn_plus1;
    int eqn2,eqn_plus2;
    int eqn3,eqn_plus3;

    const int dims = E->mesh.nsd;
    const int ends = enodes[dims];
    const int sl_plus = start_lev+1;
    const int neq = E->lmesh.NEQ[sl_plus];
    const int nels = E->lmesh.NEL[start_lev];

    assert(start_lev != E->mesh.levmax  /* un_injection */);

    for(i=1;i<neq;i++)
      AU[i]=0.0;

      for(el=1;el<=nels;el++)
        for(i=1;i<=ENODES3D;i++)  {
          node = E->IEN[start_lev][CPPR][el].node[i];
	  node_plus=E->IEN[sl_plus][CPPR][E->EL[start_lev][CPPR][el].sub[i]].node[i];

	  eqn1 = E->ID[start_lev][CPPR][node].doff[1];
	  eqn2 = E->ID[start_lev][CPPR][node].doff[2];
	  eqn3 = E->ID[start_lev][CPPR][node].doff[3];
	  eqn_plus1 = E->ID[sl_plus][CPPR][node_plus].doff[1];
	  eqn_plus2 = E->ID[sl_plus][CPPR][node_plus].doff[2];
	  eqn_plus3 = E->ID[sl_plus][CPPR][node_plus].doff[3];
	  AU[eqn_plus1] = AD[eqn1];
	  AU[eqn_plus2] = AD[eqn2];
	  AU[eqn_plus3] = AD[eqn3];
	  }
}


/* =======================================================================================
   Interpolation from coarse grid to fine. See the appology attached to project() if you get
   stressed out by node based assumptions. If it makes you feel any better, I don't like
   it much either.
   ======================================================================================= */


void interp_vector(E,start_lev,AD,AU)

    struct All_variables *E;
     int start_lev;
     double *AD,*AU;  /* data on upper/lower mesh  */
{
    void un_inject_vector();
    void fill_in_gaps();
    void from_rtf_to_xyz();
    void from_xyz_to_rtf();
    void scatter_to_nlayer_id();

    int i,j,k,m;
    float x1,x2;
    float n1,n2;
    int noxz,node0,node1,node2;
    int eqn0,eqn1,eqn2;

    const int level = start_lev + 1;
    const int dims =E->mesh.nsd;
    const int ends= enodes[dims];

    const int nox = E->lmesh.NOX[level];
    const int noz = E->lmesh.NOZ[level];
    const int noy = E->lmesh.NOY[level];
    const int high_eqn = E->lmesh.NEQ[level];

    if (start_lev==E->mesh.levmax) return;

    from_rtf_to_xyz(E,start_lev,AD,AU);    /* transform in xyz coordinates */
    un_inject_vector(E,start_lev,AU,E->temp); /*  information from lower level */
    fill_in_gaps(E,E->temp,level);
    from_xyz_to_rtf(E,level,E->temp,AU);      /* get back to rtf coordinates */
}


/*  ==============================================
    function to project viscosity down to all the
    levels in the problem. (no gaps for vbcs)
    ==============================================  */

void project_viscosity(E)
     struct All_variables *E;

{
    int lv,i,j,k,m,sl_minus;
    void inject_scalar();
    void project_scalar();
    void project_scalar_e();
    void inject_scalar_e();
    void visc_from_gint_to_nodes();
    void visc_from_nodes_to_gint();
    void visc_from_gint_to_ele();
    void visc_from_ele_to_gint();

    const int nsd=E->mesh.nsd;
    const int vpts=vpoints[nsd];

    float *viscU,*viscD;

  lv = E->mesh.levmax;

    viscU=(float *)malloc((1+E->lmesh.NNO[lv])*sizeof(float));
    viscD=(float *)malloc((1+vpts*E->lmesh.NEL[lv-1])*sizeof(float));

#ifdef CITCOM_ALLOW_ANISOTROPIC_VISC /* allow for anisotropy */
  if(E->viscosity.allow_anisotropic_viscosity){
    for(lv=E->mesh.levmax;lv>E->mesh.levmin;lv--)     {
      sl_minus = lv -1;
      if (E->viscosity.smooth_cycles==1)  {
	
        visc_from_gint_to_nodes(E,E->EVI[lv],viscU,lv);	/* isotropic */
	      project_scalar(E,lv,viscU,viscD);
	      visc_from_nodes_to_gint(E,viscD,E->EVI[sl_minus],sl_minus);
	
        /* anisotropic */
	      visc_from_gint_to_nodes(E,E->EVI2[lv],viscU,lv);
        project_scalar(E,lv,viscU,viscD);
        visc_from_nodes_to_gint(E,viscD,E->EVI2[sl_minus],sl_minus);
	
        visc_from_gint_to_nodes(E,E->EVIn1[lv],viscU,lv);
        project_scalar(E,lv,viscU,viscD);
        visc_from_nodes_to_gint(E,viscD,E->EVIn1[sl_minus],sl_minus);
	
        visc_from_gint_to_nodes(E,E->EVIn2[lv],viscU,lv);
        project_scalar(E,lv,viscU,viscD);
        visc_from_nodes_to_gint(E,viscD,E->EVIn2[sl_minus],sl_minus);
	
        visc_from_gint_to_nodes(E,E->EVIn3[lv],viscU,lv);
        project_scalar(E,lv,viscU,viscD);
        visc_from_nodes_to_gint(E,viscD,E->EVIn3[sl_minus],sl_minus);
      }
      else if (E->viscosity.smooth_cycles==2)   {
	      visc_from_gint_to_ele(E,E->EVI[lv],viscU,lv); /* isotropic */
	      inject_scalar_e(E,lv,viscU,E->EVI[sl_minus]);
	
        /* anisotropic */
	      visc_from_gint_to_ele(E,E->EVI2[lv],viscU,lv);
        inject_scalar_e(E,lv,viscU,E->EVI2[sl_minus]);
	      
        visc_from_gint_to_ele(E,E->EVIn1[lv],viscU,lv);
        inject_scalar_e(E,lv,viscU,E->EVIn1[sl_minus]);
	
        visc_from_gint_to_ele(E,E->EVIn2[lv],viscU,lv);
        inject_scalar_e(E,lv,viscU,E->EVIn2[sl_minus]);
	      
        visc_from_gint_to_ele(E,E->EVIn3[lv],viscU,lv);
        inject_scalar_e(E,lv,viscU,E->EVIn3[sl_minus]);
      }
      else if (E->viscosity.smooth_cycles==3)   {
	
        visc_from_gint_to_ele(E,E->EVI[lv],viscU,lv);/* isotropic */
	      project_scalar_e(E,lv,viscU,viscD);
	      visc_from_ele_to_gint(E,viscD,E->EVI[sl_minus],sl_minus);
	
        /* anisotropic */
	
        visc_from_gint_to_ele(E,E->EVI2[lv],viscU,lv);
        project_scalar_e(E,lv,viscU,viscD);
        visc_from_ele_to_gint(E,viscD,E->EVI2[sl_minus],sl_minus);
	
        visc_from_gint_to_ele(E,E->EVIn1[lv],viscU,lv);
        project_scalar_e(E,lv,viscU,viscD);
        visc_from_ele_to_gint(E,viscD,E->EVIn1[sl_minus],sl_minus);
	
        visc_from_gint_to_ele(E,E->EVIn2[lv],viscU,lv);
        project_scalar_e(E,lv,viscU,viscD);
        visc_from_ele_to_gint(E,viscD,E->EVIn2[sl_minus],sl_minus);
	
        visc_from_gint_to_ele(E,E->EVIn3[lv],viscU,lv);
        project_scalar_e(E,lv,viscU,viscD);
        visc_from_ele_to_gint(E,viscD,E->EVIn3[sl_minus],sl_minus);
      }
      else if (E->viscosity.smooth_cycles==0)  {
	
        inject_scalar(E,lv,E->VI[lv],E->VI[sl_minus]);/* isotropic */
	      visc_from_nodes_to_gint(E,E->VI[sl_minus],E->EVI[sl_minus],sl_minus);
	
        /* anisotropic */
	      inject_scalar(E,lv,E->VI2[lv],E->VI2[sl_minus]);
        visc_from_nodes_to_gint(E,E->VI2[sl_minus],E->EVI2[sl_minus],sl_minus);
	
        inject_scalar(E,lv,E->VIn1[lv],E->VIn1[sl_minus]);
        visc_from_nodes_to_gint(E,E->VIn1[sl_minus],E->EVIn1[sl_minus],sl_minus);
	
        inject_scalar(E,lv,E->VIn2[lv],E->VIn2[sl_minus]);
        visc_from_nodes_to_gint(E,E->VIn2[sl_minus],E->EVIn2[sl_minus],sl_minus);
	
        inject_scalar(E,lv,E->VIn3[lv],E->VIn3[sl_minus]);
        visc_from_nodes_to_gint(E,E->VIn3[sl_minus],E->EVIn3[sl_minus],sl_minus);
      }
      
        normalize_director_at_gint(E,E->EVIn1[sl_minus],E->EVIn2[sl_minus],E->EVIn3[sl_minus],sl_minus);      
    }
  }else{
#endif
    /* regular operation, isotropic viscosity */
    for(lv=E->mesh.levmax;lv>E->mesh.levmin;lv--)     {
      
      sl_minus = lv -1;
      
      if (E->viscosity.smooth_cycles==1)  {
        visc_from_gint_to_nodes(E,E->EVI[lv],viscU,lv);
        project_scalar(E,lv,viscU,viscD);
        visc_from_nodes_to_gint(E,viscD,E->EVI[sl_minus],sl_minus);
      }
      else if (E->viscosity.smooth_cycles==2)   {
        visc_from_gint_to_ele(E,E->EVI[lv],viscU,lv);
        inject_scalar_e(E,lv,viscU,E->EVI[sl_minus]);
      }
      else if (E->viscosity.smooth_cycles==3)   {
        visc_from_gint_to_ele(E,E->EVI[lv],viscU,lv);
        project_scalar_e(E,lv,viscU,viscD);
        visc_from_ele_to_gint(E,viscD,E->EVI[sl_minus],sl_minus);
      }
      else if (E->viscosity.smooth_cycles==0)  {
        inject_scalar(E,lv,E->VI[lv],E->VI[sl_minus]);
        visc_from_nodes_to_gint(E,E->VI[sl_minus],E->EVI[sl_minus],sl_minus);
      }
    }
#ifdef CITCOM_ALLOW_ANISOTROPIC_VISC
  }
#endif
    free((void *)viscU);
    free((void *)viscD);
}

/* ==================================================== */
void inject_scalar_e(E,start_lev,AU,AD)

     struct All_variables *E;
     int start_lev;
     float *AU,*AD;  /* data on upper/lower mesh  */
{
    int i,j,m;
    int el,node,e;
    float average,w;
    void gather_to_1layer_ele ();

    const int sl_minus = start_lev-1;
    const int nels_minus=E->lmesh.NEL[start_lev-1];
    const int dims=E->mesh.nsd;
    const int ends=enodes[E->mesh.nsd];
    const int vpts=vpoints[E->mesh.nsd];
    const int n_minus=nels_minus*vpts;


    for(i=1;i<=n_minus;i++)
       AD[i] = 0.0;

        for(el=1;el<=nels_minus;el++)
            for(i=1;i<=ENODES3D;i++)                {
                e = E->EL[sl_minus][CPPR][el].sub[i];
                AD[(el-1)*vpts+i] = AU[e];
                }
}

/* ==================================================== */
void project_scalar_e(E,start_lev,AU,AD)

     struct All_variables *E;
     int start_lev;
     float *AU,*AD;  /* data on upper/lower mesh  */
{
    int i,j,m;
    int el,node,e;
    float average,w;
    void gather_to_1layer_ele ();

    const int sl_minus = start_lev-1;
    const int nels_minus=E->lmesh.NEL[start_lev-1];
    const int  dims=E->mesh.nsd;
    const int ends=enodes[E->mesh.nsd];
    const double weight=(double) 1.0/ends;
    const int vpts=vpoints[E->mesh.nsd];
    const int n_minus=nels_minus*vpts;


    for(i=1;i<=n_minus;i++)
       AD[i] = 0.0;


    for(el=1;el<=nels_minus;el++)    {
        average=0.0;
        for(i=1;i<=ENODES3D;i++) {
            e = E->EL[sl_minus][CPPR][el].sub[i];
            average += AU[e];
            }

        AD[el] = average*weight;
        }
}

/* ==================================================== */
void project_scalar(E,start_lev,AU,AD)

     struct All_variables *E;
     int start_lev;
     float *AU,*AD;  /* data on upper/lower mesh  */
{
    int i,j,m;
    int el,node,node1;
    float average,w;
    void gather_to_1layer_node ();

    const int sl_minus = start_lev-1;
    const int nno_minus=E->lmesh.NNO[start_lev-1];
    const int nels_minus=E->lmesh.NEL[start_lev-1];
    const int  dims=E->mesh.nsd;
    const int ends=enodes[E->mesh.nsd];
    const double weight=(double) 1.0/ends;


   for(i=1;i<=nno_minus;i++)
     AD[i] = 0.0;

        for(el=1;el<=nels_minus;el++)
            for(i=1;i<=ENODES3D;i++) {
                average=0.0;
                node1 = E->EL[sl_minus][CPPR][el].sub[i];
                for(j=1;j<=ENODES3D;j++)                     {
                    node=E->IEN[start_lev][CPPR][node1].node[j];
                    average += AU[node];
                    }

                w=weight*average;

                node= E->IEN[sl_minus][CPPR][el].node[i];

                AD[node] += w * E->TWW[sl_minus][CPPR][el].node[i];
         }

   (E->exchange_node_f)(E,AD,sl_minus);

     for(i=1;i<=nno_minus;i++)  {
       AD[i] *= E->MASS[sl_minus][i];
     }
}

/* this is prefered scheme with averages */

void project_vector(E,start_lev,AU,AD,ic)

     struct All_variables *E;
     int start_lev,ic;
     double *AU,*AD;  /* data on upper/lower mesh  */
{
    int i,j,m;
    int el,node1,node,e1;
    int eqn1,eqn_minus1;
    int eqn2,eqn_minus2;
    int eqn3,eqn_minus3;
    double average1,average2,average3,w,weight;
    float CPU_time(),time;

    void from_rtf_to_xyz();
    void from_xyz_to_rtf();
    void gather_to_1layer_id ();

    const int sl_minus = start_lev-1;
    const int neq_minus=E->lmesh.NEQ[start_lev-1];
    const int nno_minus=E->lmesh.NNO[start_lev-1];
    const int nels_minus=E->lmesh.NEL[start_lev-1];
    const int  dims=E->mesh.nsd;
    const int ends=enodes[E->mesh.nsd];


    if (ic==1)
       weight = 1.0;
    else
       weight=(double) 1.0/ends;

   if (start_lev==E->mesh.levmin) return;

                /* convert into xyz coordinates */
      from_rtf_to_xyz(E,start_lev,AU,E->temp);

      for(i=0;i<neq_minus;i++)
        E->temp1[i] = 0.0;

                /* smooth in xyz coordinates */
        for(el=1;el<=nels_minus;el++)
          for(i=1;i<=ENODES3D;i++) {
                node= E->IEN[sl_minus][CPPR][el].node[i];
		average1=average2=average3=0.0;
		e1 = E->EL[sl_minus][CPPR][el].sub[i];
		for(j=1;j<=ENODES3D;j++) {
		    node1=E->IEN[start_lev][CPPR][e1].node[j];
		    average1 += E->temp[E->ID[start_lev][CPPR][node1].doff[1]];
		    average2 += E->temp[E->ID[start_lev][CPPR][node1].doff[2]];
		    average3 += E->temp[E->ID[start_lev][CPPR][node1].doff[3]];
		    }
		w = weight*E->TWW[sl_minus][CPPR][el].node[i];

		E->temp1[E->ID[sl_minus][CPPR][node].doff[1]] += w * average1;
		E->temp1[E->ID[sl_minus][CPPR][node].doff[2]] += w * average2;
	 	E->temp1[E->ID[sl_minus][CPPR][node].doff[3]] += w * average3;
                }


   (E->solver.exchange_id_d)(E, E->temp1, sl_minus);

     for(i=1;i<=nno_minus;i++)  {
       E->temp1[E->ID[sl_minus][CPPR][i].doff[1]] *= E->MASS[sl_minus][i];
       E->temp1[E->ID[sl_minus][CPPR][i].doff[2]] *= E->MASS[sl_minus][i];
       E->temp1[E->ID[sl_minus][CPPR][i].doff[3]] *= E->MASS[sl_minus][i];
       }

               /* back into rtf coordinates */
   from_xyz_to_rtf(E,sl_minus,E->temp1,AD);
}

/* ================================================= */
 void from_xyz_to_rtf(E,level,xyz,rtf)
 struct All_variables *E;
 int level;
 double *rtf,*xyz;
 {

 int i,j,m,eqn1,eqn2,eqn3;
 double cost,cosf,sint,sinf;

   for (i=1;i<=E->lmesh.NNO[level];i++)  {
     eqn1 = E->ID[level][CPPR][i].doff[1];
     eqn2 = E->ID[level][CPPR][i].doff[2];
     eqn3 = E->ID[level][CPPR][i].doff[3];
     sint = E->SinCos[level][CPPR][0][i];
     sinf = E->SinCos[level][CPPR][1][i];
     cost = E->SinCos[level][CPPR][2][i];
     cosf = E->SinCos[level][CPPR][3][i];
     rtf[eqn1] = xyz[eqn1]*cost*cosf
                  + xyz[eqn2]*cost*sinf
                  - xyz[eqn3]*sint;
     rtf[eqn2] = -xyz[eqn1]*sinf
                  + xyz[eqn2]*cosf;
     rtf[eqn3] = xyz[eqn1]*sint*cosf
                  + xyz[eqn2]*sint*sinf
                  + xyz[eqn3]*cost;
     }
 }

/* ================================================= */
 void from_rtf_to_xyz(E,level,rtf,xyz)
 struct All_variables *E;
 int level;
 double *rtf,*xyz;
 {

 int i,j,m,eqn1,eqn2,eqn3;
 double cost,cosf,sint,sinf;

   for (i=1;i<=E->lmesh.NNO[level];i++)  {
     eqn1 = E->ID[level][CPPR][i].doff[1];
     eqn2 = E->ID[level][CPPR][i].doff[2];
     eqn3 = E->ID[level][CPPR][i].doff[3];
     sint = E->SinCos[level][CPPR][0][i];
     sinf = E->SinCos[level][CPPR][1][i];
     cost = E->SinCos[level][CPPR][2][i];
     cosf = E->SinCos[level][CPPR][3][i];
     xyz[eqn1] = rtf[eqn1]*cost*cosf
                  - rtf[eqn2]*sinf
                  + rtf[eqn3]*sint*cosf;
     xyz[eqn2] = rtf[eqn1]*cost*sinf
                  + rtf[eqn2]*cosf
                  + rtf[eqn3]*sint*sinf;
     xyz[eqn3] = -rtf[eqn1]*sint
                  + rtf[eqn3]*cost;

     }
 }

 /* ========================================================== */
 void fill_in_gaps(E,temp,level)
    struct All_variables *E;
    int level;
    double *temp;
  {

    int i,j,k,m;
    float x1,x2;
    float n1,n2;
    int rnoz,noxz,node0,node1,node2;
    int eqn0,eqn1,eqn2;

    const int dims =E->mesh.nsd;
    const int ends= enodes[dims];

    const int nox = E->lmesh.NOX[level];
    const int noz = E->lmesh.NOZ[level];
    const int noy = E->lmesh.NOY[level];
    const int sl_minus = level-1;

    n1 = n2 =0.5;
    noxz = nox*noz;
    for(k=1;k<=noy;k+=2)          /* Fill in gaps in x direction */
      for(j=1;j<=noz;j+=2)
	  for(i=2;i<nox;i+=2)  {
	      node0 = j + (i-1)*noz + (k-1)*noxz; /* this node */
	      node1 = node0 - noz;
	      node2 = node0 + noz;

	      /* now for each direction */

	      eqn0=E->ID[level][CPPR][node0].doff[1];
	      eqn1=E->ID[level][CPPR][node1].doff[1];
	      eqn2=E->ID[level][CPPR][node2].doff[1];
	      temp[eqn0] = n1*temp[eqn1]+n2*temp[eqn2];

	      eqn0=E->ID[level][CPPR][node0].doff[2];
	      eqn1=E->ID[level][CPPR][node1].doff[2];
	      eqn2=E->ID[level][CPPR][node2].doff[2];
	      temp[eqn0] = n1*temp[eqn1]+n2*temp[eqn2];

	      eqn0=E->ID[level][CPPR][node0].doff[3];
	      eqn1=E->ID[level][CPPR][node1].doff[3];
	      eqn2=E->ID[level][CPPR][node2].doff[3];
	      temp[eqn0] = n1*temp[eqn1]+n2*temp[eqn2];
	      }

    n1 = n2 =0.5;
    for(i=1;i<=nox;i++)   /* Fill in gaps in y direction */
       for(j=1;j<=noz;j+=2)
  	  for(k=2;k<noy;k+=2)   {
	        node0 = j + (i-1)*noz + (k-1)*noxz; /* this node */
	        node1 = node0 - noxz;
	        node2 = node0 + noxz;

	        eqn0=E->ID[level][CPPR][node0].doff[1];
	        eqn1=E->ID[level][CPPR][node1].doff[1];
	        eqn2=E->ID[level][CPPR][node2].doff[1];
	        temp[eqn0] = n1*temp[eqn1]+n2*temp[eqn2];

	        eqn0=E->ID[level][CPPR][node0].doff[2];
	        eqn1=E->ID[level][CPPR][node1].doff[2];
	        eqn2=E->ID[level][CPPR][node2].doff[2];
	        temp[eqn0] = n1*temp[eqn1]+n2*temp[eqn2];

	        eqn0=E->ID[level][CPPR][node0].doff[3];
	        eqn1=E->ID[level][CPPR][node1].doff[3];
	        eqn2=E->ID[level][CPPR][node2].doff[3];
	        temp[eqn0] = n1*temp[eqn1]+n2*temp[eqn2];
	       }


    for(j=2;j<noz;j+=2)	  {
       x1 = E->sphere.R[level][j] - E->sphere.R[level][j-1];
       x2 = E->sphere.R[level][j+1] - E->sphere.R[level][j];
       n1 = x2/(x1+x2);
       n2 = 1.0-n1;
       for(k=1;k<=noy;k++)          /* Fill in gaps in z direction */
          for(i=1;i<=nox;i++)  {
		node0 = j + (i-1)*noz + (k-1)*noxz; /* this node */
		node1 = node0 - 1;
		node2 = node0 + 1;

	        eqn0=E->ID[level][CPPR][node0].doff[1];
	        eqn1=E->ID[level][CPPR][node1].doff[1];
	        eqn2=E->ID[level][CPPR][node2].doff[1];
	        temp[eqn0] = n1*temp[eqn1]+n2*temp[eqn2];

	        eqn0=E->ID[level][CPPR][node0].doff[2];
	        eqn1=E->ID[level][CPPR][node1].doff[2];
	        eqn2=E->ID[level][CPPR][node2].doff[2];
	        temp[eqn0] = n1*temp[eqn1]+n2*temp[eqn2];

	        eqn0=E->ID[level][CPPR][node0].doff[3];
	        eqn1=E->ID[level][CPPR][node1].doff[3];
	        eqn2=E->ID[level][CPPR][node2].doff[3];
	        temp[eqn0] = n1*temp[eqn1]+n2*temp[eqn2];
	        }
       }
}
