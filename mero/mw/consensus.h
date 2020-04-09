/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 04/20/2010
 */

#pragma once

#ifndef __MERO_MW_CONSENSUS_H__
#define __MERO_MW_CONSENSUS_H__


struct m0_m_container;
struct m0_id_factory;
struct m0_tx;

/**
   @defgroup consensus Consensus
   @{
*/

struct m0_consensus_domain;
struct m0_consensus_proposer;
struct m0_consensus_acceptor;
struct m0_consensus;

struct m0_consensus_acceptor_ops {
	int  (*cacc_is_value_ok)(struct m0_consensus_acceptor *acc,
				 const struct m0_consensus *cons);
	void (*cacc_reached)(struct m0_consensus_acceptor *acc,
			     struct m0_tx *tx, const struct m0_consensus *cons);
};

M0_INTERNAL int m0_consensus_domain_init(struct m0_consensus_domain **domain);
M0_INTERNAL void m0_consensus_domain_fini(struct m0_consensus_domain *domain);
M0_INTERNAL int m0_consensus_domain_add(struct m0_consensus_domain *domain,
					struct m0_server *server);

M0_INTERNAL int m0_consensus_proposer_init(struct m0_consensus_proposer
					   **proposer,
					   struct m0_id_factory *idgen);
M0_INTERNAL void m0_consensus_proposer_fini(struct m0_consensus_proposer
					    *proposer);

M0_INTERNAL int m0_consensus_acceptor_init(struct m0_consensus_acceptor
					   **acceptor,
					   struct m0_m_container *store,
					   const struct
					   m0_consensus_acceptor_ops *ops);
M0_INTERNAL void m0_consensus_acceptor_fini(struct m0_consensus_acceptor
					    *acceptor);

M0_INTERNAL int m0_consensus_init(struct m0_consensus **cons,
				  struct m0_consensus_proposer *prop,
				  const struct m0_buf *val);
M0_INTERNAL void m0_consensus_fini(struct m0_consensus *cons);

M0_INTERNAL int m0_consensus_establish(struct m0_consensus_proposer *proposer,
				       struct m0_consensus *cons);

M0_INTERNAL struct m0_buf *m0_consensus_value(const struct m0_consensus *cons);

/** @} end of consensus group */

/* __MERO_MW_CONSENSUS_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
