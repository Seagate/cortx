/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF XYRATEX TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF XYRATEX TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF XYRATEX LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF XYRATEX'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A XYRATEX REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author: Igor Perelyotov <igor.m.perelyotov@seagate.com>
 * Original creation date: 22-Apr-2015
 */

#include <Python.h>
#include "mero/ha.h"            /* m0_mero_ha */
#include "module/instance.h"
#include "net/net.h"
#include "net/buffer_pool.h"
#include "reqh/reqh.h"
#include "rpc/rpc_machine.h"
#include "spiel/spiel.h"

#define STRUCTS               \
	X(m0)                 \
	X(m0_net_domain)      \
	X(m0_net_buffer_pool) \
	X(m0_reqh)            \
	X(m0_reqh_init_args)  \
	X(m0_reqh_service)    \
	X(m0_rpc_machine)     \
	X(m0_mero_ha)         \
	X(m0_mero_ha_cfg)     \
	X(m0_spiel_tx)        \
	X(m0_spiel)

#define X(name)                                                 \
static PyObject *name ## __size(PyObject *self, PyObject *args) \
{                                                               \
	return PyInt_FromSize_t(sizeof(struct name));           \
}
STRUCTS
#undef X

static PyMethodDef methods[] = {
#define X(name) { #name "__size", name ## __size, METH_VARARGS, NULL },
STRUCTS
#undef X
	{NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initmero(void)
{
	(void)Py_InitModule("mero", methods);
}
