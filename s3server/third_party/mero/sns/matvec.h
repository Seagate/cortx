/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 * Original creation date: 10/19/2010
 */

#pragma once

#ifndef __MERO_SNS_MAT_VEC_H__
#define __MERO_SNS_MAT_VEC_H__

#include "lib/types.h"
#include "parity_ops.h"

/**
 * Represents math-vector of sz elements of type 'm0_parity_elem_t'
 */
struct m0_matvec {
	uint32_t          mv_size;
	m0_parity_elem_t *mv_vector;
};

M0_INTERNAL int m0_matvec_init(struct m0_matvec *v, uint32_t sz);
M0_INTERNAL void m0_matvec_fini(struct m0_matvec *v);
M0_INTERNAL void m0_matvec_print(const struct m0_matvec *vec);

/**
 * Gets element of vector 'v' in 'x' row
 *
 * @pre m0_matvec_init(v) has been called
 * @pre x < v->mv_size
 */
m0_parity_elem_t* m0_matvec_elem_get(const struct m0_matvec *v, uint32_t x);


/**
 * Represents math-matrix of m_width and m_height elements of type 'm0_parity_elem_t'
 */
struct m0_matrix {
	uint32_t m_width;
	uint32_t m_height;
	m0_parity_elem_t **m_matrix;
};

M0_INTERNAL int m0_matrix_init(struct m0_matrix *m, uint32_t w, uint32_t h);
M0_INTERNAL void m0_matrix_fini(struct m0_matrix *m);
M0_INTERNAL void m0_matrix_print(const struct m0_matrix *mat);

/**
 * Gets element of matrix 'm' in ('x','y') pos
 *
 * @pre m0_matrix_init(v) has been called
 * @pre x < m->m_width && y < m->m_height
 */
m0_parity_elem_t* m0_matrix_elem_get(const struct m0_matrix *m, uint32_t x, uint32_t y);

/**
 * Defines binary operation over matrix or vector element
 */
typedef m0_parity_elem_t (*m0_matvec_matrix_binary_operator_t)(m0_parity_elem_t,
							       m0_parity_elem_t);

/**
 * Apply operation 'f' to element of vector 'v' in row 'row' with const 'c':
 * v[row] = f(v[row], c);
 *
 * @pre m0_matvec_init(v) has been called
 */
M0_INTERNAL void m0_matvec_row_operate(struct m0_matvec *v, uint32_t row,
				       m0_parity_elem_t c,
				       m0_matvec_matrix_binary_operator_t f);

/**
 * Apply operation 'f' to every row element of matrix 'm' in row 'row' with const 'c':
 * m(row,i) = f(m(row,i), c);
 *
 * @pre m0_matvec_init(v) has been called
 */
M0_INTERNAL void m0_matrix_row_operate(struct m0_matrix *m, uint32_t row,
				       m0_parity_elem_t c,
				       m0_matvec_matrix_binary_operator_t f);

/**
 * Apply operation 'f' to every col element of matrix 'm' in col 'col' with const 'c':
 * m(i,col) = f(m(i,col), c);
 *
 * @pre m0_matrix_init(m) has been called
 */
M0_INTERNAL void m0_matrix_col_operate(struct m0_matrix *m, uint32_t col,
				       m0_parity_elem_t c,
				       m0_matvec_matrix_binary_operator_t f);

/**
 * Apply operation 'f' to every row element of matrix 'm' in row 'row0' with row 'row1' and
 * apply operation 'fi' to every row element of matrix 'm' in row 'rowi' with const 'ci':
 * m(row0,i) = f(f0(m(row0,i),c0), f1(m(row1,i),c1));
 *
 * @pre m0_matrix_init(m) has been called
 */
M0_INTERNAL void m0_matrix_rows_operate(struct m0_matrix *m, uint32_t row0,
					uint32_t row1,
					m0_matvec_matrix_binary_operator_t f0,
					m0_parity_elem_t c0,
					m0_matvec_matrix_binary_operator_t f1,
					m0_parity_elem_t c1,
					m0_matvec_matrix_binary_operator_t f);

/**
 * Apply operation 'f' to every row element of matrix 'm' in row 'row0' with row 'row1' and
 * apply operation 'fi' to every row element of matrix 'm' in row 'rowi' with const 'ci':
 * m(row0,i) = f(f0(m(row0,i),c0), m(row1,i));
 *
 * @pre m0_matrix_init(m)
 */
M0_INTERNAL void m0_matrix_rows_operate2(struct m0_matrix *m, uint32_t row0,
					 uint32_t row1,
					 m0_matvec_matrix_binary_operator_t f0,
					 m0_parity_elem_t c0,
					 m0_matvec_matrix_binary_operator_t f);

/**
 * Apply operation 'f' to every row element of matrix 'm' in row 'row0' with row 'row1' and
 * apply operation 'fi' to every row element of matrix 'm' in row 'rowi' with const 'ci':
 * m(row0,i) = f(m(row0,i), f1(m(row1,i),c1));
 *
 * @pre m0_matrix_init(m) has been called
 */
M0_INTERNAL void m0_matrix_rows_operate1(struct m0_matrix *m, uint32_t row0,
					 uint32_t row1,
					 m0_matvec_matrix_binary_operator_t f1,
					 m0_parity_elem_t c1,
					 m0_matvec_matrix_binary_operator_t f);

/**
 * Apply operation 'f' to every row element of vector 'v' in row 'row0' with row 'row1' and
 * apply operation 'fi' to every row element of vector 'v' in row 'rowi' with const 'ci':
 * m(row0,i) = f(f0(m(row0,i),c0), f1(m(row1,i),c1));
 *
 * @pre m0_matvec_init(v) has been called
 */
M0_INTERNAL void m0_matvec_rows_operate(struct m0_matvec *v, uint32_t row0,
					uint32_t row1,
					m0_matvec_matrix_binary_operator_t f0,
					m0_parity_elem_t c0,
					m0_matvec_matrix_binary_operator_t f1,
					m0_parity_elem_t c1,
					m0_matvec_matrix_binary_operator_t f);

/**
 * Apply operation 'f' to every row element of vector 'v' in row 'row0' with row 'row1' and
 * apply operation 'fi' to every row element of vector 'v' in row 'rowi' with const 'ci':
 * m(row0,i) = f(m(row0,i), f1(m(row1,i),c1));
 *
 * @pre m0_matvec_init(v) has been called
 */
M0_INTERNAL void m0_matvec_rows_operate1(struct m0_matvec *v, uint32_t row0,
					 uint32_t row1,
					 m0_matvec_matrix_binary_operator_t f1,
					 m0_parity_elem_t c1,
					 m0_matvec_matrix_binary_operator_t f);

/**
 * Apply operation 'f' to every row element of vector 'v' in row 'row0' with row 'row1' and
 * apply operation 'fi' to every row element of vector 'v' in row 'rowi' with const 'ci':
 * m(row0,i) = f(f0(m(row0,i),c0), m(row1,i));
 *
 * @pre m0_matvec_init(v) has been called
 */
M0_INTERNAL void m0_matvec_rows_operate2(struct m0_matvec *v, uint32_t row0,
					 uint32_t row1,
					 m0_matvec_matrix_binary_operator_t f0,
					 m0_parity_elem_t c0,
					 m0_matvec_matrix_binary_operator_t f);

/**
 * Apply operation 'f' to every col element of matrix 'm' in col 'col0' with col 'col1' and
 * apply operation 'fi' to every col element of matrix 'm' in col 'coli' with const 'ci':
 * m(i,col0) = f(f0(m(i,col0),c0), f1(m(i,col1),c1));
 *
 * @pre m0_matrix_init(m) has been called
 */
M0_INTERNAL void m0_matrix_cols_operate(struct m0_matrix *m, uint32_t col0,
					uint32_t col1,
					m0_matvec_matrix_binary_operator_t f0,
					m0_parity_elem_t c0,
					m0_matvec_matrix_binary_operator_t f1,
					m0_parity_elem_t c1,
					m0_matvec_matrix_binary_operator_t f);

/**
 * Swaps row 'r0' and row 'r1' each with other
 * @pre m0_matrix_init(m) has been called
 */
M0_INTERNAL void m0_matrix_swap_row(struct m0_matrix *m, uint32_t r0,
				    uint32_t r1);

/**
 * Swaps row 'r0' and row 'r1' each with other
 * @pre m0_matvec_init(v) has been called
 */
M0_INTERNAL void m0_matvec_swap_row(struct m0_matvec *v, uint32_t r0,
				    uint32_t r1);

/**
 * Multiplies matrix 'm' on vector 'v'
 * Returns vector 'r'
 * @param mul - multiplicaton function
 * @param add - addition function
 *
 * @pre m0_matvec_init(v) has been called
 * @pre m0_matrix_init(m) has been called
 * @pre m0_vec_init(r) has been called
 */
M0_INTERNAL void m0_matrix_vec_multiply(const struct m0_matrix *m,
					const struct m0_matvec *v,
					struct m0_matvec *r,
					m0_matvec_matrix_binary_operator_t mul,
					m0_matvec_matrix_binary_operator_t add);

/**
 * Returns submatrix of matrix 'mat' into 'submat', where 'x', 'y' - offsets
 * Uses 'submat' dimensions
 * @pre m0_matrix_init(mat) has been called
 * @pre m0_matrix_init(submat) has been called
 */
M0_INTERNAL void m0_matrix_submatrix_get(const struct m0_matrix *mat,
					 struct m0_matrix *submat,
					 uint32_t x, uint32_t y);

/**
 * Multiplies matrix ma with matrix mb and stores the result in matrix mc
 * @param[in]  ma
 * @param[in]  mb
 * @param[out] mc
 * @pre mc->m_height == ma->m_height
 * @pre mc->m_width == mb->m_width
 * @pre mc->m_matrix[i][j] == 0 \forall 0 <= i < mc->m_height
 *					0 <= j < mc->m_width
 */
M0_INTERNAL void m0_matrix_multiply(const struct m0_matrix *ma,
		                    const struct m0_matrix *mb,
				    struct m0_matrix *mc);

/**
 * Populates the input square matrix as an identity matrix.
 * @pre M0_MATRIX_IS_SQUARE(identity_matrix)
 */
M0_INTERNAL void m0_identity_matrix_fill(struct m0_matrix *identity_mat);

/**
 * Verifies whether input matrix is initialized or not.
 * @param[in] mat
 * @retval    'true' if mat->m_width > 0 && mat->m_height > 0.
 * @retval    'false' otherwise.
 */
M0_INTERNAL bool m0_matrix_is_init(const struct m0_matrix *mat);


/* Verifies whether input matrix has all elements zero. */
M0_INTERNAL bool m0_matrix_is_null(const struct m0_matrix *mat);

/**
 * Copies src_row from src to des_row in des.
 * @param[out] des	destination matrix
 * @param[in]  src	source matrix
 * @param[in]  des_row  destination row index
 * @param[in]  src_row  source row index
 * @pre dest->m_width == src->m_width
 */
M0_INTERNAL void m0_matrix_row_copy(struct m0_matrix *des,
				    const struct m0_matrix *src,
				    uint32_t des_row, uint32_t src_row);

M0_INTERNAL bool m0_matrix_is_square(const struct m0_matrix *mat);
/* __MERO_SNS_MAT_VEC_H__ */
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
