/*BHEADER**********************************************************************
 * Copyright (c) 2013, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory. Written by
 * Jacob Schroder, Rob Falgout, Tzanio Kolev, Ulrike Yang, Veselin
 * Dobrev, et al. LLNL-CODE-660355. All rights reserved.
 *
 * This file is part of XBraid. Email xbraid-support@llnl.gov for support.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License (as published by the Free Software
 * Foundation) version 2.1 dated February 1999.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 ***********************************************************************EHEADER*/

/** \file _braid.c
 * \brief Source code for developer routines.  See braid.h for more information.
 *
 */

#include "_braid.h"
#include "braid_defs.h"
#include "_util.h"

#define DEBUG 0

#if DEBUG
braid_Int  FRefine_count = 0;
#endif

braid_Int _braid_error_flag = 0;
FILE    *_braid_printfile  = NULL;

/*----------------------------------------------------------------------------
 * Macros used below
 *----------------------------------------------------------------------------*/

/* Compute number of reals given some number of bytes (use ceiling) */
#define _braid_NBytesToNReals(nbytes, nreals) \
nreals = nbytes / sizeof(braid_Real) + ((nbytes % sizeof(braid_Real)) != 0)

/*----------------------------------------------------------------------------
 * Returns the index interval for 'proc' in a blocked data distribution
 *----------------------------------------------------------------------------*/

braid_Int
_braid_GetBlockDistInterval(braid_Int   npoints,
                            braid_Int   nprocs,
                            braid_Int   proc,
                            braid_Int  *ilower_ptr,
                            braid_Int  *iupper_ptr)
{
    braid_Int  ilower, iupper, quo, rem, p;

    quo = npoints/nprocs;
    rem = npoints%nprocs;

    p = proc;
    ilower = p*quo + (p < rem ? p : rem);
    p = proc+1;
    iupper = p*quo + (p < rem ? p : rem) - 1;

    *ilower_ptr = ilower;
    *iupper_ptr = iupper;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Returns the processor that owns 'index' in a blocked data distribution
 * (returns -1 if index is out of range)
 *----------------------------------------------------------------------------*/

braid_Int
_braid_GetBlockDistProc(braid_Int   npoints,
                        braid_Int   nprocs,
                        braid_Int   index,
                        braid_Int  *proc_ptr)
{
    braid_Int      proc, quo, rem, p, q;

    /* Compute processor number */
    if ((index < 0) || (index > (npoints-1)))
    {
        proc = -1;
    }
    else
    {
        quo = npoints/nprocs;
        rem = npoints%nprocs;

        if (quo > 0)
        {
            p = index/(quo+1);
            q = (index - rem*(quo+1))/quo;
            proc = (p < rem ? p : rem+q);
        }
        else
        {
            proc = index;
        }
    }

    *proc_ptr = proc;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Returns the index interval for my processor on the finest grid level
 *----------------------------------------------------------------------------*/

braid_Int
_braid_GetDistribution(braid_Core   core,
                       braid_Int   *ilower_ptr,
                       braid_Int   *iupper_ptr)
{
    MPI_Comm   comm    = _braid_CoreElt(core, comm);
    braid_Int  gupper = _braid_CoreElt(core, gupper);
    braid_Int  npoints, nprocs, proc;

    npoints = gupper + 1;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &proc);

    _braid_GetBlockDistInterval(npoints, nprocs, proc, ilower_ptr, iupper_ptr);

    return _braid_error_flag;
}


braid_Int
_braid_GetProcLeftOrRight( braid_Core  core,
                   braid_Int   level,
                   braid_Int   direction,
                   braid_Int   *proc_ptr)
{
    _braid_Grid  **grids = _braid_CoreElt( core, grids );
    if ( direction < 0 )
       *proc_ptr = _braid_GridElt( grids[level] , recv_proc );
    else
       *proc_ptr = _braid_GridElt( grids[level] , send_proc );
    return _braid_error_flag; 
}

/*----------------------------------------------------------------------------
 * Returns the processor that owns 'index' on the given grid 'level'
 * (returns -1 if index is out of range)
 *----------------------------------------------------------------------------*/
braid_Int
_braid_GetProc(braid_Core   core,
               braid_Int    level,
               braid_Int    index,
               braid_Int   *proc_ptr)
{
    MPI_Comm       comm   = _braid_CoreElt(core, comm);
    _braid_Grid  **grids  = _braid_CoreElt(core, grids);
    braid_Int      gupper = _braid_CoreElt(core, gupper);
    braid_Int      npoints, nprocs;
    braid_Int      l, cfactor;

    npoints = gupper + 1;
    MPI_Comm_size(comm, &nprocs);
    /* Map index to the finest grid */
    for (l = level-1; l > -1; l--)
    {
        cfactor = _braid_GridElt(grids[l], cfactor);
        _braid_MapCoarseToFine(index, cfactor, index);
    }

    _braid_GetBlockDistProc(npoints, nprocs, index, proc_ptr);
     
    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Returns the coarsening factor to use on grid 'level'
 *----------------------------------------------------------------------------*/

braid_Int
_braid_GetCFactor(braid_Core   core,
                  braid_Int    level,
                  braid_Int   *cfactor_ptr)
{
    braid_Int     *cfactors  = _braid_CoreElt(core, cfactors);
    braid_Int      cfdefault = _braid_CoreElt(core, cfdefault);
    braid_Int      cfactor;

    if (cfactors[level] != 0)
    {
        cfactor = cfactors[level];
    }
    else
    {
        cfactor = cfdefault;
    }
    *cfactor_ptr = cfactor;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

braid_Int
_braid_CommRecvInit(braid_Core           core,
                    braid_Int            level,
                    braid_Int            index,
                    braid_Vector        *vector_ptr,
                    _braid_CommHandle  **handle_ptr)
{
    MPI_Comm            comm = _braid_CoreElt(core, comm);
    braid_App           app  = _braid_CoreElt(core, app);
    _braid_CommHandle  *handle = NULL;
    void               *buffer;
    MPI_Request        *requests;
    MPI_Status         *status;
    braid_Int           proc, size, num_requests;
    braid_BufferStatus bstatus = _braid_CoreElt( core, bstatus );
 
    _braid_GetProcLeftOrRight( core, level, -1 , &proc );
    
     if (proc > -1)
    {
        handle = _braid_TAlloc(_braid_CommHandle, 1);

        /* Allocate buffer through user routine */
        _braid_BufferStatusInit( 0, 0, bstatus );
        _braid_CoreFcn(core, bufsize)(app, &size, bstatus);
        buffer = malloc(size);

        num_requests = 1;
        requests = _braid_CTAlloc(MPI_Request, num_requests);
        status   = _braid_CTAlloc(MPI_Status, num_requests);
        MPI_Irecv(buffer, size, MPI_BYTE, proc, 0, comm, &requests[0]);

        _braid_CommHandleElt(handle, request_type) = 1; /* recv type = 1 */
        _braid_CommHandleElt(handle, num_requests) = num_requests;
        _braid_CommHandleElt(handle, requests)     = requests;
        _braid_CommHandleElt(handle, status)       = status;
        _braid_CommHandleElt(handle, buffer)       = buffer;
        _braid_CommHandleElt(handle, vector_ptr)   = vector_ptr;
    }

    *handle_ptr = handle;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

braid_Int
_braid_CommSendInit(braid_Core           core,
                    braid_Int            level,
                    braid_Int            index,
                    braid_Vector         vector,
                    _braid_CommHandle  **handle_ptr)
{
    MPI_Comm            comm = _braid_CoreElt(core, comm);
    braid_App           app  = _braid_CoreElt(core, app);
    _braid_CommHandle  *handle = NULL;
    void               *buffer;
    MPI_Request        *requests;
    MPI_Status         *status;
    braid_Int           proc, size, num_requests;
    braid_BufferStatus  bstatus   = _braid_CoreElt(core, bstatus);

    if (1) 
         _braid_GetProcLeftOrRight( core, level, 1, &proc );
    else
         _braid_GetProc(core, level, index+1, &proc);
    
    if (proc > -1)
    {
        handle = _braid_TAlloc(_braid_CommHandle, 1);

        /* Allocate buffer through user routine */
        _braid_BufferStatusInit( 0, 0, bstatus );
        _braid_CoreFcn(core, bufsize)(app, &size, bstatus);
        buffer = malloc(size);

        /* Note that bufpack may return a size smaller than bufsize */
        _braid_StatusElt(bstatus, size) = size;
        _braid_CoreFcn(core, bufpack)(app, vector, buffer, bstatus);
        size = _braid_StatusElt( bstatus, size );

        num_requests = 1;
        requests = _braid_CTAlloc(MPI_Request, num_requests);
        status   = _braid_CTAlloc(MPI_Status, num_requests);
        MPI_Isend(buffer, size, MPI_BYTE, proc, 0, comm, &requests[0]);

        _braid_CommHandleElt(handle, request_type) = 0; /* send type = 0 */
        _braid_CommHandleElt(handle, num_requests) = num_requests;
        _braid_CommHandleElt(handle, requests)     = requests;
        _braid_CommHandleElt(handle, status)       = status;
        _braid_CommHandleElt(handle, buffer)       = buffer;
    }

    *handle_ptr = handle;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

braid_Int
_braid_CommWait(braid_Core          core,
                _braid_CommHandle **handle_ptr)
{
    braid_App           app    = _braid_CoreElt(core, app);
    _braid_CommHandle  *handle = *handle_ptr;

    if (handle != NULL)
    {
        braid_Int      request_type = _braid_CommHandleElt(handle, request_type);
        braid_Int      num_requests = _braid_CommHandleElt(handle, num_requests);
        MPI_Request   *requests     = _braid_CommHandleElt(handle, requests);
        MPI_Status    *status       = _braid_CommHandleElt(handle, status);
        void          *buffer       = _braid_CommHandleElt(handle, buffer);
        braid_BufferStatus bstatus  = _braid_CoreElt(core, bstatus);

        MPI_Waitall(num_requests, requests, status);

        if (request_type == 1) /* recv type */
        {
            _braid_BufferStatusInit( 0, 0, bstatus );
            braid_Vector  *vector_ptr = _braid_CommHandleElt(handle, vector_ptr);
            _braid_CoreFcn(core, bufunpack)(app, buffer, vector_ptr, bstatus);
        }

        _braid_TFree(requests);
        _braid_TFree(status);
        _braid_TFree(handle);
        _braid_TFree(buffer);

        *handle_ptr = NULL;
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Returns an index into the local u-vector for grid 'level' at point 'index'.
 * If the u-vector is not stored, returns -1.
 *----------------------------------------------------------------------------*/

braid_Int
_braid_UGetIndex(braid_Core   core,
                 braid_Int    level,
                 braid_Int    index,
                 braid_Int   *uindex_ptr)
{
    _braid_Grid        **grids       = _braid_CoreElt(core, grids);
    braid_Int            ilower      = _braid_GridElt(grids[level], ilower);
    braid_Int            iupper      = _braid_GridElt(grids[level], iupper);
    braid_Int            clower      = _braid_GridElt(grids[level], clower);
    braid_Int            cfactor     = _braid_GridElt(grids[level], cfactor);
    braid_Int            uindex, ic, iclo;

    uindex = -1;
    if ((index >= ilower) && (index <= iupper))
    {
        // If on level that only stores C-points
        if ( (_braid_CoreElt(core, storage) < 0) ||
                (level < _braid_CoreElt(core, storage)) )
        {
            if ( _braid_IsCPoint(index, cfactor) )
            {
                _braid_MapFineToCoarse(index, cfactor, ic);
                _braid_MapFineToCoarse(clower, cfactor, iclo);
                uindex = ic-iclo;
            }
        }
        else
        {
            uindex = index-ilower;
        }
    }

    *uindex_ptr = uindex;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Returns a reference to the local u-vector on grid 'level' at point 'index'.
 * If the u-vector is not stored, returns NULL.
 *----------------------------------------------------------------------------*/

braid_Int
_braid_UGetVectorRef(braid_Core     core,
                     braid_Int      level,
                     braid_Int      index,
                     braid_Vector  *u_ptr)
{
    _braid_Grid        **grids = _braid_CoreElt(core, grids);
    braid_Vector        *ua    = _braid_GridElt(grids[level], ua);
    braid_Int            iu;
    braid_Vector         u = NULL;

    _braid_UGetIndex(core, level, index, &iu);
    if (iu > -1)
    {
        u = ua[iu];
    }

    *u_ptr = u;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Stores a reference to the local u-vector on grid 'level' at point 'index'.
 * If the u-vector is not stored, nothing is done.
 *----------------------------------------------------------------------------*/

braid_Int
_braid_USetVectorRef(braid_Core    core,
                     braid_Int     level,
                     braid_Int     index,
                     braid_Vector  u)
{
    _braid_Grid        **grids = _braid_CoreElt(core, grids);
    braid_Vector        *ua    = _braid_GridElt(grids[level], ua);
    braid_Int            iu;

    _braid_UGetIndex(core, level, index, &iu);
    if (iu > -1)
    {
        ua[iu] = u;
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Returns a copy of the u-vector on grid 'level' at point 'index'.  If 'index'
 * is my "receive index" (as set by UCommInit(), for example), the u-vector will
 * be received from a neighbor processor.  If the u-vector is not stored, NULL
 * is returned.
 *----------------------------------------------------------------------------*/

braid_Int
_braid_UGetVector(braid_Core     core,
                  braid_Int      level,
                  braid_Int      index,
                  braid_Vector  *u_ptr)
{
    braid_App            app         = _braid_CoreElt(core, app);
    _braid_Grid        **grids       = _braid_CoreElt(core, grids);
    braid_Vector        *ua          = _braid_GridElt(grids[level], ua);
    braid_Int            recv_index  = _braid_GridElt(grids[level], recv_index);
    _braid_CommHandle   *recv_handle = _braid_GridElt(grids[level], recv_handle);
    braid_Vector         u = NULL;
    braid_Int            iu;

    if (index == recv_index)
    {
        /* If a recv was initiated, receive u value from neighbor processor */
        if (recv_index > -1)
        {
            _braid_CommWait(core, &recv_handle);
            _braid_GridElt(grids[level], recv_index)  = -1;
            _braid_GridElt(grids[level], recv_handle) = recv_handle;
            u = ua[-1];
        }
    }
    else
    {
        _braid_UGetIndex(core, level, index, &iu);
        if (iu > -1)
        {
            _braid_CoreFcn(core, clone)(app, ua[iu], &u);
        }
    }

    *u_ptr = u;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Stores the u-vector on grid 'level' at point 'index'.  If 'index' is my "send
 * index", a send is initiated to a neighbor processor.  If 'move' is true, the
 * u-vector is moved into core storage instead of copied.  If the u-vector is
 * not stored, nothing is done.
 *----------------------------------------------------------------------------*/

braid_Int
_braid_USetVector(braid_Core    core,
                  braid_Int     level,
                  braid_Int     index,
                  braid_Vector  u,
                  braid_Int     move)
{
    braid_App            app         = _braid_CoreElt(core, app);
    _braid_Grid        **grids       = _braid_CoreElt(core, grids);
    braid_Vector        *ua          = _braid_GridElt(grids[level], ua);
    braid_Int            send_index  = _braid_GridElt(grids[level], send_index);
    _braid_CommHandle   *send_handle = _braid_GridElt(grids[level], send_handle);
    braid_Int            iu;

    if (index == send_index)
    {
        /* Post send to neighbor processor */
        _braid_CommSendInit(core, level, index, u, &send_handle);
        _braid_GridElt(grids[level], send_index)  = -1;
        _braid_GridElt(grids[level], send_handle) = send_handle;
    }

    _braid_UGetIndex(core, level, index, &iu);
    if (iu > -1)
    {
        if (ua[iu] != NULL)
        {
            _braid_CoreFcn(core, free)(app, ua[iu]);
        }
        if (move)
        {
            ua[iu] = u;                                   /* move the vector */
        }
        else
        {
            _braid_CoreFcn(core, clone)(app, u, &ua[iu]); /* copy the vector */
        }
    }
    else if (move)
    {
        _braid_CoreFcn(core, free)(app, u);              /* free the vector */
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Basic communication (from the left, to the right)
 *----------------------------------------------------------------------------*/

braid_Int
_braid_UCommInitBasic(braid_Core  core,
                      braid_Int   level,
                      braid_Int   recv_msg,
                      braid_Int   send_msg,
                      braid_Int   send_now)
{
    _braid_Grid        **grids       = _braid_CoreElt(core, grids);
    braid_Int            ilower      = _braid_GridElt(grids[level], ilower);
    braid_Int            iupper      = _braid_GridElt(grids[level], iupper);
    braid_Vector        *ua          = _braid_GridElt(grids[level], ua);
    braid_Int            recv_index  = -1;
    braid_Int            send_index  = -1;
    _braid_CommHandle   *recv_handle = NULL;
    _braid_CommHandle   *send_handle = NULL;
    braid_Int            iu;

    /* Post receive */
    if (recv_msg)
    {
        recv_index = ilower-1;
        _braid_CommRecvInit(core, level, recv_index, &ua[-1], &recv_handle);
    }

    /* Post send */
    if (send_msg)
    {
        send_index = iupper;
        if (send_now)
        {
            _braid_UGetIndex(core, level, send_index, &iu);
            _braid_CommSendInit(core, level, send_index, ua[iu], &send_handle);
            send_index = -1;
        }
    }

    _braid_GridElt(grids[level], recv_index)  = recv_index ;
    _braid_GridElt(grids[level], send_index)  = send_index ;
    _braid_GridElt(grids[level], recv_handle) = recv_handle;
    _braid_GridElt(grids[level], send_handle) = send_handle;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Working on all intervals
 *----------------------------------------------------------------------------*/

braid_Int
_braid_UCommInit(braid_Core  core,
                 braid_Int   level)
{
    _braid_Grid        **grids       = _braid_CoreElt(core, grids);
    braid_Int            ilower      = _braid_GridElt(grids[level], ilower);
    braid_Int            iupper      = _braid_GridElt(grids[level], iupper);
    braid_Int            cfactor     = _braid_GridElt(grids[level], cfactor);
    braid_Vector        *ua          = _braid_GridElt(grids[level], ua);
    braid_Int            recv_index  = -1;
    braid_Int            send_index  = -1;
    _braid_CommHandle   *recv_handle = NULL;
    _braid_CommHandle   *send_handle = NULL;
    braid_Int            iu;

    /* Note that this routine works for the case of all points being C-points,
     * i.e., cfactor = 1.  A send and receive are always posted. */

    if (ilower <= iupper)
    {
        /* Post receive */
        _braid_CommRecvInit(core, level, ilower-1, &ua[-1], &recv_handle);
        recv_index = ilower-1;

        /* Only post send if iupper is a C-point, otherwise compute and send later */
        if ( _braid_IsCPoint(iupper, cfactor) )
        {
            _braid_UGetIndex(core, level, iupper, &iu);
            _braid_CommSendInit(core, level, iupper, ua[iu], &send_handle);
            send_index = -1;
        }
        else
        {
            send_index = iupper;
        }
    }

    _braid_GridElt(grids[level], recv_index)  = recv_index ;
    _braid_GridElt(grids[level], send_index)  = send_index ;
    _braid_GridElt(grids[level], recv_handle) = recv_handle;
    _braid_GridElt(grids[level], send_handle) = send_handle;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Working only on F-pt intervals
 *----------------------------------------------------------------------------*/

braid_Int
_braid_UCommInitF(braid_Core  core,
                  braid_Int   level)
{
    _braid_Grid        **grids       = _braid_CoreElt(core, grids);
    braid_Int            ilower      = _braid_GridElt(grids[level], ilower);
    braid_Int            iupper      = _braid_GridElt(grids[level], iupper);
    braid_Int            cfactor     = _braid_GridElt(grids[level], cfactor);
    braid_Vector        *ua          = _braid_GridElt(grids[level], ua);
    braid_Int            recv_index  = -1;
    braid_Int            send_index  = -1;
    _braid_CommHandle   *recv_handle = NULL;
    _braid_CommHandle   *send_handle = NULL;
    braid_Int            iu;

    if (ilower <= iupper)
    {
        /* Only post receive if ilower is an F-point */
        if ( _braid_IsFPoint(ilower, cfactor) )
        {
            _braid_CommRecvInit(core, level, ilower-1, &ua[-1], &recv_handle);
            recv_index = ilower-1;
        }

        /* Only post send if iupper is a C-point and iupper+1 is an F-point.  This
         * check allows for the case of cfactor=1, i.e., all C-points.  Otherwise,
         * if iupper+1 is an F-point, set send_index, so that when that point is
         * computed later, it is sent. */
        if ( _braid_IsCPoint(iupper, cfactor) && _braid_IsFPoint(iupper+1, cfactor))
        {
            _braid_UGetIndex(core, level, iupper, &iu);
            _braid_CommSendInit(core, level, iupper, ua[iu], &send_handle);
            send_index = -1;
        }
        else if ( _braid_IsFPoint(iupper+1, cfactor) )
        {
            send_index = iupper;
        }
    }

    _braid_GridElt(grids[level], recv_index)  = recv_index ;
    _braid_GridElt(grids[level], send_index)  = send_index ;
    _braid_GridElt(grids[level], recv_handle) = recv_handle;
    _braid_GridElt(grids[level], send_handle) = send_handle;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Finish up communication
 *----------------------------------------------------------------------------*/

braid_Int
_braid_UCommWait(braid_Core  core,
                 braid_Int   level)
{
    _braid_Grid        **grids       = _braid_CoreElt(core, grids);
    _braid_CommHandle   *recv_handle = _braid_GridElt(grids[level], recv_handle);
    _braid_CommHandle   *send_handle = _braid_GridElt(grids[level], send_handle);

    _braid_CommWait(core, &recv_handle);
    _braid_CommWait(core, &send_handle);
    _braid_GridElt(grids[level], recv_index)  = -1;
    _braid_GridElt(grids[level], send_index)  = -1;
    _braid_GridElt(grids[level], recv_handle) = recv_handle;
    _braid_GridElt(grids[level], send_handle) = send_handle;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

braid_Int
_braid_GetInterval(braid_Core   core,
                   braid_Int    level,
                   braid_Int    interval_index,
                   braid_Int   *flo_ptr,
                   braid_Int   *fhi_ptr,
                   braid_Int   *ci_ptr)
{
    _braid_Grid  **grids   = _braid_CoreElt(core, grids);
    braid_Int      ilower  = _braid_GridElt(grids[level], ilower);
    braid_Int      iupper  = _braid_GridElt(grids[level], iupper);
    braid_Int      clower  = _braid_GridElt(grids[level], clower);
    braid_Int      cupper  = _braid_GridElt(grids[level], cupper);
    braid_Int      cfactor = _braid_GridElt(grids[level], cfactor);
    braid_Int      flo, fhi, ci;

    flo = ilower;
    fhi = iupper;
    ci  = -1;

    if ( _braid_IsCPoint(clower, cfactor) )
    {
        flo = clower + (interval_index-1)*cfactor + 1;
        fhi = clower + (interval_index  )*cfactor - 1;
        if (flo < ilower)
        {
            flo = ilower;
        }
        if (fhi > iupper)
        {
            fhi = iupper;
        }

        ci = clower + interval_index*cfactor;
        if (ci > cupper)
        {
            ci = -1;  /* return -1 if no C-points */
        }
    }

    *flo_ptr = flo;
    *fhi_ptr = fhi;
    *ci_ptr  = ci;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

braid_Int
_braid_AccessVector(braid_Core          core,
                    braid_AccessStatus  status,
                    braid_Vector        u)
{
    braid_App      app    = _braid_CoreElt(core, app);

    _braid_CoreFcn(core, access)(app, u, status);

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Get an initial guess for ustop to use in the step routine (implicit schemes)
 *----------------------------------------------------------------------------*/

braid_Int
_braid_GetUInit(braid_Core     core,
                braid_Int      level,
                braid_Int      index,
                braid_Vector   u,
                braid_Vector  *ustop_ptr)
{
    _braid_Grid    **grids    = _braid_CoreElt(core, grids);
    braid_Int        ilower   = _braid_GridElt(grids[level], ilower);
    braid_Int        storage  = _braid_CoreElt(core, storage);
    braid_Vector    *va       = _braid_GridElt(grids[level], va);

    braid_Vector     ustop = *ustop_ptr;
    braid_Int        ii;

    ii = index-ilower;

    _braid_UGetVectorRef(core, level, index, &ustop);

    /* If ustop is NULL, then storage is only at C-points on this level and this
     * is an F-point.  See the comment block around FRestrict() for the fixed-point
     * logic behind our choices in ustop. */
    if( ustop == NULL)
    {
        if( (level == 0) || ( storage == -2 ) )
        {
            ustop = u;
        }
        else
        {
            ustop = va[ii];
        }
    }

    /* If you have storage at this point, use it, unless you're in compatibility mode (-2). */
    else if( storage == -2 )
    {
        ustop = u;
    }

    *ustop_ptr = ustop;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Integrate one time step
 *----------------------------------------------------------------------------*/

braid_Int
_braid_Step(braid_Core     core,
            braid_Int      level,
            braid_Int      index,
            braid_Vector   ustop,
            braid_Vector   u)
{
    braid_App        app      = _braid_CoreElt(core, app);
    braid_Real       tol      = _braid_CoreElt(core, tol);
    braid_Int        iter     = _braid_CoreElt(core, niter);
    braid_Int       *rfactors = _braid_CoreElt(core, rfactors);
    braid_Int       *wfactors = _braid_CoreElt(core, wfactors);
    _braid_Grid    **grids    = _braid_CoreElt(core, grids);
    braid_StepStatus status   = _braid_CoreElt(core, sstatus);
    braid_Int        nrefine  = _braid_CoreElt(core, nrefine);
    braid_Int        gupper   = _braid_CoreElt(core, gupper);
    braid_Int        ilower   = _braid_GridElt(grids[level], ilower);
    braid_Real      *ta       = _braid_GridElt(grids[level], ta);
    braid_Vector    *fa       = _braid_GridElt(grids[level], fa);

    braid_Int        ii;

    ii = index-ilower;
    _braid_StepStatusInit(ta[ii-1], ta[ii], tol, iter, level, nrefine, gupper, status);

    /* If ustop is set to NULL, use a default approach for setting it */
    if (ustop == NULL)
    {
        _braid_GetUInit(core, level, index, u, &ustop);
    }

    if (level == 0)
    {
        _braid_CoreFcn(core, step)(app, ustop, NULL, u, status);
        rfactors[ii] = _braid_StatusElt(status, rfactor);
        wfactors[ii] = _braid_StatusElt(status, wfactor);
        if ( !_braid_CoreElt(core, r_space) && _braid_StatusElt(status, r_space) )
            _braid_CoreElt(core, r_space) = 1;
    }
    else
    {
        if ( _braid_CoreElt(core, residual) == NULL )
        {
            _braid_CoreFcn(core, step)(app, ustop, NULL, u, status);
            if(fa[ii] != NULL)
            {
                _braid_CoreFcn(core, sum)(app, 1.0, fa[ii], 1.0, u);
            }
        }
        else
        {
            _braid_CoreFcn(core, step)(app, ustop, fa[ii], u, status);
        }
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Compute residual
 *----------------------------------------------------------------------------*/

braid_Int
_braid_Residual(braid_Core     core,
                braid_Int      level,
                braid_Int      index,
                braid_Vector   ustop,
                braid_Vector   r)
{
    braid_App        app      = _braid_CoreElt(core, app);
    braid_Real       tol      = _braid_CoreElt(core, tol);
    braid_Int        iter     = _braid_CoreElt(core, niter);
    braid_Int       *rfactors = _braid_CoreElt(core, rfactors);
    braid_Int       *wfactors = _braid_CoreElt(core, wfactors);
    _braid_Grid    **grids    = _braid_CoreElt(core, grids);
    braid_StepStatus status   = _braid_CoreElt(core, sstatus);
    braid_Int        nrefine  = _braid_CoreElt(core, nrefine);
    braid_Int        gupper   = _braid_CoreElt(core, gupper);
    braid_Int        ilower   = _braid_GridElt(grids[level], ilower);
    braid_Real      *ta       = _braid_GridElt(grids[level], ta);

    braid_Vector     rstop;
    braid_Int        ii;

    ii = index-ilower;
    _braid_StepStatusInit(ta[ii-1], ta[ii], tol, iter, level, nrefine, gupper, status);
    if ( _braid_CoreElt(core, residual) == NULL )
    {
        /* By default: r = ustop - \Phi(ustart)*/
        _braid_GetUInit(core, level, index, r, &rstop);
        _braid_CoreFcn(core, step)(app, rstop, NULL, r, status);
        _braid_CoreFcn(core, sum)(app, 1.0, ustop, -1.0, r);
        if (level == 0)
        {
            rfactors[ii] = _braid_StatusElt(status, rfactor);
            wfactors[ii] = _braid_StatusElt(status, wfactor);
            if ( !_braid_CoreElt(core, r_space) && _braid_StatusElt(status, r_space) )
                _braid_CoreElt(core, r_space) = 1;
        }
    }
    else
    {
        /* Call the user's residual routine */
        _braid_CoreFcn(core, residual)(app, ustop, r, status);
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Compute FAS residual = f - residual
 *----------------------------------------------------------------------------*/

braid_Int
_braid_FASResidual(braid_Core     core,
                   braid_Int      level,
                   braid_Int      index,
                   braid_Vector   ustop,
                   braid_Vector   r)
{
    braid_App        app    = _braid_CoreElt(core, app);
    _braid_Grid    **grids  = _braid_CoreElt(core, grids);
    braid_Int        ilower = _braid_GridElt(grids[level], ilower);
    braid_Vector    *fa     = _braid_GridElt(grids[level], fa);

    braid_Int        ii;

    _braid_Residual(core, level, index, ustop, r);
    if (level == 0)
    {
        _braid_CoreFcn(core, sum)(app, 0.0, r, -1.0, r);
    }
    else
    {
        ii = index-ilower;
        if(fa[ii] == NULL)
        {
            _braid_CoreFcn(core, sum)(app, 0.0, r, -1.0, r);
        }
        else
        {
            _braid_CoreFcn(core, sum)(app, 1.0, fa[ii], -1.0, r);
        }
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Coarsen in space
 *----------------------------------------------------------------------------*/

braid_Int
_braid_Coarsen(braid_Core     core,
               braid_Int      level,    /* coarse level */
               braid_Int      f_index,  /* fine index */
               braid_Int      c_index,  /* coarse index */
               braid_Vector   fvector,
               braid_Vector  *cvector)
{
    braid_App      app             = _braid_CoreElt(core, app);
    _braid_Grid  **grids           = _braid_CoreElt(core, grids);
    braid_CoarsenRefStatus cstatus = _braid_CoreElt(core, cstatus);
    braid_Int      nrefine         = _braid_CoreElt(core, nrefine);
    braid_Int      gupper          = _braid_CoreElt(core, gupper);
    braid_Int      c_ilower        = _braid_GridElt(grids[level], ilower);
    braid_Int      f_ilower        = _braid_GridElt(grids[level-1], ilower);
    braid_Real    *c_ta            = _braid_GridElt(grids[level], ta);
    braid_Real    *f_ta            = _braid_GridElt(grids[level-1], ta);

    braid_Int      c_ii = c_index-c_ilower;
    braid_Int      f_ii = f_index-f_ilower;

    if ( _braid_CoreElt(core, scoarsen) == NULL )
    {
        /* No spatial coarsening needed, just clone the fine vector.*/
        _braid_CoreFcn(core, clone)(app, fvector, cvector);
    }
    else
    {
        /* Call the user's coarsening routine */
        _braid_CoarsenRefStatusInit(f_ta[f_ii], f_ta[f_ii-1], f_ta[f_ii+1],
                                    c_ta[c_ii-1], c_ta[c_ii+1],
                                    level-1, nrefine, gupper, cstatus);
        _braid_CoreFcn(core, scoarsen)(app, fvector, cvector, cstatus);
    }
    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Refine in space (basic routine)
 *----------------------------------------------------------------------------*/

braid_Int
_braid_RefineBasic(braid_Core     core,
                   braid_Int      level,    /* fine level */
                   braid_Real    *f_ta,     /* pointer into fine time array */
                   braid_Real    *c_ta,     /* pointer into coarse time array */
                   braid_Vector   cvector,
                   braid_Vector  *fvector)
{
    braid_App              app     = _braid_CoreElt(core, app);
    braid_CoarsenRefStatus cstatus = _braid_CoreElt(core, cstatus);
    braid_Int              nrefine = _braid_CoreElt(core, nrefine);
    braid_Int              gupper  = _braid_CoreElt(core, gupper);

    if ( _braid_CoreElt(core, scoarsen) == NULL )
    {
        /* No spatial refinement needed, just clone the fine vector.*/
        _braid_CoreFcn(core, clone)(app, cvector, fvector);
    }
    else
    {
        /* Call the user's refinement routine */
        _braid_CoarsenRefStatusInit(f_ta[0], f_ta[-1], f_ta[+1], c_ta[-1], c_ta[+1],
                                    level, nrefine, gupper, cstatus);
        _braid_CoreFcn(core, srefine)(app, cvector, fvector, cstatus);
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Refine in space
 *----------------------------------------------------------------------------*/

braid_Int
_braid_Refine(braid_Core     core,
              braid_Int      level,    /* fine level */
              braid_Int      f_index,  /* fine index */
              braid_Int      c_index,  /* coarse index */
              braid_Vector   cvector,
              braid_Vector  *fvector)
{
    _braid_Grid  **grids           = _braid_CoreElt(core, grids);
    braid_Int      f_ilower        = _braid_GridElt(grids[level], ilower);
    braid_Int      c_ilower        = _braid_GridElt(grids[level+1], ilower);
    braid_Real    *f_ta            = _braid_GridElt(grids[level], ta);
    braid_Real    *c_ta            = _braid_GridElt(grids[level+1], ta);

    braid_Int      c_ii = c_index-c_ilower;
    braid_Int      f_ii = f_index-f_ilower;

    _braid_RefineBasic(core, level, &f_ta[f_ii], &c_ta[c_ii], cvector, fvector);

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Create a new grid object
 *----------------------------------------------------------------------------*/

braid_Int
_braid_GridInit(braid_Core     core,
                braid_Int      level,
                braid_Int      ilower,
                braid_Int      iupper,
                _braid_Grid  **grid_ptr)
{
    _braid_Grid   *grid;
    braid_Real    *ta;

    grid = _braid_CTAlloc(_braid_Grid, 1);

    _braid_GridElt(grid, level)  = level;
    _braid_GridElt(grid, ilower) = ilower;
    _braid_GridElt(grid, iupper) = iupper;
    _braid_GridElt(grid, recv_index) = -1;
    _braid_GridElt(grid, send_index) = -1;
    _braid_GridElt(grid, recv_proc)  = -1;
    _braid_GridElt(grid, send_proc)  = -1;
    /* Store each processor's time slice, plus one time value to the left
     * and to the right */
    ta = _braid_CTAlloc(braid_Real, iupper-ilower+3);
    _braid_GridElt(grid, ta_alloc) = ta;
    _braid_GridElt(grid, ta)       = ta+1;  /* shift */

    *grid_ptr = grid;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

braid_Int
_braid_GridClean(braid_Core    core,
                 _braid_Grid  *grid)
{
    braid_App      app      = _braid_CoreElt(core, app);
    braid_Int      ilower   = _braid_GridElt(grid, ilower);
    braid_Int      iupper   = _braid_GridElt(grid, iupper);
    braid_Int      nupoints = _braid_GridElt(grid, nupoints);
    braid_Vector  *ua       = _braid_GridElt(grid, ua);
    braid_Vector  *va       = _braid_GridElt(grid, va);
    braid_Vector  *fa       = _braid_GridElt(grid, fa);
    braid_Vector  *ua_alloc = _braid_GridElt(grid, ua_alloc);
    braid_Vector  *va_alloc = _braid_GridElt(grid, va_alloc);
    braid_Vector  *fa_alloc = _braid_GridElt(grid, fa_alloc);

    braid_Int      ii;

    if (ua_alloc)
    {
        for (ii = 0; ii < nupoints; ii++)
        {
            if (ua[ii] != NULL)
            {
                _braid_CoreFcn(core, free)(app, ua[ii]);
                ua[ii] = NULL;
            }
        }
    }
    if (va_alloc)
    {
        for (ii = -1; ii <= (iupper-ilower); ii++)
        {
            if (va[ii] != NULL)
            {
                _braid_CoreFcn(core, free)(app, va[ii]);
                va[ii] = NULL;
            }
        }
    }
    if (fa_alloc)
    {
        for (ii = -1; ii <= (iupper-ilower); ii++)
        {
            if (fa[ii] != NULL)
            {
                _braid_CoreFcn(core, free)(app, fa[ii]);
                fa[ii] = NULL;
            }
        }
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

braid_Int
_braid_GridDestroy(braid_Core    core,
                   _braid_Grid  *grid)
{
    if (grid)
    {
        braid_Vector  *ua_alloc = _braid_GridElt(grid, ua_alloc);
        braid_Real    *ta_alloc = _braid_GridElt(grid, ta_alloc);
        braid_Vector  *va_alloc = _braid_GridElt(grid, va_alloc);
        braid_Vector  *fa_alloc = _braid_GridElt(grid, fa_alloc);

        _braid_GridClean(core, grid);

        if (ua_alloc)
        {
            _braid_TFree(ua_alloc);
        }
        if (ta_alloc)
        {
            _braid_TFree(ta_alloc);
        }
        if (va_alloc)
        {
            _braid_TFree(va_alloc);
        }
        if (fa_alloc)
        {
            _braid_TFree(fa_alloc);
        }

        _braid_TFree(grid);
    }
    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Set initial guess at C-points
 *----------------------------------------------------------------------------*/

braid_Int
_braid_InitGuess(braid_Core  core,
                 braid_Int   level)
{
    braid_App      app      = _braid_CoreElt(core, app);
    braid_Int      seq_soln = _braid_CoreElt(core, seq_soln);
    _braid_Grid  **grids    = _braid_CoreElt(core, grids);
    braid_Int      ilower   = _braid_GridElt(grids[level], ilower);
    braid_Int      iupper   = _braid_GridElt(grids[level], iupper);
    braid_Int      clower   = _braid_GridElt(grids[level], clower);
    braid_Int      cupper   = _braid_GridElt(grids[level], cupper);
    braid_Int      cfactor  = _braid_GridElt(grids[level], cfactor);
    braid_Real    *ta       = _braid_GridElt(grids[level], ta);
    braid_Vector  *va       = _braid_GridElt(grids[level], va);

    braid_Vector   u;
    braid_Int      i, iu;

    if ( (level == 0) && (seq_soln == 1) )
    {
        /* If first processor, grab initial condition */
        if(ilower == 0)
        {
            _braid_CoreFcn(core, init)(app, ta[0], &u);
            _braid_USetVector(core, 0, 0, u, 0);
            ilower += 1;
        }
        /* Else, receive point to the left */
        else
        {
            _braid_UCommInitBasic(core, 0, 1, 0, 0);     /* Post receive to the left*/
            _braid_UGetVector(core, 0, ilower-1, &u);    /* Wait on receive */
        }

        /* Set Flag so that USetVector initiates send when iupper is available */
        _braid_GridElt(grids[level], send_index)  = iupper;

        /* Initialize all points on the finest grid with sequential time marching */
        for(i = ilower; i <= iupper; i++)
        {
            _braid_Step(core, 0, i, NULL, u);       /* Step forward */
            _braid_USetVector(core, 0, i, u, 0);    /* Store: copy u into core,
                                                    sending to left if needed */
        }

        _braid_UCommWait(core, 0);                 /* Wait on comm to finish */
    }
    else if (level == 0)
    {
        /* Only initialize the C-points on the finest grid */
        for (i = clower; i <= cupper; i += cfactor)
        {
            _braid_CoreFcn(core, init)(app, ta[i-ilower], &u);
            _braid_USetVectorRef(core, level, i, u);
        }
    }
    else
    {
        for (i = ilower; i <= iupper; i++)
        {
            _braid_UGetIndex(core, level, i, &iu);
            if (iu > -1)
            {
                _braid_CoreFcn(core, clone)(app, va[i-ilower], &u);
                _braid_USetVectorRef(core, level, i, u);
            }
        }
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

braid_Int
_braid_ComputeFullRNorm(braid_Core  core,
                        braid_Int   level,
                        braid_Real *return_rnorm)
{
    MPI_Comm           comm        = _braid_CoreElt(core, comm);
    braid_App          app         = _braid_CoreElt(core, app);
    braid_Real         tol         = _braid_CoreElt(core, tol);
    braid_Int          iter        = _braid_CoreElt(core, niter);
    _braid_Grid      **grids       = _braid_CoreElt(core, grids);
    braid_StepStatus   status      = _braid_CoreElt(core, sstatus);
    braid_Int          nrefine     = _braid_CoreElt(core, nrefine);
    braid_Int          gupper      = _braid_CoreElt(core, gupper);
    braid_Int          ncpoints    = _braid_GridElt(grids[level], ncpoints);
    braid_Int          tnorm       = _braid_CoreElt(core, tnorm);
    braid_Real        *ta          = _braid_GridElt(grids[level], ta);
    braid_Int          ilower      = _braid_GridElt(grids[level], ilower);
    _braid_CommHandle *send_handle;
    braid_Int          send_index;

    braid_Int        flo, fhi, fi, ci, ii, interval;
    braid_Real       rnorm_temp, rnorm = 0, global_rnorm = 0;
    braid_Vector     u, r;

    _braid_UCommInit(core, level);

    /* Start from the right-most interval. */
    for (interval = ncpoints; interval > -1; interval--)
    {
        _braid_GetInterval(core, level, interval, &flo, &fhi, &ci);
        if (flo <= fhi)
        {
            _braid_UGetVector(core, level, flo-1, &u);
        }
        else if (ci > 0)
        {
            _braid_UGetVector(core, level, ci-1, &u);
        }

        /* Generate F-points and get residual. */
        for (fi = flo; fi <= fhi; fi++)
        {
            _braid_CoreFcn(core, clone)(app, u, &r);
            _braid_Step(core, level, fi, NULL, u);

            /* Update local processor norm. */
            ii = fi-ilower;
            _braid_StepStatusInit(ta[ii-1], ta[ii], tol, iter, level, nrefine, gupper, status);
            _braid_CoreFcn(core, full_rnorm_res)(app, u, r, status);
            _braid_CoreFcn(core, spatialnorm)(app, r, &rnorm_temp);
            if(tnorm == 1)       /* one-norm */
            {
                rnorm += rnorm_temp;
            }
            else if(tnorm == 3)  /* inf-norm */
            {
                rnorm = (((rnorm_temp) > (rnorm)) ? (rnorm_temp) : (rnorm));
            }
            else                 /* default two-norm */
            {
                rnorm += (rnorm_temp*rnorm_temp);
            }

            /* Communicate w/ neighbor nodes. */
            send_handle = _braid_GridElt(grids[level], send_handle);
            send_index  = _braid_GridElt(grids[level], send_index);
            if (fi == send_index)
            {
                /* Post send to neighbor processor */
                _braid_CommSendInit(core, level, fi, u, &send_handle);
                _braid_GridElt(grids[level], send_index)  = -1;
                _braid_GridElt(grids[level], send_handle) = send_handle;
            }
            _braid_CoreFcn(core, free)(app, r);
        }
        /* Residual from C-point. */
        if (ci > 0)
        {
            /* Update local processor norm. */
            ii = ci-ilower;
            _braid_StepStatusInit(ta[ii-1], ta[ii], tol, iter, level, nrefine, gupper, status);
            _braid_UGetVector(core, level, ci, &r);
            _braid_CoreFcn(core, full_rnorm_res)(app, r, u, status);
            _braid_CoreFcn(core, spatialnorm)(app, u, &rnorm_temp);

            if(tnorm == 1)       /* one-norm */
            {
                rnorm += rnorm_temp;
            }
            else if(tnorm == 3)  /* inf-norm */
            {
                rnorm = (((rnorm_temp) > (rnorm)) ? (rnorm_temp) : (rnorm));
            }
            else                 /* default two-norm */
            {
                rnorm += (rnorm_temp*rnorm_temp);
            }
            _braid_CoreFcn(core, free)(app, r);
        }

        if ((flo <= fhi) || (ci > 0))
        {
            _braid_CoreFcn(core, free)(app, u);
        }
    }

    _braid_UCommWait(core, level);

    /* Compute global residual norm. */
    if(tnorm == 1)       /* one-norm reduction */
    {
        MPI_Allreduce(&rnorm, &global_rnorm, 1, MPI_DOUBLE, MPI_SUM, comm);
    }
    else if(tnorm == 3)  /* inf-norm reduction */
    {
        MPI_Allreduce(&rnorm, &global_rnorm, 1, MPI_DOUBLE, MPI_MAX, comm);
    }
    else                 /* default two-norm reduction */
    {
        MPI_Allreduce(&rnorm, &global_rnorm, 1, MPI_DOUBLE, MPI_SUM, comm);
        global_rnorm = sqrt(global_rnorm);
    }

    *return_rnorm = global_rnorm;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Do nu sweeps of F-then-C relaxation
 *----------------------------------------------------------------------------*/

braid_Int
_braid_FCRelax(braid_Core  core,
               braid_Int   level)
{
    braid_App       app      = _braid_CoreElt(core, app);
    braid_Int      *nrels    = _braid_CoreElt(core, nrels);
    _braid_Grid   **grids    = _braid_CoreElt(core, grids);
    braid_Int       ncpoints = _braid_GridElt(grids[level], ncpoints);

    braid_Vector    u;
    braid_Int       flo, fhi, fi, ci;
    braid_Int       nu, nrelax, interval;

    nrelax  = nrels[level];

    for (nu = 0; nu < nrelax; nu++)
    {
        _braid_UCommInit(core, level);

        /* Start from the right-most interval */
        for (interval = ncpoints; interval > -1; interval--)
        {
            _braid_GetInterval(core, level, interval, &flo, &fhi, &ci);

            if (flo <= fhi)
            {
                _braid_UGetVector(core, level, flo-1, &u);
            }
            else if (ci > 0)
            {
                _braid_UGetVector(core, level, ci-1, &u);
            }

            /* F-relaxation */
            for (fi = flo; fi <= fhi; fi++)
            {
                _braid_Step(core, level, fi, NULL, u);
                _braid_USetVector(core, level, fi, u, 0);
            }

            /* C-relaxation */
            if (ci > 0)
            {
                _braid_Step(core, level, ci, NULL, u);
                _braid_USetVector(core, level, ci, u, 1);
            }

            /* if ((flo <= fhi) && (interval == ncpoints)) */
            if ((flo <= fhi) && !(ci > 0))
            {
                _braid_CoreFcn(core, free)(app, u);
            }
        }
        _braid_UCommWait(core, level);
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * F-Relax on level and restrict to level+1
 *
 * At the bottom of FRestrict(), the coarse-grid right-hand side is computed and
 * stored in c_fa[].  The result stored in c_fa[] must be considered carefully
 * to maintain a fixed point nature of the algorithm.  The FAS coarse-grid
 * equation is
 *
 *   A_c(u_c) = R(f - A(u)) + A_c(R(u))
 *
 * where R() is restriction, and A() and A_c() are the fine and coarse
 * operators.  We desire our algorithm to be a fixed-point method, in that the
 * exact solution (from sequential time stepping) should yield a zero initial
 * residual and that the residual should stay zero as Braid iterates.  This is
 * actually a subtle point.  In this case, the solution u_c should be equivalent
 * to the solution u.  Since R(f - A(u)) = 0, this implies that
 *
 *   A_c(u_c) = A_c(R(u))
 *
 * This further implies that the initial guess provided to the implicit solve
 * routines in A_c() must be consistent on the right an left hand sides.
 *
 * The function (@ref _braid_GetUInit()) provides this functionality by
 * encapsulating all decisions about how to set this initial guess (called
 * ustop) The ustop values for A_c(R(u)) are set in (@ _braid_Residual) and for
 * A_c(u_c), they are set in (@ _braid_step).
 *
 * storage == -2 implies that (@ref _braid_GetUInit()) will always use the
 * previous time step value as the initial guess, even if you have better
 * information, which is the case at C-points on the fine grid, and at all
 * points on coarse-levels.
 *
 * storage == -1 implies that (@ref _braid_GetUInit()) will always use the best
 * information.  The value at the previous time step is used as the initial
 * guess only at F-points on the fine-grid.  The va[] values provide all initial
 * guess at F- and C-points on coarse-grids.
 *
 * storage == 0 is the same as -1, except that storage also exists at fine-grid
 * F-points.
 *----------------------------------------------------------------------------*/

braid_Int
_braid_FRestrict(braid_Core   core,
                 braid_Int    level)
{
    MPI_Comm             comm        = _braid_CoreElt(core, comm);
    braid_App            app         = _braid_CoreElt(core, app);
    _braid_Grid        **grids       = _braid_CoreElt(core, grids);
    braid_AccessStatus   astatus     = _braid_CoreElt(core, astatus);
    braid_Int            iter        = _braid_CoreElt(core, niter);
    braid_Int            print_level = _braid_CoreElt(core, print_level);
    braid_Int            access_level= _braid_CoreElt(core, access_level);
    braid_Int            tnorm       = _braid_CoreElt(core, tnorm);
    braid_Real          *tnorm_a     = _braid_CoreElt(core, tnorm_a);
    braid_Int            nrefine     = _braid_CoreElt(core, nrefine);
    braid_Int            gupper      = _braid_CoreElt(core, gupper);
    braid_Int            cfactor     = _braid_GridElt(grids[level], cfactor);
    braid_Int            ncpoints    = _braid_GridElt(grids[level], ncpoints);
    braid_Real          *ta          = _braid_GridElt(grids[level], ta);
    braid_Int            f_ilower    = _braid_GridElt(grids[level], ilower);
    _braid_CommHandle   *recv_handle = NULL;
    _braid_CommHandle   *send_handle = NULL;

    braid_Int            c_level, c_ilower, c_iupper, c_index, c_i, c_ii;
    braid_Vector         c_u, *c_va, *c_fa;

    braid_Vector         u, r;
    braid_Int            interval, flo, fhi, fi, ci;
    braid_Real           rnorm, grnorm, rnorm_temp, rnm;

    c_level  = level+1;
    c_ilower = _braid_GridElt(grids[c_level], ilower);
    c_iupper = _braid_GridElt(grids[c_level], iupper);
    c_va     = _braid_GridElt(grids[c_level], va);
    c_fa     = _braid_GridElt(grids[c_level], fa);

    rnorm = 0.0;

    _braid_UCommInit(core, level);

    /* Start from the right-most interval.
     *
     * Do an F-relax and then a C-relax.  These relaxations are needed to compute
     * the residual, which is needed for the coarse-grid right-hand-side and for
     * convergence checking on the finest grid.  This loop updates va and fa. */
    for (interval = ncpoints; interval > -1; interval--)
    {
        _braid_GetInterval(core, level, interval, &flo, &fhi, &ci);

        if (flo <= fhi)
        {
            _braid_UGetVector(core, level, flo-1, &r);
        }
        else if (ci > 0)
        {
            _braid_UGetVector(core, level, ci-1, &r);
        }

        /* F-relaxation */
        _braid_GetRNorm(core, -1, &rnm);
        for (fi = flo; fi <= fhi; fi++)
        {
            _braid_Step(core, level, fi, NULL, r);
            _braid_USetVector(core, level, fi, r, 0);

            /* Allow user to process current vector, note that r here is
             * temporarily holding the state vector */
            if( (access_level >= 2) && (level == 0) )
            {
                _braid_AccessStatusInit(ta[fi-f_ilower], rnm, iter, level, nrefine, gupper,
                                        0, 0, astatus);
                _braid_AccessVector(core, astatus, r);
            }
        }

        /* Allow user to process current C-point */
        if( (access_level>= 2) && (level == 0) && (ci > -1) )
        {
            _braid_UGetVectorRef(core, level, ci, &u);
            _braid_AccessStatusInit(ta[ci-f_ilower], rnm, iter, level, nrefine, gupper,
                                    0, 0, astatus);
            _braid_AccessVector(core, astatus, u);
        }

        /* Compute residual and restrict */
        if (ci > 0)
        {
            /* Compute FAS residual */
            _braid_UGetVectorRef(core, level, ci, &u);
            _braid_FASResidual(core, level, ci, u, r);

            /* Compute rnorm (only on level 0) */
            if (level == 0)
            {
                _braid_CoreFcn(core, spatialnorm)(app, r, &rnorm_temp);
                tnorm_a[interval] = rnorm_temp;       /* inf-norm uses tnorm_a */
                if(tnorm == 1)
                {
                    rnorm += rnorm_temp;               /* one-norm combination */
                }
                else if(tnorm == 2)
                {
                    rnorm += (rnorm_temp*rnorm_temp);  /* two-norm combination */
                }
            }

            /* Restrict u and residual, coarsening in space if needed */
            _braid_MapFineToCoarse(ci, cfactor, c_index);
            _braid_Coarsen(core, c_level, ci, c_index, u, &c_va[c_index-c_ilower]);
            _braid_Coarsen(core, c_level, ci, c_index, r, &c_fa[c_index-c_ilower]);
        }
        else if (ci == 0)
        {
            /* Restrict initial condition, coarsening in space if needed */
            _braid_UGetVectorRef(core, level, 0, &u);
            _braid_Coarsen(core, c_level, 0, 0, u, &c_va[0]);
        }

        if ((flo <= fhi) || (ci > 0))
        {
            _braid_CoreFcn(core, free)(app, r);
        }
    }
    _braid_UCommWait(core, level);

    /* If debug printing, print out tnorm_a for this interval. This
     * should show the serial propagation of the exact solution */
    if ((print_level >= 2) && (level == 0) )
    {
        _braid_PrintSpatialNorms(core, tnorm_a, ncpoints);
    }

    /* Compute rnorm (only on level 0) */
    if (level == 0)
    {
        if(tnorm == 1)          /* one-norm reduction */
        {
            MPI_Allreduce(&rnorm, &grnorm, 1, braid_MPI_REAL, MPI_SUM, comm);
        }
        else if(tnorm == 3)     /* inf-norm reduction */
        {
            _braid_Max(tnorm_a, ncpoints, &rnorm);
            MPI_Allreduce(&rnorm, &grnorm, 1, braid_MPI_REAL, MPI_MAX, comm);
        }
        else                    /* default two-norm reduction */
        {
            MPI_Allreduce(&rnorm, &grnorm, 1, braid_MPI_REAL, MPI_SUM, comm);
            grnorm = sqrt(grnorm);
        }

        /* Store new rnorm */
        _braid_SetRNorm(core, -1, grnorm);
    }

    /* Now apply coarse residual to update fa values */

    /* Set initial guess on coarse level */
    _braid_InitGuess(core, c_level);

    /* Initialize update of c_va[-1] boundary */
    if (c_ilower <= c_iupper)
    {
        _braid_CommRecvInit(core, c_level, c_ilower-1, &c_va[-1],
                            &recv_handle);
        _braid_CommSendInit(core, c_level, c_iupper, c_va[c_iupper-c_ilower],
                            &send_handle);
    }

    /* Start with rightmost point */
    for (c_i = c_iupper; c_i >= c_ilower; c_i--)
    {
        if (c_i > 0)
        {
            c_ii = c_i - c_ilower;
            if (c_ii == 0)
            {
                /* Finalize update of c_va[-1] */
                _braid_CommWait(core, &recv_handle);
            }
            _braid_CoreFcn(core, clone)(app, c_va[c_ii-1], &c_u);
            _braid_Residual(core, c_level, c_i, c_va[c_ii], c_u);
            _braid_CoreFcn(core, sum)(app, 1.0, c_u, 1.0, c_fa[c_ii]);
            _braid_CoreFcn(core, free)(app, c_u);
        }
    }
    _braid_CommWait(core, &send_handle);

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * F-Relax on level and interpolate to level-1
 *----------------------------------------------------------------------------*/

braid_Int
_braid_FInterp(braid_Core  core,
               braid_Int   level)
{
    braid_App          app          = _braid_CoreElt(core, app);
    _braid_Grid      **grids        = _braid_CoreElt(core, grids);
    braid_AccessStatus astatus      = _braid_CoreElt(core, astatus);
    braid_Int          iter         = _braid_CoreElt(core, niter);
    braid_Int          access_level = _braid_CoreElt(core, access_level);
    braid_Int          nrefine      = _braid_CoreElt(core, nrefine);
    braid_Int          gupper       = _braid_CoreElt(core, gupper);
    braid_Int          ilower       = _braid_GridElt(grids[level], ilower);
    braid_Int          ncpoints     = _braid_GridElt(grids[level], ncpoints);
    braid_Vector      *va           = _braid_GridElt(grids[level], va);
    braid_Real        *ta           = _braid_GridElt(grids[level], ta);

    braid_Real         rnorm;
    braid_Int          f_level, f_cfactor, f_index;
    braid_Vector       f_u, f_e;

    braid_Vector       u, e;
    braid_Int          flo, fhi, fi, ci;
    braid_Int          interval;

    f_level   = level-1;
    f_cfactor = _braid_GridElt(grids[f_level], cfactor);

    _braid_GetRNorm(core, -1, &rnorm);

    _braid_UCommInitF(core, level);

    /**
     * Start from the right-most interval
     *
     * First, generate the coarse-grid F-points through F-relaxation and
     * interpolate them to the fine grid, where they are C-points.  Second,
     * interpolate the coarse-grid C-points to the fine-grid.  The user-defined
     * spatial refinement (if set) is also called.
     **/
    for (interval = ncpoints; interval > -1; interval--)
    {
        _braid_GetInterval(core, level, interval, &flo, &fhi, &ci);

        /* Relax and interpolate F-points, refining in space if needed */
        if (flo <= fhi)
        {
            _braid_UGetVector(core, level, flo-1, &u);
        }
        for (fi = flo; fi <= fhi; fi++)
        {
            _braid_Step(core, level, fi, NULL, u);
            _braid_USetVector(core, level, fi, u, 0);
            /* Allow user to process current vector
             * We consider this iter+1, because we are on an up-cycle.*/
            if( (access_level >= 2) )
            {
                _braid_AccessStatusInit(ta[fi-ilower], rnorm, iter+1, level, nrefine, gupper,
                                        0, 0, astatus);
                _braid_AccessVector(core, astatus, u);
            }
            e = va[fi-ilower];
            _braid_CoreFcn(core, sum)(app, 1.0, u, -1.0, e);
            _braid_MapCoarseToFine(fi, f_cfactor, f_index);
            _braid_Refine(core, f_level, f_index, fi, e, &f_e);
            _braid_UGetVectorRef(core, f_level, f_index, &f_u);
            _braid_CoreFcn(core, sum)(app, 1.0, f_e, 1.0, f_u);
            _braid_USetVectorRef(core, f_level, f_index, f_u);
            _braid_CoreFcn(core, free)(app, f_e);
        }
        if (flo <= fhi)
        {
            _braid_CoreFcn(core, free)(app, u);
        }

        /* Interpolate C-points, refining in space if needed */
        if (ci > 0)
        {
            _braid_UGetVectorRef(core, level, ci, &u);
            /* Allow user to process current C-point
             * We consider this iter+1, because we are on an up-cycle.*/
            if( (access_level >= 2) )
            {
                _braid_AccessStatusInit(ta[ci-ilower], rnorm, iter+1, level, nrefine, gupper,
                                        0, 0, astatus);
                _braid_AccessVector(core, astatus, u);
            }
            e = va[ci-ilower];
            _braid_CoreFcn(core, sum)(app, 1.0, u, -1.0, e);
            _braid_MapCoarseToFine(ci, f_cfactor, f_index);
            _braid_Refine(core, f_level, f_index, ci, e, &f_e);
            _braid_UGetVectorRef(core, f_level, f_index, &f_u);
            _braid_CoreFcn(core, sum)(app, 1.0, f_e, 1.0, f_u);
            _braid_USetVectorRef(core, f_level, f_index, f_u);
            _braid_CoreFcn(core, free)(app, f_e);
        }
    }

    _braid_UCommWait(core, level);

    /* Clean up */
    _braid_GridClean(core, grids[level]);

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Refine in Space at every point on processor if r_space is set
 *----------------------------------------------------------------------------*/

braid_Int
_braid_FRefineSpace(braid_Core   core,
                    braid_Int   *refined_ptr)
{

    MPI_Comm      comm    = _braid_CoreElt(core, comm);
    _braid_Grid **grids   = _braid_CoreElt(core, grids);

    braid_Int     r_space = _braid_CoreElt(core, r_space);
    braid_Int     ilower  = _braid_GridElt(grids[0], ilower);
    braid_Int     iupper  = _braid_GridElt(grids[0], iupper);
    braid_Real   *ta      = _braid_GridElt(grids[0], ta);

    braid_Int     global_r_space;
    braid_Int     i, ii;
    braid_Vector  c_vec, f_vec;

    if ( _braid_CoreElt(core, scoarsen) != NULL )
    {

        if ( r_space > 0 )
        {
            for ( i = ilower; i <= iupper; i++ )
            {
                ii = i-ilower;
                _braid_UGetVectorRef(core, 0, i , &c_vec);

                if ( c_vec != NULL )
                {
                    _braid_RefineBasic(core, -1, &ta[ii], &ta[ii], c_vec, &f_vec);
                    _braid_USetVectorRef(core, 0, i, f_vec);
                }
            }
        }

        /* Check if any refinment was completed globally. If true then refine the
           initial time point if not done already, increase nrefine, and return 2 */
        MPI_Allreduce(&r_space, &global_r_space, 1, braid_MPI_INT, MPI_MAX, comm);
        if (global_r_space > 0)
        {
            /* Need to make sure to refine the first point. If it is a lone C point
               on a processor r_space then can never be set for that processor */
            if ( ilower == 0 && r_space == 0 )
            {
                _braid_UGetVectorRef(core, 0, 0, &c_vec);
                _braid_RefineBasic(core, -1, &ta[0], &ta[0], c_vec, &f_vec);
                _braid_USetVectorRef(core, 0, 0, f_vec);
            }

            *refined_ptr = 2;
            _braid_CoreElt(core, nrefine) += 1;
        }
        else
        {
            *refined_ptr = 0;
        }
    }
    else
    {
        *refined_ptr = 0;
    }

    /* Reset r_space */
    _braid_CoreElt(core, r_space) = 0;
    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Create a new fine grid (level 0) and corresponding grid hierarchy by refining
 * the current fine grid based on user-provided refinement factors.  Return the
 * boolean 'refined_ptr' to indicate whether grid refinement was actually done.
 * To simplify the algorithm, refinement factors are automatically capped to be
 * no greater than the coarsening factor (for level 0).  The grid data is also
 * redistributed to achieve good load balance in the temporal dimension.  If the
 * refinement factor is 1 in each time interval, no refinement is done.
 *
 * This routine is somewhat complex, but an attempt was made to use consistent
 * terminology throughout.  We refer to the initial level 0 grid as the "coarse"
 * grid and the new level 0 grid as the "fine" grid.  The routine starts with
 * the coarse grid and creates an intermediate "refined" grid that is co-located
 * with the parent coarse grid data distribution.  On this refined grid, a
 * number of things are constructed: the mapping between the coarse and fine
 * indexes; the new fine time values; and the injected and (possibly) spatially
 * refined coarse u-vectors.  This data is then redistributed by first sending
 * the mapping and time value information to the appropriate processors (the
 * receiving side polls for arbitrary messages until each fine index is
 * accounted for).  The u-vector values are communicated in a second phase
 * (without polling).  Finally, a new hierarchy is created and the fine grid
 * values are initialized by integrating the communicated u-vector values to the
 * next C-point to the right.  Note that in the case of C-point storage, some
 * u-vector values may not need to be communicated.
 *
 * The variable names use certain conventions as well.  No prefix (usually)
 * indicates a coarse variable, and the prefixes 'r_' and 'f_' indicate data on
 * the "refined" and "fine" grids, respectively.  Characters 'c', 'f', and 'a'
 * usually mean "coarse", "fine", and "array".  Indexes 'i', 'r_i', and 'f_i'
 * refer to global indexes, while 'ii', 'r_ii', and 'f_ii' are local indexes.
 *
 * Here are some important variables along with a brief description (they are
 * illustrated further in the example below):
 *
 *   gupper, f_gupper - global upper index for coarse and fine grids
 *   ilower,   iupper,   npoints   - extents and size of local coarse interval
 *   r_ilower, r_iupper, r_npoints - extents and size of local refined interval
 *   f_ilower, f_iupper, f_npoints - extents and size of local fine interval
 *
 *   r_ca - index map from fine to coarse on the refined grid  (size 'r_npoints')
 *   r_ta - time values on the refined grid                    (size 'r_npoints')
 *   r_fa - index map from coarse to fine on the refined grid  (size 'npoints+1')
 *          (note the extra value)
 *   f_ca - index map from fine to coarse on the fine grid     (size 'f_npoints')
 *
 *   send_ua - array of u-vectors to send to new processors    (size 'npoints')
 *   recv_ua - array of u-vectors received from old processors (size 'f_npoints')
 *
 * Example: Some processor p owns the coarse interval, ilower = 29, iupper = 33.
 * The coarsening factor is 3 and 'rfactors' indicates the refinement factor for
 * the coarse interval to the left.  From this, an intermediate refined grid and
 * a final fine grid are formed as described above.
 *
 *   Coarse            |-----------c-----------|-----------|-----------c
 *     Grid           29          30          31          32          33
 *
 * rfactors            3           1           2           3           2
 *
 *  Refined ---c---|---|-----------c-----|-----|---c---|---|-----c-----|
 *     Grid   57  58  59          60    61    62  63  64  65    66    67
 *
 * r_ilower   57
 *     r_ca   -1  -1  29          30    -1    31  -1  -1  32    -1    33
 *     r_ta    *   *   *           *     *     *   *   *   *     *     *
 *     r_fa           59          60          62          65          67          70
 *  send_ua            *           *           *           *           *
 *
 *     Fine      |-----|---c---|---|-----c-----|---|---c---|-----|-----c
 *     Grid     61    62  63  64  65    66    67  68  69  70    71    72
 *
 * f_ilower     61
 *     f_ca     -1    31  -1  -1  32    -1    33  -1  -1  34    -1    35
 *  f_first           62
 *   f_next                                                                       74
 *  recv_ua      0     *   0   0   *     0     *   0   0   *     0     *
 *
 * When storing C-pts only, data from coarse indices 29 and 34 are not needed,
 * so we have the following differences from above:
 *     r_ca   -1  -1  -1          30    -1    31  -1  -1  32    -1    33
 *  send_ua            0           *           *           *           *
 *     f_ca     -1    31  -1  -1  32    -1    33  -1  -1  -1    -1    35
 *  recv_ua      0     *   0   0   *     0     *   0   0   0     0     *
 *
 *----------------------------------------------------------------------------*/

braid_Int
_braid_FRefine(braid_Core   core,
               braid_Int   *refined_ptr)
{
    MPI_Comm           comm            = _braid_CoreElt(core, comm);
    braid_App          app             = _braid_CoreElt(core, app);
    braid_Int          iter            = _braid_CoreElt(core, niter);
    braid_Int          refine          = _braid_CoreElt(core, refine);
    braid_Int         *rfactors        = _braid_CoreElt(core, rfactors);
    braid_Int         *wfactors        = _braid_CoreElt(core, wfactors);
    braid_Int          nrefine         = _braid_CoreElt(core, nrefine);
    braid_Int          max_refinements = _braid_CoreElt(core, max_refinements);
    braid_Int          tpoints_cutoff  = _braid_CoreElt(core, tpoints_cutoff);
    braid_AccessStatus astatus         = _braid_CoreElt(core, astatus);
    braid_BufferStatus bstatus         = _braid_CoreElt(core, bstatus);
    braid_Int          access_level    = _braid_CoreElt(core, access_level);
    _braid_Grid      **grids           = _braid_CoreElt(core, grids);
    braid_Int          ncpoints        = _braid_GridElt(grids[0], ncpoints);

    braid_Real     rnorm;

    /* Use prefix 'r_' for the refined version of the current grid level 0, and
     * use prefix 'f_' for the fine grid in the new distribution. */

    braid_Int      npoints, ilower, iupper, gupper, i, j, ii;
    braid_Int      r_npoints, r_ilower, r_iupper, r_i, r_ii;
    braid_Int      f_npoints, f_ilower, f_iupper, f_gupper, f_i, f_j, f_ii;
    braid_Int     *r_ca, *r_fa, *f_ca, f_first, f_next, next;
    braid_Real    *ta, *r_ta, *f_ta;

    braid_Vector  *send_ua, *recv_ua, u;
    braid_Int     *send_procs, *recv_procs, *send_unums, *recv_unums, *iptr;
    braid_Real    *send_buffer, *recv_buffer, **send_buffers, **recv_buffers, *bptr;
    void          *buffer;
    braid_Int      send_size, recv_size, *send_sizes, size, isize, max_usize;
    braid_Int      ncomms, nsends, nrecvs, nreceived, nprocs, proc, prevproc;
    braid_Int      unum, send_msg, recv_msg;
    MPI_Request   *requests, request;
    MPI_Status    *statuses, status;

    _braid_Grid   *f_grid;
    braid_Int      cfactor, rfactor, m, interval, flo, fhi, fi, ci, f_hi, f_ci;

#if DEBUG
    braid_Int  myproc;
    /*cfactor = 6;*/ /* RDF HACKED TEST */
    MPI_Comm_rank(comm, &myproc);
#endif

    /* Only refine if refinement is turned on */
    if(refine == 0)
    {
        *refined_ptr = 0;
        return _braid_error_flag;
    }

    gupper  = _braid_CoreElt(core, gupper);
    ilower  = _braid_GridElt(grids[0], ilower);
    iupper  = _braid_GridElt(grids[0], iupper);
    npoints = iupper - ilower + 1;

    /* If reached max refinements or have too many time points, stop refining */
    //TODO Need to make it so load balencing can still occur here.
    if( !((nrefine < max_refinements) && (gupper < tpoints_cutoff)) )
    {
        _braid_CoreElt(core, refine)   = 0;
        _braid_CoreElt(core, rstopped) = iter;
        _braid_FRefineSpace(core, refined_ptr);
        return _braid_error_flag;
    }

    /*-----------------------------------------------------------------------*/
    /* 1. Compute f_gupper and the local interval extents for both the refined
     * and fine grids.  The local refined interval contains the fine grid points
     * underlying the coarse interval (ilower-1, iupper]. */

    /* Compute f_gupper and r_npoints */
    _braid_GetCFactor(core, 0, &cfactor);
    r_npoints = 0;
    for (i = ilower; i <= iupper; i++)
    {
        /* First modify rfactor to be no greater than cfactor (required) */
        ii = i - ilower;
        if (rfactors[ii] < 1)
        {
            _braid_Error(braid_ERROR_GENERIC, "Refinement factor smaller than one");
            rfactors[ii] = 1;
        }
        rfactors[ii] = _braid_min(rfactors[ii], cfactor);
        r_npoints += rfactors[i-ilower];
    }
    MPI_Allreduce(&r_npoints, &f_gupper, 1, braid_MPI_INT, MPI_SUM, comm);
    f_gupper--;

#if DEBUG
    for (i = ilower; i <= iupper; i++)
    {
        ii = i - ilower;
        printf("%d %d: 0 rfactor %d = %d\n", FRefine_count, myproc, i, rfactors[ii]);
    }
#endif

    /* Check to see if we need to refine, and return if not */
    //TODO load balencing in here too
    if (f_gupper == gupper)
    {
        _braid_FRefineSpace(core, refined_ptr);
        return _braid_error_flag;
    }
    else
    {
        _braid_CoreElt(core, r_space) = 0;
    }

    /* Compute r_ilower and r_iupper */
    MPI_Scan(&r_npoints, &r_iupper, 1, braid_MPI_INT, MPI_SUM, comm);
    r_ilower = r_iupper - r_npoints;
    r_iupper = r_iupper - 1;

    /* Compute f_ilower, f_iupper, and f_npoints for the final distribution */
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &proc);


    /*-----------------------------------------------------------------------*/
    /* 2. On the refined grid, compute the mapping between coarse and fine
     * indexes (r_ca, r_fa) and the fine time values (r_ta). */

    r_ca = _braid_CTAlloc(braid_Int,  r_npoints);
    r_ta = _braid_CTAlloc(braid_Real, r_npoints);
    r_fa = _braid_CTAlloc(braid_Int,  npoints+1);
    ta = _braid_GridElt(grids[0], ta);
    r_ii = 0;
    for (i = (ilower-1); i < iupper; i++)
    {
        ii = i-ilower;
        rfactor = rfactors[ii+1];

        for (j = 1; j <= rfactor; j++)
        {
            if (j < rfactor)
            {
                r_ca[r_ii] = -1;
                /* This works because we have ta[-1] */
                r_ta[r_ii] = ta[ii] + (((braid_Real)j)/rfactor)*(ta[ii+1]-ta[ii]);
            }
            else
            {
                r_ca[r_ii] = i+1;
                r_ta[r_ii] = ta[ii+1];
                r_fa[ii+1] = r_ilower + r_ii;
            }

            r_ii++;
        }
    }

    /* Get the next r_fa value to my right */
    ncomms = 2; /* Upper bound */
    requests = _braid_CTAlloc(MPI_Request, ncomms);
    statuses = _braid_CTAlloc(MPI_Status,  ncomms);
    ncomms = 0;
    if (npoints > 0)
    {
        /* Post r_fa receive */
        r_fa[npoints] = f_gupper+1;
        if (iupper < gupper)
        {
            MPI_Irecv(&r_fa[npoints], 1, braid_MPI_INT, MPI_ANY_SOURCE, 2, comm,
                      &requests[ncomms++]);
        }

        /* Post r_fa send */
        if (ilower > 0)
        {

            prevproc = _braid_GridElt(grids[0], recv_proc );
            //_braid_GetBlockDistProc((gupper+1), nprocs, (ilower-1), &prevproc);           
            MPI_Isend(&r_fa[0], 1, braid_MPI_INT, prevproc, 2, comm,
                      &requests[ncomms++]);
        }
    }
    MPI_Waitall(ncomms, requests, statuses);
    _braid_TFree(requests);
    _braid_TFree(statuses);

    /* If storing only C-points on the fine grid, modify r_ca to mark only those
     * coarse points that need to be sent to initialize the C-points */
    if (_braid_CoreElt(core, storage) != 0)
    {
        for (ii = 0; ii < npoints; ii++)
        {
            /* If the index for the next coarse point is not larger than the index
             * for the next C-point, then the coarse point is not needed */
            if ( !(r_fa[ii+1] > _braid_NextCPoint(r_fa[ii], cfactor)) )
            {
                r_ii = r_fa[ii] - r_ilower;
                r_ca[r_ii] = -1;
            }
        }
    }

    //2a -- Do load balencing here now that we have r_ca -- which we need to fill in the wfactors
    // and also make the new grid .
    if ( _braid_CoreElt( core, lbalence ) ) 
    {
        braid_Int *new_wfactors = _braid_CTAlloc( braid_Int , r_iupper - r_ilower + 1 );
        braid_Int jj = 0;
        i = r_ilower;
        while ( i <= r_iupper )
        {
            if ( r_ca[i] > 0 )
            {
                for ( ; jj >= 0 ; jj-- )
                    new_wfactors[ i - jj - r_ilower] = wfactors[ r_ca[i-r_ilower] - ilower ] ;
                jj = 0;
            }
            else
                jj++;
            i++;
        }
        _braid_LoadBalence( core, new_wfactors, r_ilower, r_iupper, f_gupper, &f_ilower, &f_iupper );
        free( new_wfactors );
    }
    else
        _braid_GetBlockDistInterval((f_gupper+1), nprocs, proc, &f_ilower, &f_iupper);
    
    
    //TODO -- Not 100 percent sure this works if last point is an F point 
    //And a cfactor problem if cfactor changes across a grid! n
    braid_Int max_levels = _braid_CoreElt( core, max_levels );
    braid_Int min_coarse = _braid_CoreElt( core, min_coarse );
    braid_Int new_gupper = f_gupper;
    braid_Int g_lower = 0;
    braid_Int new_level = 0; 
    while (  new_level + 1 < max_levels ) 
    {
         _braid_ProjectInterval( g_lower, new_gupper, 0, cfactor, &g_lower, &new_gupper );
         if ( new_gupper - g_lower > min_coarse )
         {
              _braid_MapFineToCoarse( new_gupper, cfactor, new_gupper );
              new_level++; 
         }
    }     
    new_level++;
    
    printf( "  calculated %d new levels \n " , new_level );

    braid_Int *index_map_send = _braid_CTAlloc( braid_Int, r_iupper-r_ilower+1);
    braid_Int *index_map_recv = _braid_CTAlloc( braid_Int, f_iupper-f_ilower+1);
    braid_Int *new_send_procs = _braid_CTAlloc( braid_Int, new_level );
    braid_Int *new_recv_procs = _braid_CTAlloc( braid_Int, new_level );
    
    if (0)
    {
        _braid_assume_partition( core, new_level, &r_ilower, &r_iupper, &f_ilower, &f_iupper, &gupper,
                                 &new_send_procs, &new_recv_procs, &index_map_send, &index_map_recv );
    }

    /* Initialize the new fine grid */
    f_npoints = f_iupper - f_ilower + 1;
    _braid_GridInit(core, 0, f_ilower, f_iupper, &f_grid);

    /*-----------------------------------------------------------------------*/
    /* 3. Send the index mapping and time value information (r_ca, r_ta) to the
     * appropriate processors to build index mapping and time value information
     * for the fine grid (f_ca, f_ta).  Also compute f_first and f_next. */

    /* Post f_next receive */
    f_next = -1;
    if (f_npoints > 0)
    {
        f_next = f_gupper+1;
        if (f_iupper < f_gupper)
        {
            MPI_Irecv(&f_next, 1, braid_MPI_INT, MPI_ANY_SOURCE, 3, comm, &request);
        }
    }



    /* Compute send information and send f_next info */
    size = 2*sizeof(braid_Int);         /* size of two integers */
    _braid_NBytesToNReals(size, isize); /* convert to units of braid_Real */
    send_procs  = _braid_CTAlloc(braid_Int,  r_npoints);
    send_sizes  = _braid_CTAlloc(braid_Int,  r_npoints);
    send_buffer = _braid_CTAlloc(braid_Real, r_npoints*(1+isize+1));
    bptr = send_buffer;
    nsends = -1;

    // 
    if ( _braid_CoreElt( core, lbalence ) && r_npoints > 0 )  
         prevproc = index_map_send[0] -1; 
    else
        _braid_GetBlockDistProc((f_gupper+1), nprocs, (r_ilower-1), &prevproc);

    ii = 0;
    for (r_ii = 0; r_ii < r_npoints; r_ii++)
    {
        r_i = r_ilower + r_ii;

        if ( _braid_CoreElt( core, lbalence ) ) 
            proc = index_map_send[r_ii];
        else
            _braid_GetBlockDistProc((f_gupper+1), nprocs, r_i, &proc);

        if ((proc != prevproc) || (nsends < 0))
        {
            nsends++;
            send_procs[nsends] = proc;
            bptr++; /* leave room for size value */

            if ((proc != prevproc) && (prevproc > -1))
            {
                /* Send f_next info */
                MPI_Send(&r_fa[ii], 1, braid_MPI_INT, prevproc, 3, comm);
            }
            prevproc = proc;
        }
        send_sizes[nsends] += (isize+1);

        iptr = (braid_Int *) bptr;
        iptr[0] = r_i;
        iptr[1] = r_ca[r_ii];
        bptr += isize;
        bptr[0] = r_ta[r_ii];
        bptr++;

        /* Update f_next info */
        if (r_fa[ii] == r_i)
        {
            ii++;
        }
    }
    nsends++;

#if DEBUG
    for (m = 0; m < nsends; m++)
    {
        size = send_sizes[m];
        proc = send_procs[m];
        printf("%d %d: 1 send %d, proc = %d, send size = %d\n",
               FRefine_count, myproc, m, proc, size);
    }
#endif

    requests = _braid_CTAlloc(MPI_Request, nsends);
    statuses = _braid_CTAlloc(MPI_Status,  nsends);

    /* Post sends (do this first, since we will poll on receives) */
    bptr = send_buffer;
    for (m = 0; m < nsends; m++)
    {
        size = send_sizes[m];
        proc = send_procs[m];
        bptr[0] = (braid_Real) size; /* insert size at the beginning */
        MPI_Isend(bptr, (1+size), braid_MPI_REAL, proc, 4, comm, &requests[m]);
        bptr += (1+size);
    }

    /* Post receives */
    recv_size = f_npoints*(1+isize+1); /* max receive size */
    recv_buffer = _braid_CTAlloc(braid_Real, recv_size);
    f_ca = _braid_CTAlloc(braid_Int,  f_npoints);
    f_ta = _braid_GridElt(f_grid, ta);
    nreceived = 0;
    while (nreceived < f_npoints)
    {
        /* post receive from arbitrary process (should always get at least one) */
        bptr = recv_buffer;
        size = recv_size;
        MPI_Recv(bptr, (1+size), braid_MPI_REAL, MPI_ANY_SOURCE, 4, comm, &status);

        size = (braid_Int) bptr[0];
        bptr++;
        for (j = 0; j < size; j += (isize+1))
        {
            iptr = (braid_Int *) bptr;
            f_i = iptr[0];
            f_ii = f_i - f_ilower;
            f_ca[f_ii] = iptr[1];
            bptr += isize;
            f_ta[f_ii] = bptr[0];
            bptr++;
            {
#if DEBUG
                printf("%d %d: 1 f_i = %02d, f_ca = %2d, f_ta = %f  (recv %2d) \n",
                       FRefine_count, myproc, f_i, f_ca[f_ii], f_ta[f_ii], nreceived);
#endif
            }
            nreceived++;
        }

    }

    /* Finish sends and f_next receive */
    MPI_Waitall(nsends, requests, statuses);
    if (f_npoints > 0)
    {
        if (f_iupper < f_gupper)
        {
            MPI_Wait(&request, &status);
        }
    }

    /* Compute f_first */
    f_first = f_next;
    for (f_ii = 0; f_ii < f_npoints; f_ii++)
    {
        if (f_ca[f_ii] > -1)
        {
            f_first = f_ilower + f_ii;
            break;
        }
    }

#if DEBUG
    printf("%d %d: 2 f_first = %d, f_next = %d\n",
           FRefine_count, myproc, f_first, f_next);
#endif

    /* Free up some memory */
    _braid_TFree(requests);
    _braid_TFree(statuses);
    _braid_TFree(send_procs);
    _braid_TFree(send_sizes);
    _braid_TFree(send_buffer);
    _braid_TFree(recv_buffer);

    /*-----------------------------------------------------------------------*/
    /* 4. Build u-vectors on the fine grid (send_ua) by first integrating on the
     * coarse grid, then injecting and refining spatially.  Redistribute these
     * u-vectors to the fine grid (recv_ua). */

    send_ua = _braid_CTAlloc(braid_Vector, npoints);
    send_procs = _braid_CTAlloc(braid_Int, npoints);
    send_unums = _braid_CTAlloc(braid_Int, npoints);
    send_buffers = _braid_CTAlloc(braid_Real *, npoints);

    recv_ua = _braid_CTAlloc(braid_Vector, f_npoints);
    recv_procs = _braid_CTAlloc(braid_Int, f_npoints);
    recv_unums = _braid_CTAlloc(braid_Int, f_npoints);
    recv_buffers = _braid_CTAlloc(braid_Real *, f_npoints);

    _braid_GetRNorm(core, -1, &rnorm);

    _braid_UCommInitF(core, 0);

    /* Start from the right-most interval */
    for (interval = ncpoints; interval > -1; interval--)
    {
        _braid_GetInterval(core, 0, interval, &flo, &fhi, &ci);

        /* Integrate F-points and refine in space */
        if (flo <= fhi)
        {
            _braid_UGetVector(core, 0, flo-1, &u);
            for (fi = flo; fi <= fhi; fi++)
            {
                _braid_Step(core, 0, fi, NULL, u);
                _braid_USetVector(core, 0, fi, u, 0); /* needed for communication */

                /* Set send_ua */
                ii = fi - ilower;
                r_ii = r_fa[ii] - r_ilower;
                if (r_ca[r_ii] > -1)
                {
                    _braid_RefineBasic(core, -1, &r_ta[r_ii], &ta[ii], u, &send_ua[ii]);
                }

                /* Allow user to process current vector */
                if( (access_level >= 2) )
                {
                    _braid_AccessStatusInit(ta[ii], rnorm, iter, 0, nrefine, gupper, 0, 0, astatus);
                    _braid_AccessVector(core, astatus, u);
                }
            }
            _braid_CoreFcn(core, free)(app, u);
        }

        /* Refine C-points in space */
        if (ci > -1)
        {
            _braid_UGetVectorRef(core, 0, ci, &u);

            /* Set send_ua */
            ii = ci - ilower;
            r_ii = r_fa[ii] - r_ilower;
            if (r_ca[r_ii] > -1)
            {
                _braid_RefineBasic(core, -1, &r_ta[r_ii], &ta[ii], u, &send_ua[ii]);
            }

            /* Allow user to process current vector */
            if( (access_level >= 2) )
            {
                _braid_AccessStatusInit(ta[ii], rnorm, iter, 0, nrefine, gupper, 0, 0, astatus);
                _braid_AccessVector(core, astatus, u);
            }
        }
    }

    _braid_UCommWait(core, 0);

    /* Compute nsends, send_procs, and send_unums from send_ua array */
    nsends = -1;
    prevproc = -1;
    for (ii = 0; ii < npoints; ii++)
    {
        if (send_ua[ii] != NULL)
        {
            r_i = r_fa[ii];
            if ( _braid_CoreElt(core,lbalence) )  
               proc = index_map_send[ii];
            else
               _braid_GetBlockDistProc((f_gupper+1), nprocs, r_i, &proc);
            
            if (proc != prevproc)
            {
                nsends++;
                send_procs[nsends] = proc;
                send_unums[nsends] = 0;
                prevproc = proc;
            }
            send_unums[nsends]++;
        }
    }
    nsends++;

    /* Compute nrecvs, recv_procs, and recv_unums from f_ca array */
    nrecvs = -1;
    prevproc = -1;
    for (f_ii = 0; f_ii < f_npoints; f_ii++)
    {
        if (f_ca[f_ii] > -1)
        {
            i = f_ca[f_ii];
            if ( _braid_CoreElt( core, lbalence )) 
               proc = index_map_recv[f_ii];
            else
               _braid_GetBlockDistProc((gupper+1), nprocs, i, &proc);
            
            if (proc != prevproc)
            {
                nrecvs++;
                recv_procs[nrecvs] = proc;
                recv_unums[nrecvs] = 0;
                prevproc = proc;
            }
            recv_unums[nrecvs]++;
        }
    }
    nrecvs++;

    requests = _braid_CTAlloc(MPI_Request, (nsends+nrecvs));
    statuses = _braid_CTAlloc(MPI_Status,  (nsends+nrecvs));

    _braid_BufferStatusInit( 1, 0, bstatus );
    _braid_CoreFcn(core, bufsize)(app, &max_usize, bstatus); /* max buffer size */
    _braid_NBytesToNReals(max_usize, max_usize);

    /* Post u-vector receives */
    for (m = 0; m < nrecvs; m++)
    {
        unum = recv_unums[m]; /* Number of u-vectors being received */
        recv_size = unum*(1 + max_usize);
        recv_buffers[m] = _braid_CTAlloc(braid_Real, recv_size);
        MPI_Irecv(recv_buffers[m], recv_size, braid_MPI_REAL, recv_procs[m], 5, comm,
                  &requests[m]);

#if DEBUG
        proc = recv_procs[m];
        printf("%d %d: 2 recv %d, proc = %d, unum = %d, size = %d\n",
               FRefine_count, myproc, m, proc, unum, recv_size);
#endif
    }

    /* Post u-vector sends */
    ii = 0;
    for (m = 0; m < nsends; m++)
    {
        unum = send_unums[m]; /* Number of u-vectors being sent */
        send_size = unum*(1 + max_usize);
        send_buffers[m] = _braid_CTAlloc(braid_Real, send_size);
        send_size = 0; /* Recompute send_size and realloc buffer */
        bptr = send_buffers[m];
        while (unum > 0)
        {
            if (send_ua[ii] != NULL)
            {
                /* Pack u into buffer, adjust size, and put size into buffer */
                buffer = &bptr[1];
                _braid_CoreFcn(core, bufsize)(app, &size, bstatus);
                _braid_StatusElt( bstatus, size ) = size;
                _braid_CoreFcn(core, bufpack)(app, send_ua[ii], buffer, bstatus);
                size = _braid_StatusElt(bstatus, size);
                _braid_CoreFcn(core, free)(app, send_ua[ii]);
                _braid_NBytesToNReals(size, size);
                bptr[0] = (braid_Int) size; /* insert size at the beginning */
                bptr += (1+size);
                send_size += (1+size);
                unum--;
            }
            ii++;
        }
        send_buffers[m] = _braid_TReAlloc(send_buffers[m], braid_Real, send_size);
        MPI_Isend(send_buffers[m], send_size, braid_MPI_REAL, send_procs[m], 5, comm,
                  &requests[m + nrecvs]);

#if DEBUG
        unum = send_unums[m];
        proc = send_procs[m];
        printf("%d %d: 2 send %d, proc = %d, unum = %d, size = %d\n",
               FRefine_count, myproc, m, proc, unum, send_size);
#endif
    }

#if DEBUG
    printf("%d %d: 3\n", FRefine_count, myproc);
#endif

    /* Finish communication */
    MPI_Waitall((nsends+nrecvs), requests, statuses);

#if DEBUG
    printf("%d %d: 4\n", FRefine_count, myproc);
#endif

    /* Unpack u-vectors */
    f_ii = 0;
    for (m = 0; m < nrecvs; m++)
    {
        unum = recv_unums[m];
        bptr = recv_buffers[m];
        while (unum > 0)
        {
            if (f_ca[f_ii] > -1)
            {
                /* Unpack buffer into u-vector */
                buffer = &bptr[1];
                _braid_CoreFcn(core, bufunpack)(app, buffer, &recv_ua[f_ii], bstatus);
                size = (braid_Int) bptr[0];
                bptr += (1+size);
                unum--;
            }
            f_ii++;
        }
    }

    /* Free up some memory */
    _braid_TFree(send_ua);
    _braid_TFree(send_procs);
    _braid_TFree(send_unums);
    for (m = 0; m < nsends; m++)
    {
        _braid_TFree(send_buffers[m]);
    }
    _braid_TFree(send_buffers);
    _braid_TFree(recv_procs);
    _braid_TFree(recv_unums);
    for (m = 0; m < nrecvs; m++)
    {
        _braid_TFree(recv_buffers[m]);
    }
    _braid_TFree(recv_buffers);
    _braid_TFree(requests);
    _braid_TFree(statuses);
    _braid_TFree(r_ca);
    _braid_TFree(r_ta);
    _braid_TFree(r_fa);
    _braid_TFree(f_ca);
    {
        braid_Int  level, nlevels = _braid_CoreElt(core, nlevels);
        _braid_TFree(_braid_CoreElt(core, rfactors));
        _braid_TFree(_braid_CoreElt(core, tnorm_a));

        for (level = 0; level < nlevels; level++)
        {
            _braid_GridDestroy(core, grids[level]);
        }
    }

    /*-----------------------------------------------------------------------*/
    /* 5. Build the new fine grid hierarchy, then use recv_ua to populate the
     * initial values on grid level 0.  This is done by integrating values to the
     * next C-point the right.  Because we require that rfactor <= cfactor, each
     * C-point either has a corresponding value or has a value in the F-interval
     * immediately to the left that can be integrated.  Communication from the
     * left processor may still be needed. */

#if DEBUG
    printf("%d %d: 5\n", FRefine_count, myproc);
#endif

    /* Initialize new hierarchy */
    _braid_CoreElt(core, gupper)  = f_gupper;
    _braid_CoreElt(core, nrefine) += 1;
    /*braid_SetCFactor(core,  0, cfactor);*/ /* RDF HACKED TEST */
    _braid_InitHierarchy(core, f_grid, 1, new_recv_procs, new_send_procs);
    
    if ( new_level != _braid_CoreElt( core, nlevels ) )
       printf( " new levels calculation is wrong \n " );

    /* Initialize communication */
    recv_msg = 0;
    send_msg = 0;
    if (f_first > _braid_NextCPoint(f_ilower, cfactor))
    {
        recv_msg = 1;
    }
    if (f_next > _braid_NextCPoint(f_iupper+1, cfactor))
    {
        send_msg = 1;
    }
    _braid_UCommInitBasic(core, 0, recv_msg, send_msg, 0);

#if DEBUG
    printf("%d %d: 6 recv_msg = %d, send_msg = %d\n",
           FRefine_count, myproc, recv_msg, send_msg);
#endif

    /* Start from the right-most point */
    f_i = f_iupper;
    next = f_next;
    while (f_i >= f_ilower)
    {
        /* Find the next value to the left */
        u = NULL;
        for ( ; f_i >= f_ilower; f_i--)
        {
            f_ii = f_i - f_ilower;
            if (recv_ua[f_ii] != NULL)
            {
                u = recv_ua[f_ii];
                break;
            }
        }
        if ((f_i < f_ilower) && (recv_msg))
        {
            f_i = f_ilower-1; /* receive value from left processor */
            _braid_UGetVector(core, 0, f_i, &u);
        }

        /* Integrate the value if needed and set */
        if (u != NULL)
        {
            f_j = f_i;
            f_ci = _braid_NextCPoint(f_i, cfactor);
            if (next > f_ci)
            {
                /* integrate */
                f_hi = _braid_min(f_ci, f_iupper);
                for ( ; f_j < f_hi; f_j++)
                {
                    _braid_USetVector(core, 0, f_j, u, 0);
                    _braid_Step(core, 0, f_j+1, NULL, u);
                }
            }
            _braid_USetVector(core, 0, f_j, u, 1);
            next = f_i;
            f_i--;
        }
    }

    /* Free up some memory */
    _braid_TFree(recv_ua);

#if DEBUG
    printf("%d %d: 7\n", FRefine_count, myproc);
    fflush(stdout);
#endif

    _braid_UCommWait(core, 0);

#if DEBUG
    printf("%d %d: 8\n", FRefine_count, myproc);
    fflush(stdout);
    FRefine_count++;
#endif

    *refined_ptr = 1;

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Access to XBraid on grid level
 *----------------------------------------------------------------------------*/

braid_Int
_braid_FAccess(braid_Core     core,
               braid_Int      level,
               braid_Int      done)
{
    braid_App           app         = _braid_CoreElt(core, app);
    _braid_Grid       **grids       = _braid_CoreElt(core, grids);
    braid_AccessStatus  astatus     = _braid_CoreElt(core, astatus);
    braid_Int           iter        = _braid_CoreElt(core, niter);
    braid_Int           nrefine     = _braid_CoreElt(core, nrefine);
    braid_Int           gupper      = _braid_CoreElt(core, gupper);
    braid_Int           access_level= _braid_CoreElt(core, access_level);
    braid_Int           ncpoints    = _braid_GridElt(grids[level], ncpoints);
    braid_Real          *ta         = _braid_GridElt(grids[level], ta);
    braid_Int           ilower      = _braid_GridElt(grids[level], ilower);

    braid_Real     rnorm;
    braid_Vector   u;
    braid_Int      interval, flo, fhi, fi, ci;

    _braid_UCommInitF(core, level);

    _braid_GetRNorm(core, -1, &rnorm);

    /* Start from the right-most interval */
    for (interval = ncpoints; interval > -1; interval--)
    {
        _braid_GetInterval(core, level, interval, &flo, &fhi, &ci);

        /* Give access at F-points */
        if (flo <= fhi)
        {
            _braid_UGetVector(core, level, flo-1, &u);
        }
        for (fi = flo; fi <= fhi; fi++)
        {
            _braid_Step(core, level, fi, NULL, u);
            _braid_USetVector(core, level, fi, u, 0);

            if (access_level >= 1)
            {
                _braid_AccessStatusInit( ta[fi-ilower], rnorm, iter, level, nrefine, gupper,
                                         done, 0, astatus);
                _braid_AccessVector(core, astatus, u);
            }
        }
        if (flo <= fhi)
        {
            _braid_CoreFcn(core, free)(app, u);
        }

        /* Give access at C-points */
        if ((ci > -1) && (access_level >= 1))
        {
            _braid_UGetVectorRef(core, level, ci, &u);
            _braid_AccessStatusInit( ta[ci-ilower], rnorm, iter, level, nrefine, gupper,
                                     done, 0, astatus);
            _braid_AccessVector(core, astatus, u);
        }
    }
    _braid_UCommWait(core, level);

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Initialize grid hierarchy
 *----------------------------------------------------------------------------*/

braid_Int
_braid_InitHierarchy(braid_Core    core,
                     _braid_Grid  *fine_grid,
                     braid_Int     refined,
                     braid_Int    *recv_procs,
                     braid_Int    *send_procs)
{
    MPI_Comm       comm       = _braid_CoreElt(core, comm);
    braid_Int      max_levels = _braid_CoreElt(core, max_levels);
    braid_Int      min_coarse = _braid_CoreElt(core, min_coarse);
    braid_Int     *nrels      = _braid_CoreElt(core, nrels);
    braid_Int      nrdefault  = _braid_CoreElt(core, nrdefault);
    braid_Int      gupper     = _braid_CoreElt(core, gupper);
    braid_Int     *rfactors   = _braid_CoreElt(core, rfactors);
    braid_Int     *wfactors   = _braid_CoreElt(core, wfactors);
    braid_Int      nlevels    = _braid_CoreElt(core, nlevels);
    _braid_Grid  **grids      = _braid_CoreElt(core, grids);

    /**
     * These are some common index names used to refer to intervals and
     * time points.  Here's what they mean.
     *
     * cfactor            - coarsening factor, fixed on each level
     * ilower, iupper     - lowest and highest time indices on a level for one processor,
     *                      could be C or F points
     * clower, cupper     - lowest and highest C-point indices on a level for one processor
     *                      analagous to ilower, iupper being projected onto C-points
     * clo, chi           - the coarse level indices for clower and cupper
     * f_iupper, f_ilower - lowest and highest F-point indices on a level
     * c_iupper, c_ilower - lowest and highest time indices on the coarse level
     *                      (analagous to ilower and iupper, but on the next level down)
     * flo, fhi, ci       - describes an interval [ci, flo, flo+1, ..., fhi]
     * gclower, gcupper   - global lowest and highest C-point indices on a level
     **/

    braid_Int      level;
    braid_Int      ilower, iupper;
    braid_Int      clower, cupper, cfactor, ncpoints, nupoints;
    braid_Real    *ta;
    braid_Vector  *ua;
    braid_Vector  *va;
    braid_Vector  *fa;

    _braid_Grid   *grid;
    braid_Real    *f_ta;
    braid_Int      i, f_i, f_ilower, clo, chi, gclower, gcupper;

    MPI_Request    request1, request2;
    MPI_Status     status;
    braid_Int      left_proc, right_proc;

    grids[0] = fine_grid;

    /* Do sequential time marching if min_coarse is already reached */
    if ( gupper <= min_coarse )
    {
        max_levels = 1;
        _braid_CoreElt(core, max_levels) = max_levels;
    }

    /* Allocate space for rfactors (and initialize to zero) */
    ilower = _braid_GridElt(grids[0], ilower);
    iupper = _braid_GridElt(grids[0], iupper);
    rfactors = _braid_CTAlloc(braid_Int, iupper-ilower+2); /* Ensures non-NULL */
    wfactors = _braid_CTAlloc(braid_Int, iupper-ilower+2); /* Ensures non-NULL */
    for(i = 0; i < iupper-ilower+2; i++)
    {
        /* We need to ensure an rfactor of 1 for global index 0, and for
         * the case of a user-defined residual function and F-relaxation,
         * a default rfactor value at F-points */
        rfactors[i] = 1;
        wfactors[i] = 1;
    }
    _braid_CoreElt(core, rfactors) = rfactors;
    _braid_CoreElt(core, wfactors) = wfactors;
    /* Set up nrels array */
    for (level = 0; level < max_levels; level++)
    {
        if (nrels[level] < 0)
        {
            nrels[level] = nrdefault;
        }
    }

    /* Coarsen global grid to determine nlevels */
    gclower = 0;
    gcupper = gupper;
    for (level = 0; level < max_levels; level++)
    {
        grid = grids[level];
        ilower = _braid_GridElt(grid, ilower);
        iupper = _braid_GridElt(grid, iupper);
        if (level > 0)
        {
            /* Copy ta info from level-1 grid */
            ta       = _braid_GridElt(grid, ta);
            f_ilower = _braid_GridElt(grids[level-1], ilower);
            f_ta     = _braid_GridElt(grids[level-1], ta);
            cfactor  = _braid_GridElt(grids[level-1], cfactor);
            for (i = ilower; i <= iupper; i++)
            {
                _braid_MapCoarseToFine(i, cfactor, f_i);
                ta[i-ilower] = f_ta[f_i-f_ilower];
            }
        }

        _braid_GetCFactor(core, level, &cfactor);

        _braid_GridElt(grid, gupper) = gcupper;

        _braid_ProjectInterval(gclower, gcupper, 0, cfactor, &gclower, &gcupper);
        _braid_MapFineToCoarse(gclower, cfactor, gclower);
        _braid_MapFineToCoarse(gcupper, cfactor, gcupper);

        /* Coarsen */
        _braid_ProjectInterval(ilower, iupper, 0, cfactor, &clower, &cupper);
        _braid_MapFineToCoarse(clower, cfactor, clo);
        _braid_MapFineToCoarse(cupper, cfactor, chi);
        ncpoints = chi-clo+1;
        if (ncpoints < 0)
        {
            ncpoints = 0;
        }
        _braid_GridElt(grid, clower)   = clower;
        _braid_GridElt(grid, cupper)   = cupper;
        _braid_GridElt(grid, cfactor)  = cfactor;
        _braid_GridElt(grid, ncpoints) = ncpoints;
        if ( (gclower < gcupper) && (max_levels > level+1) &&
                ((gcupper - gclower) >= min_coarse) )
        {
            /* Initialize the coarse grid */
            _braid_GridInit(core, level+1, clo, chi, &grids[level+1]);
        }
        else
        {
            /* This is the coarsest level */
            if ( (level > 0) || (!refined) )
            {
                /* If this is a true coarse level (it has a fine grid above it in
                 * the current hierarchy) or it is a fine level that was not built
                 * by refining a coarser grid, then do serial time integration by
                 * setting only one C-point and the rest F-points */
                if (ilower == 0)
                {
                    ncpoints = 1;
                }
                else
                {
                    ncpoints = 0;
                }
                /* clower > cupper indicates empty interval */
                _braid_GridElt(grid, clower)   = ilower;
                _braid_GridElt(grid, cupper)   = 0;
                _braid_GridElt(grid, cfactor)  = gupper+1;
                _braid_GridElt(grid, ncpoints) = ncpoints;
            }

            /* Stop coarsening */
            break;
        }

        if(level == 0)
        {
            /* Allocate space for storage of residual norm at each C-point */
            _braid_CoreElt(core, tnorm_a)  = _braid_CTAlloc(braid_Real, ncpoints);
        }
    }
    nlevels = level+1;
    _braid_CoreElt(core, nlevels) = nlevels;

    /* Allocate ua, va, and fa here */
    for (level = 0; level < nlevels; level++)
    {
        grid = grids[level];
        ilower = _braid_GridElt(grid, ilower);
        iupper = _braid_GridElt(grid, iupper);
        if (level > 0)
        {
            va = _braid_CTAlloc(braid_Vector, iupper-ilower+2);
            fa = _braid_CTAlloc(braid_Vector, iupper-ilower+2);
            _braid_GridElt(grid, va_alloc) = va;
            _braid_GridElt(grid, fa_alloc) = fa;
            _braid_GridElt(grid, va)       = va+1;  /* shift */
            _braid_GridElt(grid, fa)       = fa+1;  /* shift */
        }

        // If on level that only stores C-points
        if ( (_braid_CoreElt(core, storage) < 0) ||
                (level < _braid_CoreElt(core, storage)) )
        {
            nupoints = _braid_GridElt(grid, ncpoints);   /* only C-points */
        }
        else
        {
            nupoints = iupper-ilower+1;                  /* all points */
        }
        ua = _braid_CTAlloc(braid_Vector, nupoints+1);
        _braid_GridElt(grid, nupoints)  = nupoints;
        _braid_GridElt(grid, ua_alloc)  = ua;
        _braid_GridElt(grid, ua)        = ua+1;  /* shift */
    }

    if (recv_procs && send_procs ) 
    {
       for (i = 0 ; i < nlevels; i++ )
       {
          _braid_GridElt( grids[i], recv_proc ) = recv_procs[i] ;
          _braid_GridElt( grids[i], send_proc ) = send_procs[i] ;
       }
    }

    /* Communicate ta[-1] and ta[iupper-ilower+1] information */
    for (level = 0; level < nlevels; level++)
    {
        grid = grids[level];
        ilower = _braid_GridElt(grid, ilower);
        iupper = _braid_GridElt(grid, iupper);
        ta     = _braid_GridElt(grid, ta);

        if (ilower <= iupper)
        {
           if( recv_procs && send_procs) 
           {
              _braid_GetProcLeftOrRight( core, level, -1, &left_proc);
              _braid_GetProcLeftOrRight( core, level, 1, &right_proc);
           }
           else
           {
              _braid_GetProc(core, level, ilower-1, &left_proc);
              _braid_GetProc(core, level, iupper+1, &right_proc);
              _braid_GridElt( grid, recv_proc ) = left_proc;
              _braid_GridElt( grid, send_proc ) = right_proc; 
           }

            /* Post receive to set ta[-1] on each processor*/
            if (left_proc > -1)
            {
                MPI_Irecv(&ta[-1], sizeof(braid_Real), MPI_BYTE,
                          left_proc, 1, comm, &request1);
            }
            else
            {
                /* Place a repeat value to indicate the start of the time-line for this level */
                ta[-1] = ta[0];
            }
            /* Post receive to set ta[iupper-ilower+1] on each processor */
            if ( _braid_CoreElt(core, scoarsen) != NULL )
            {
                if (right_proc > -1)
                {
                    MPI_Irecv(&ta[iupper-ilower+1], sizeof(braid_Real), MPI_BYTE,
                              right_proc, 1, comm, &request2);
                }
                else
                {
                    /* Place a repeat value to indicate the end the time-line for this level */
                    ta[iupper-ilower+1] = ta[iupper-ilower];
                }
            }

            /* Post send that sets ta[-1] on each processor */
            if (right_proc > -1)
            {
                MPI_Send(&ta[iupper-ilower], sizeof(braid_Real), MPI_BYTE,
                         right_proc, 1, comm);
            }
            /* Post send that sets ta[iupper-ilower+1] on each processor */
            if ( (left_proc > -1) && ( _braid_CoreElt(core, scoarsen) != NULL ) )
            {
                MPI_Send(&ta[0], sizeof(braid_Real), MPI_BYTE, left_proc, 1, comm);
            }

            /* Finish receive */
            if (left_proc > -1)
            {
                MPI_Wait(&request1, &status);
            }
            if ( (right_proc > -1) && ( _braid_CoreElt(core, scoarsen) != NULL ) )
            {
                MPI_Wait(&request2, &status);
            }
        }
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Print the residual norm at ever C-point for debugging purposes
 *----------------------------------------------------------------------------*/

braid_Int
_braid_PrintSpatialNorms(braid_Core    core,
                         braid_Real   *rnorms,     /* This processor's local residual norms at C-points */
                         braid_Int     n)          /* Length of the rnorms array */
{
    MPI_Comm       comm       = _braid_CoreElt(core, comm);
    MPI_Comm       comm_world = _braid_CoreElt(core, comm_world);
    _braid_Grid  **grids      = _braid_CoreElt(core, grids);
    braid_Int      cfactor    = _braid_GridElt(grids[0], cfactor);
    braid_Int      gupper     = _braid_CoreElt(core, gupper);

    braid_Int      g_ncpoints = ceil( ((braid_Real) gupper) / ((braid_Real) cfactor ));

    braid_Int     *recvcounts = NULL;
    braid_Real    *recvbuf;
    braid_Int     *displs;
    braid_Int      i, p, myid_t, myid_world, my_root_global_rank;

    MPI_Comm_size(comm, &p);
    MPI_Comm_rank(comm_world, &myid_world);
    MPI_Comm_rank(comm, &myid_t);

    /* We need to know all the processor's belonging to the temporal
     * communicator of global rank 0.  It is only these processors that are
     * involved with printing output. */
    my_root_global_rank = myid_world;
    MPI_Bcast(&my_root_global_rank, 1, braid_MPI_INT, 0, comm);

    if(my_root_global_rank == 0)
    {
        if(myid_t == 0)
        {
            recvbuf = _braid_CTAlloc(braid_Real, g_ncpoints);
            recvcounts = _braid_CTAlloc(braid_Int, p);
            displs = _braid_CTAlloc(braid_Int, p);
        }

        /* Rank 0 gather's every processor's number of C-points, which forms the
         * displacements (displs) for the Gatherv call below */
        MPI_Gather(&n, 1, braid_MPI_INT, recvcounts, 1, braid_MPI_INT, 0, comm);

        if(myid_t == 0)
        {
            displs[0] = 0;
            for(i = 1; i < p; i++)
            {
                displs[i] = displs[i-1] + recvcounts[i-1];
            }
        }

        /* Gather over comm */
        MPI_Gatherv(rnorms, n, braid_MPI_REAL, recvbuf, recvcounts, displs,
                    braid_MPI_REAL, 0, comm);

        if(myid_t == 0)
        {
            for(i = 0; i < g_ncpoints; i++)
            {
                _braid_printf("  Braid:  time step: %6d, rnorm: %1.2e\n", i*cfactor, recvbuf[i]);
            }
        }

        if(myid_t == 0)
        {
            _braid_TFree(recvbuf);
            _braid_TFree(recvcounts);
            _braid_TFree(displs);
        }
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Copy the initialized C-points on the fine grid, to all coarse levels.
 * Allows first down cycle to be skipped, in FMG fashion.
 *----------------------------------------------------------------------------*/

braid_Int
_braid_CopyFineToCoarse(braid_Core  core)
{
    braid_App      app     = _braid_CoreElt(core, app);
    _braid_Grid  **grids   = _braid_CoreElt(core, grids);
    braid_Int      nlevels = _braid_CoreElt(core, nlevels);

    braid_Int      f_index, index, is_stored, level, f_cfactor;
    braid_Int      ilower, iupper;
    braid_Vector   u, *va;

    for(level = 1; level < nlevels; level++)
    {

        f_cfactor = _braid_GridElt(grids[level-1], cfactor);
        iupper    = _braid_GridElt(grids[level], iupper);
        ilower    = _braid_GridElt(grids[level], ilower);
        va        = _braid_GridElt(grids[level], va);

        /* Loop over all points belonging to this processor, and if a C-point,
         * then carry out spatial coarsening and copy to ua and va */
        for (index=ilower; index<=iupper; index++)
        {
            _braid_MapCoarseToFine(index, f_cfactor, f_index);
            _braid_UGetVector(core, level-1, f_index, &u);
            _braid_Coarsen(core, level, f_index, index, u, &va[index-ilower]);

            _braid_CoreFcn(core, free)(app, u);
            _braid_CoreFcn(core, clone)(app, va[index-ilower], &u);
            _braid_USetVectorRef(core, level, index, u);
            _braid_UGetIndex(core, level, index, &is_stored);
            if (is_stored <= -1)
            {
                _braid_CoreFcn(core, free)(app, u);
            }

        }
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Set the residual norm for iteration iter.  If iter < 0, set the rnorm for the
 * last iteration minus |iter|-1.  Also set the initial residual norm.
 *----------------------------------------------------------------------------*/

braid_Int
_braid_SetRNorm(braid_Core  core,
                braid_Int   iter,
                braid_Real  rnorm)
{
    braid_Real  *rnorms   = _braid_CoreElt(core, rnorms);
    braid_Int    max_iter = _braid_CoreElt(core, max_iter);
    braid_Int    k;

    k = iter;
    if (iter < 0)
    {
        k = _braid_CoreElt(core, niter) + 1 + iter;
    }

    if ((k > -1) && (k <= max_iter))
    {
        rnorms[k] = rnorm;

        /* Set initial residual norm if not already set */
        if (k == 0)
        {
            if ( _braid_CoreElt(core, rnorm0) == braid_INVALID_RNORM )
            {
                _braid_CoreElt(core, rnorm0) = rnorm;
            }
        }
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Get the residual norm for iteration iter.  If iter < 0, get the rnorm for the
 * last iteration minus |iter|-1.
 *----------------------------------------------------------------------------*/

braid_Int
_braid_GetRNorm(braid_Core  core,
                braid_Int   iter,
                braid_Real *rnorm_ptr)
{
    braid_Real  *rnorms   = _braid_CoreElt(core, rnorms);
    braid_Int    max_iter = _braid_CoreElt(core, max_iter);
    braid_Int    k;

    /* Initialize to invalid value */
    *rnorm_ptr = braid_INVALID_RNORM;

    k = iter;
    if (iter < 0)
    {
        k = _braid_CoreElt(core, niter) + 1 + iter;
    }

    if ((k > -1) && (k <= max_iter))
    {
        *rnorm_ptr = rnorms[k];
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Same as SetRNorm, but sets full residual norm
 *----------------------------------------------------------------------------*/

braid_Int
_braid_SetFullRNorm(braid_Core  core,
                    braid_Int   iter,
                    braid_Real  rnorm)
{
    braid_Real  *rnorms   = _braid_CoreElt(core, full_rnorms);
    braid_Int    max_iter = _braid_CoreElt(core, max_iter);
    braid_Int    k;

    if (rnorms == NULL)
    {
        return _braid_error_flag;
    }

    k = iter;
    if (iter < 0)
    {
        k = _braid_CoreElt(core, niter) + 1 + iter;
    }

    if ((k > -1) && (k <= max_iter))
    {
        rnorms[k] = rnorm;

        /* Set initial residual norm if not already set */
        if (k == 0)
        {
            if ( _braid_CoreElt(core, full_rnorm0) == braid_INVALID_RNORM )
            {
                _braid_CoreElt(core, full_rnorm0) = rnorm;
            }
        }
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Same as GetRNorm, but gets full residual norm
 *----------------------------------------------------------------------------*/

braid_Int
_braid_GetFullRNorm(braid_Core  core,
                    braid_Int   iter,
                    braid_Real *rnorm_ptr)
{
    braid_Real  *rnorms   = _braid_CoreElt(core, full_rnorms);
    braid_Int    max_iter = _braid_CoreElt(core, max_iter);
    braid_Int    k;

    /* Initialize to invalid value */
    *rnorm_ptr = braid_INVALID_RNORM;

    if (rnorms == NULL)
    {
        return _braid_error_flag;
    }

    k = iter;
    if (iter < 0)
    {
        k = _braid_CoreElt(core, niter) + 1 + iter;
    }

    if ((k > -1) && (k <= max_iter))
    {
        *rnorm_ptr = rnorms[k];
    }

    return _braid_error_flag;
}

/*----------------------------------------------------------------------------
 * Process the error with code ierr raised at the given line and source file
 *----------------------------------------------------------------------------*/

void
_braid_ErrorHandler(const char *filename,
                    braid_Int   line,
                    braid_Int   ierr,
                    const char *msg)
{
    _braid_error_flag |= ierr;

#ifdef braid_PRINT_ERRORS
    if (msg)
    {
        _braid_printf("braid error in file \"%s\", line %d, error code = %d - %s\n",
                      filename, line, ierr, msg);
    }
    else
    {
        _braid_printf("braid error in file \"%s\", line %d, error code = %d\n",
                      filename, line, ierr);
    }
#endif
}


/*------------------------------------------------------------------------------
 * Get my new ilower and iupper based on load balencing weights
 *------------------------------------------------------------------------------*/
braid_Int
_braid_LoadBalence(braid_Core core,
                   braid_Int  *weights,
                   braid_Int  curr_ilower,
                   braid_Int  curr_iupper,
                   braid_Int  ngupper,
                   braid_Int  *nilower,
                   braid_Int  *niupper)
{
    braid_Int rank, comm_size;
    MPI_Comm comm = _braid_CoreElt( core, comm );
    MPI_Comm_rank( comm, &rank );
    MPI_Comm_size( comm, &comm_size );

    braid_Int *wfactors = weights;
    braid_Int gupper    = ngupper;                             //_braid_CoreElt( core, gupper );
    braid_Int ilower    = curr_ilower;                         //_braid_GridElt( grids[0], ilower );
    braid_Int iupper    = curr_iupper;                         // _braid_CoreElt( grids[0], iupper );

    braid_Int i, j;
    braid_Real l_sum, g_sum, l_max, g_max, c_sum_begin, c_sum_end, goal_load, num_partitions;
    braid_Int *send_buffer_ilower, *send_buffer_iupper, partition_low, partition_high, num_recvs, num_sends;
    MPI_Request *send_requests, *recv_requests;
    MPI_Status  *send_status, *recv_status;

    // Get my local sum
    l_sum = 0;
    l_max = 0;
    if ( ilower == 0 ) wfactors[0] = 0;
    for ( i = ilower; i <= iupper; i++ )
    {
        l_max = (l_max < wfactors[i-ilower])? wfactors[i-ilower] : l_max;
        l_sum += wfactors[i-ilower];
    }

    MPI_Allreduce(&l_sum, &g_sum, 1, braid_MPI_REAL, MPI_SUM, comm);
    MPI_Allreduce(&l_max, &g_max, 1, braid_MPI_REAL, MPI_MAX, comm);
    MPI_Exscan(&l_sum, &c_sum_begin, 1, braid_MPI_REAL, MPI_SUM, comm );

    c_sum_end = c_sum_begin + l_sum;
    goal_load = _braid_max ( g_sum / _braid_min( comm_size , gupper + 1 ), g_max );
    num_partitions = ceil( g_sum / goal_load )  ; // number of procs we will use
    num_partitions = _braid_min( comm_size, num_partitions);

    num_recvs = 0;
    num_sends = 0;
    recv_requests = _braid_CTAlloc( MPI_Request, 2 );
    j = 0;

    if ( l_sum > 0 || rank == 0 )
    {
        if ( rank < num_partitions )
        {
            //Post my recieves for ilower and iupper
            if ( rank < num_partitions -1 )
            {
                MPI_Irecv( niupper, 1, MPI_INT, MPI_ANY_SOURCE, 18, comm, &recv_requests[num_recvs++] );
            }
            else
                *niupper = gupper;
            if ( rank > 0 )
                MPI_Irecv( nilower, 1, MPI_INT, MPI_ANY_SOURCE, 17, comm, &recv_requests[num_recvs++] );
            else
                *nilower = 0;
        }

        // Get the partition boundaries that lie on my processor
        partition_low = 1;
        partition_high = num_partitions - 1 ;
        if ( l_sum > 0 && ilower > 0 )
            partition_low = (braid_Int) ceil(c_sum_begin/goal_load + 1e-10);
        if ( l_sum > 0 && iupper < gupper )
            partition_high = (braid_Int) floor(c_sum_end/goal_load);
        if ( l_sum == 0 && ilower == 0 )
            partition_high = 0;

        braid_Int num_sends = 0;
        send_buffer_ilower  = _braid_CTAlloc( braid_Int , (partition_high - partition_low + 1) );
        send_buffer_iupper  = _braid_CTAlloc( braid_Int , (partition_high - partition_low + 1) );
        send_requests = _braid_CTAlloc( MPI_Request, 2*(partition_high-partition_low + 1) ) ;

        if ( ilower == 0 ) wfactors[0] = 0;
        for ( i = partition_low; i <= partition_high; i++ )
        {
            while ( c_sum_begin < goal_load*i )
                c_sum_begin += wfactors[j++];
            send_buffer_ilower[(i-partition_low)] = j + ilower;
            send_buffer_iupper[(i-partition_low)] = j-1 + ilower;
            if ( j <= gupper )
                MPI_Isend( &send_buffer_ilower[i-partition_low], 1, braid_MPI_INT, i, 17, comm, &send_requests[num_sends++] );
            MPI_Isend( &send_buffer_iupper[i-partition_low], 1, braid_MPI_INT, i - 1, 18, comm, &send_requests[num_sends++] );
        }
    }

    recv_status = _braid_CTAlloc( MPI_Status , num_recvs );
    send_status = _braid_CTAlloc( MPI_Status , num_sends );
    MPI_Waitall( num_recvs , recv_requests , recv_status);
    if ( rank < num_partitions )
    {
        MPI_Waitall( num_sends , send_requests , send_status);
    }
    if ( rank >= num_partitions )
    {
        *nilower = gupper + 1;
        *niupper = gupper;
    }

    MPI_Barrier ( comm );
    return _braid_error_flag;
}



braid_Int
_braid_GetPartition(braid_Core core,
                    braid_Int *assumed_ilower,
                    braid_Int *assumed_iupper,
                    braid_Int *new_ilower,
                    braid_Int *new_iupper,
                    braid_Int *new_gupper,
                    braid_Int *owners
                   )
{

    MPI_Comm comm = _braid_CoreElt( core, comm );
    braid_Int comm_size, rank, i, *proc_send_to, **send_buffer, sbuf_0, sbuf_1, proc, proc1;
    braid_Int num_sends, ilower, iupper, gupper, num_messages, *recv_buffer;
    MPI_Request *send_request;
    MPI_Status recv_status;

    //Get rid of this!!!!!!
    ilower = *new_ilower;
    iupper = *new_iupper;
    gupper = *new_gupper;

    MPI_Comm_size( comm, &comm_size );
    MPI_Comm_rank( comm, &rank );
    num_sends = iupper - ilower + 1;
    send_buffer = _braid_CTAlloc( braid_Int*, num_sends + 1 ); //1
    proc_send_to = _braid_CTAlloc( braid_Int,  num_sends + 1 ); //2
    sbuf_0 = -1;
    sbuf_1 = 0;
    proc1 = -1;

    for ( i = ilower; i <= iupper; i++ )
    {
        _braid_GetBlockDistProc( gupper +1, comm_size, i , &proc );
        if ( proc != proc1 )
        {
            sbuf_0++;
            sbuf_1 = 2;
            send_buffer[sbuf_0] = _braid_CTAlloc( braid_Int, num_sends + 1  ); //3 + loop
            send_buffer[sbuf_0][1] = i ;
            proc_send_to[sbuf_0] = proc;
            proc1 = proc;
        }
        else
        {
            send_buffer[sbuf_0][sbuf_1] = i;
            sbuf_1++;
        }
        send_buffer[sbuf_0][0]++;
    }

    send_request = _braid_CTAlloc( MPI_Request , sbuf_0 + 1 ); //4 + loop
    for ( i = 0; i <= sbuf_0; i++ )
        MPI_Isend( &send_buffer[i][0] , 1 + send_buffer[i][0] , braid_MPI_INT, proc_send_to[i] , 18, comm, &send_request[i] );

    num_messages = *assumed_iupper - *assumed_ilower + 1 ;
    recv_buffer = _braid_CTAlloc( braid_Int, num_messages + 2 ); //5 + loop
    while ( num_messages > 0 )
    {
        MPI_Recv( recv_buffer, num_messages + 1, braid_MPI_INT, MPI_ANY_SOURCE, 18, comm, &recv_status);
        for ( i = 0 ; i < recv_buffer[0] ; i++ )
            owners[ recv_buffer[i+1] - *assumed_ilower] = recv_status.MPI_SOURCE;
        num_messages -= recv_buffer[0];
    }
    if (sbuf_0 >= 0  )
        MPI_Waitall( sbuf_0 + 1, send_request, MPI_STATUS_IGNORE );

    free ( proc_send_to ); // 4 + loop
    free ( recv_buffer ); // 3 + loop
    free ( send_request ); // 2 + loop

    for ( i = 0; i <= sbuf_0; i++ )
    {
        free( send_buffer[i] ); //1
    }
    free ( send_buffer ); //0

    MPI_Barrier( comm );
    return _braid_error_flag;
}

braid_Int _braid_assume_partition( braid_Core core,
                                   braid_Int   nlevels,
                                   braid_Int  *old_ilower,
                                   braid_Int  *old_iupper,
                                   braid_Int  *new_ilower,
                                   braid_Int  *new_iupper,
                                   braid_Int  *new_gupper,
                                   braid_Int   **send_procs_o,
                                   braid_Int   **recv_procs_o,
                                   braid_Int   **refine_map_s,
                                   braid_Int   **refine_map_r)
{
    MPI_Comm comm = _braid_CoreElt( core, comm );

    //mallocs
    braid_Int *gupper, *cfactor, *iupper, *ilower, *owners, *load_sends, *respond;
    braid_Int *need_procs, *need_indices,  *have_procs, *have_indices, *recv_buffer;
    braid_Int **send_buffer, **resp_buffer;
    MPI_Request *send_requests, *resp_requests;

    braid_Int rank, comm_size, i , j, k, kk,  index , level;
    braid_Int haves, needs, bals, sends, response, low, high, sent;
    braid_Int  num_old_inds, assumed_ilower, assumed_iupper;
    braid_Int num_messages, num_needs, num_haves, curr_proc, new_proc;
    MPI_Status status;

    MPI_Comm_size( comm, &comm_size );
    MPI_Comm_rank( comm, &rank );
    gupper =  _braid_CTAlloc( braid_Int, nlevels ); //1
    cfactor = _braid_CTAlloc( braid_Int, nlevels ); //2
    ilower =  _braid_CTAlloc( braid_Int, nlevels ); //3
    iupper =  _braid_CTAlloc( braid_Int, nlevels ); //4

    gupper[0] = *new_gupper;
    iupper[0] = *new_iupper;
    ilower[0] = *new_ilower;
    _braid_GetCFactor( core, 0, &cfactor[0] );

    for ( i = 1; i < nlevels; i++ )
    {
        _braid_ProjectInterval( ilower[i-1], iupper[i-1] ,0, cfactor[i-1], &ilower[i], &iupper[i] );
        _braid_MapFineToCoarse( ilower[i], cfactor[i-1], ilower[i] );
        _braid_MapFineToCoarse( iupper[i], cfactor[i-1], iupper[i]);
        _braid_ProjectInterval( 0, gupper[i-1], 0, cfactor[i-1], &k, &gupper[i] );
        _braid_MapFineToCoarse( gupper[i], cfactor[i-1], gupper[i] );
        _braid_GetCFactor(core, i, &cfactor[i] ); // cfactor[i-1];
    }

    // 1. Figure out how many messages i will get asked about
    _braid_GetBlockDistInterval(gupper[0]+1, comm_size, rank, &assumed_ilower, &assumed_iupper );
    owners = _braid_CTAlloc( braid_Int, assumed_iupper - assumed_ilower + 1 ); //5
    _braid_GetPartition( core, &assumed_ilower, &assumed_iupper, new_ilower, new_iupper, new_gupper,  owners );
    num_messages = 0;
    //Messages for nearest neighbour communication
    for ( i = _braid_max( assumed_ilower, 1 ); i <= assumed_iupper ; i++ )
    {
        index = i;
        level = 0;
        num_messages++;  //One message for nearest neighbour stuff
        while ( level < nlevels -1 && !(index % cfactor[level])  )
        {
            index = index/cfactor[level++];
            num_messages++; // one message for each nearest neighbour on coarse levels.
        }
    }

    //One message will be recieved for each fine point on assumed partition
    //regarding who owns that index for load balencing / frefine purposes
    for ( i = assumed_ilower; i <= assumed_iupper; i++ )
        num_messages++;

    //Get a list of proceesors to ask about my fine points from the old
    //grid. Basically I need to know where to send all of the indices
    //I had on the old time grid when moving to the new time grid
    // Lo
    num_old_inds = *old_iupper - *old_ilower + 1 ;
    load_sends = _braid_CTAlloc( braid_Int, num_old_inds + 1); //6
    for ( i = *old_ilower; i <= *old_iupper; i++ )
    {
        _braid_GetBlockDistProc( gupper[0] + 1, comm_size, i , &load_sends[i - *old_ilower] );
    }
    load_sends[num_old_inds] = -1;

    //This is a list of indices that I need to complete F Relaxation on each grid
    //These will be saved in the core and used to send infor during relaxation
    need_indices = _braid_CTAlloc( braid_Int, nlevels ); //7
    need_procs = _braid_CTAlloc( braid_Int, nlevels + 1 ); //8
    num_needs = 0;
    for ( i = 0; i < nlevels; i++ )
    {
        index = iupper[i] + 1;
        if ( index <= gupper[i] && iupper[i] >= ilower[i])
        {
            for ( j = i; j > 0; j-- )
                _braid_MapCoarseToFine( index, cfactor[j-1], index );
            _braid_GetBlockDistProc( gupper[0] + 1, comm_size, index, &need_procs[num_needs] );
            need_indices[num_needs++] = index;
        }
    }

    //This is a list of indices I have, but that I dont need a response about
    //for F relaxation.
    have_indices = _braid_CTAlloc( braid_Int, iupper[0] - ilower[0] + 10 ); //9
    have_procs = _braid_CTAlloc( braid_Int, iupper[0] - ilower[0] + 10); //10
    num_haves = -1;
    curr_proc = -1;
    new_proc = -1 ;
    for ( i = ilower[0] + 1 ; i <= iupper[0]; i++ )
    {
        level = 0;
        index = i;
        _braid_GetBlockDistProc( gupper[0] + 1, comm_size, index, &new_proc );
        if ( new_proc != curr_proc )
        {
            curr_proc = new_proc;
            have_procs[++num_haves] = new_proc;
        }
        have_indices[num_haves]++;
        while ( level < nlevels -1 && !( index % cfactor[level] ) )
        {
            index = index/cfactor[level++];
            if ( index >  ilower[level] )
                have_indices[num_haves]++;
        }
    }
    num_haves++;



    //Now we need to build the send buffer. The format of the send buffer is
    //0. Number of indices that I have but you dont need to recieve a response for
    //1. Number of indices that I need a response for Frelax
    //2. First indice that I need response for Frelax
    //3. Second index i need response for Frealx
    //4. etc ....
    //6. Number of indices that I have for FRefine!
    //7 -- xx indices that I need a reponse for


    haves = 0;
    needs = 0;
    bals  = 0;
    sends = 0;
    response = 0;
    have_procs[ num_haves ] = -1;
    need_procs[ num_needs ] = -1;

    //Get the lowest processor and the highest processor that we need to talk to
    //THis is ugly -- must be a better way
    low = braid_Int_Max;
    high = -1;
    if ( num_haves > 0 )
    {
        low = _braid_min( have_procs[0], low );
        high = _braid_max( have_procs[num_haves-1], high );
    }

    if ( num_needs > 0 )
    {
        low = _braid_min( need_procs[0], low );
        high = _braid_max( need_procs[num_needs-1], high );
    }
    if ( num_old_inds > 0 )
    {
        low = _braid_min( load_sends[0], low );
        high = _braid_max( load_sends[num_old_inds -1], high );
    }
    
    send_buffer = _braid_CTAlloc( braid_Int*, high - low + 5  ); //7
    resp_buffer = _braid_CTAlloc( braid_Int*, high - low + 5 ); //8
    send_requests = _braid_CTAlloc( MPI_Request, high - low + 5 ); //9
    resp_requests = _braid_CTAlloc( MPI_Request, high - low + 5 ); //10
    
    //Build the send buffer
    sent = 1;
    for ( i = low; i <= high; i++ )
    {
        if ( sent )
        {
            send_buffer[sends] = _braid_CTAlloc( braid_Int, 11 + nlevels + num_old_inds  ); //10 + sends
            sent = 0;
        }
        if ( i == have_procs[haves] )
        {
            send_buffer[sends][0] = have_indices[haves++];
            sent = 1;
        }

        while( need_procs[needs] == i )
        {
            send_buffer[sends][2 + send_buffer[sends][1]] = need_indices[needs++];
            send_buffer[sends][1]++;
            sent = 1;
        }

        while ( i == load_sends[bals] )
        {
            send_buffer[sends][ 2 + send_buffer[sends][1]]++;
            send_buffer[sends][ 3 + send_buffer[sends][1] + bals ] = *old_ilower + bals;
            bals++;
            sent = 1;
        }
        if ( sent )
        {

            MPI_Isend( send_buffer[sends] ,  3 + send_buffer[sends][1] + bals , braid_MPI_INT, i, 14, comm, &send_requests[sends] );
            if (send_buffer[sends][1] > 0 || send_buffer[sends][2 + send_buffer[sends][1]] > 0 )
            {
                resp_buffer[response] = _braid_CTAlloc( braid_Int, 11 + nlevels + num_old_inds ); // 10 + sends + response
                MPI_Irecv( resp_buffer[response] , 11 + nlevels + num_old_inds, braid_MPI_INT, i, 13, comm, &resp_requests[response] );
                response++;
            }
        }
    }
    free ( have_procs ); //9 + sends + response
    free ( have_indices ); // 8 + sends + response
    free ( need_indices ); // 7 + sends + respomse


    //Now we need to recvieve all the messages, that are send to me, and respond if needed.
    //Response buffer is formatted as :
    //0. Number of indices I am returning for FRefine
    //1. Owner of first index sent to me.
    //2. Owner of second index sent to me.
    //3. etc....
    //4. Number of indicies returning for Load balencing
    //5. Owner of first index for balencing
    //6. etc ....
    i = 0;
    recv_buffer = _braid_CTAlloc( braid_Int, 11 + nlevels + assumed_iupper - assumed_ilower  ); // 8 + ...
    respond = _braid_CTAlloc( braid_Int, 11 + nlevels + assumed_iupper - assumed_ilower );  // 9 + ...
    while (num_messages > 0 )
    {
        MPI_Recv( recv_buffer, 11 + nlevels + assumed_iupper - assumed_ilower, braid_MPI_INT, MPI_ANY_SOURCE, 14, comm, &status );
        num_messages -= (recv_buffer[0] + recv_buffer[1] + recv_buffer[ 2 + recv_buffer[1] ]);
        if ( recv_buffer[1] > 0 || recv_buffer[ 2 + recv_buffer[1] ] > 0 )
        {
            //Needs for Frealx information
            respond[0] = recv_buffer[1];
            for ( i = 0; i < recv_buffer[1]; i++ )
                respond[i+1] = owners[ recv_buffer[2+i] - assumed_ilower ] ;

            //Needs for load balencing information
            respond[ 1 + respond[0] ] = recv_buffer[ 2 + recv_buffer[1] ];
            for( i = 0; i< respond[ 1 + respond[0] ]; i++ )
                respond[ i + respond[0]  + 2 ] = owners[ recv_buffer[ 3 + respond[0] + i] -assumed_ilower] ;

            MPI_Send( respond, 3 + respond[0] + respond[1+respond[0]]  , braid_MPI_INT, status.MPI_SOURCE, 13, comm );
        }
    }
    free( recv_buffer ); //  8+ ...
    free( respond ); // 7 + ...
    free( owners ); // 6 + ...

    //Now wait for everything to finish up
    if ( sends > 0 )
        MPI_Waitall( sends , send_requests , MPI_STATUS_IGNORE );
    if ( response > 0 )
        MPI_Waitall( response, resp_requests, MPI_STATUS_IGNORE );

    free( send_requests ); //5 + ...
    free( resp_requests ); // 4 + sends + response
    for( i = 0; i < sends; i++ )
        free( send_buffer[i] );
    free( send_buffer ); // 3 + response

    //Extract the information gathered into a useful format.
    k = 0;
    kk = 0;
    for ( i = 0; i < response; i++ )
    {
        braid_Int *recv = resp_buffer[i];
        for ( j = 0; j < recv[0] ; j++ )
            need_procs[k++] = recv[j+1];
        for ( j = 0; j <  recv[ 1 + recv[0]]    ; j++ )
            load_sends[kk++] = recv[ 2 + recv[0] + j ];
    }
    for ( i = 0; i < response; i++ )
        free( resp_buffer[i] );
    free( resp_buffer ); // 2

    MPI_Barrier (comm);

    //Get the recv proc information
    *send_procs_o = _braid_TReAlloc( *send_procs_o, braid_Int , nlevels );
    *recv_procs_o = _braid_TReAlloc( *recv_procs_o, braid_Int , nlevels );
    braid_Int *send_buffer_1 = _braid_CTAlloc( braid_Int, nlevels );
    MPI_Request *send_req_1 = _braid_CTAlloc( MPI_Request, nlevels );
    sends = 0;
    for( i = 0; i < nlevels; i++ )
        (*send_procs_o)[i] = -1;

    for ( i = 0; i < k; i++ )
    {
        if ( need_procs[i] > 0 && rank < comm_size - 1 && ilower[0] <= iupper[0] )
        {
            send_buffer_1[sends] = i;
            MPI_Isend( &send_buffer_1[sends] , 1, braid_MPI_INT, need_procs[i], 15, comm , &send_req_1[sends]);
            (*send_procs_o)[i] = need_procs[i];
            sends++;
        }
        else
            (*send_procs_o)[i] = -1;
    }
    for ( i = 0; i < nlevels; i++ )
        (*recv_procs_o)[i] = -1;

    for ( i = 0; i < nlevels; i++ )
    {
        if ( iupper[i] - ilower[i] + 1 > 0 && rank > 0 )
        {

            MPI_Recv( &kk, 1, braid_MPI_INT, MPI_ANY_SOURCE, 15, comm , &status);
            (*recv_procs_o)[kk] = status.MPI_SOURCE;
        }
    }
    if ( sends > 0 )
        MPI_Waitall( sends, send_req_1 , MPI_STATUS_IGNORE );

    *refine_map_s = _braid_TReAlloc( *refine_map_s, braid_Int, *old_iupper - *old_ilower + 1 );
    for ( i = 0; i < *old_iupper - *old_ilower + 1; i++ )
        (*refine_map_s)[i] = load_sends[i];

    *refine_map_r = _braid_TReAlloc( *refine_map_r, braid_Int, *new_iupper- *new_ilower+1 );
    braid_Int **send_buff = _braid_CTAlloc( braid_Int * , *old_iupper - *old_ilower + 1 );
    braid_Int *send_rank = _braid_CTAlloc( braid_Int , *old_iupper - *old_ilower + 1 );
    braid_Int send_num = -1;

    braid_Int proc1 = -1;
    braid_Int proc = 0;
    for ( i = *old_ilower; i <= *old_iupper; i++ )
    {
        proc = (*refine_map_s)[i- *old_ilower];
        if ( proc != proc1 )
        {
            send_num++;
            send_buff[send_num] = _braid_CTAlloc( braid_Int, *old_iupper - *old_ilower + 5 );
            send_rank[send_num] = proc;
            proc1 = proc;
        }
        send_buff[send_num][0]++;
        send_buff[send_num][send_buff[send_num][0]] = i ;
    }


    MPI_Request *req_last = _braid_CTAlloc( MPI_Request , *new_iupper - *new_ilower + 1 );
    for ( i = 0; i <= send_num; i++ )
    {
        MPI_Isend( send_buff[i] , send_buff[i][0] + 1 , braid_MPI_INT, send_rank[i] , 20, comm, &req_last[i] );
    }
    num_messages = *new_iupper - *new_ilower + 1;
    braid_Int *recv_buff = _braid_CTAlloc( braid_Int , *new_iupper - *new_ilower + 5 );
    while ( num_messages > 0 )
    {
        MPI_Recv( recv_buff, *new_iupper - *new_ilower + 5, braid_MPI_INT, MPI_ANY_SOURCE, 20, comm, &status );
        for ( i = 0; i < recv_buff[0]; i++ )
            (*refine_map_r)[ recv_buff[ 1 + i ] - *new_ilower ] = status.MPI_SOURCE ;
        num_messages -= recv_buff[0];
    }
    free( recv_buff );


    if ( send_num >= 0  )
    {
        MPI_Waitall( send_num+1 , req_last, MPI_STATUS_IGNORE);
        for( i = 0; i<= send_num; i++ )
            free( send_buff[i] );
    }

    free(send_buff);
    free(send_rank);
    free( req_last );
    free( send_buffer_1);
    free( send_req_1 );
    free( need_procs ); //1
    free( load_sends ); //0
    free( gupper ); //9
    free( ilower ); //8
    free( iupper ); //7
    free( cfactor ); //6

    MPI_Barrier(comm);
    return _braid_error_flag;
}


