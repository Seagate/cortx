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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 02/22/2012
 */


#include <stdio.h>         /* printf */

#ifdef ENABLE_FAULT_INJECTION

#include <stdlib.h>        /* random */
#include <unistd.h>        /* getpid */
#include <err.h>           /* warn */
#include <yaml.h>

#include "lib/memory.h"    /* m0_free */
#include "lib/mutex.h"     /* m0_mutex */
#include "lib/time.h"      /* m0_time_now */
#include "lib/string.h"    /* m0_strdup */
#include "lib/errno.h"     /* ENOMEM */
#include "lib/finject.h"
#include "lib/finject_internal.h"


M0_INTERNAL int m0_fi_init(void)
{
	unsigned int random_seed;

	m0_mutex_init(&fi_states_mutex);

	/*
	 * Initialize pseudo random generator, which is used in M0_FI_RANDOM
	 * triggering algorithm
	 */
	random_seed = m0_time_now() ^ getpid();
	srandom(random_seed);

	fi_states_init();

	return 0;
}

M0_INTERNAL void m0_fi_fini(void)
{
	fi_states_fini();
	m0_mutex_fini(&fi_states_mutex);
}

enum {
	FI_RAND_PROB_SCALE   = 100,
};

/**
 * Returns random value in range [0..FI_RAND_PROB_SCALE]
 */
M0_INTERNAL uint32_t fi_random(void)
{
	return (double)random() / RAND_MAX * FI_RAND_PROB_SCALE;
}

M0_INTERNAL void m0_fi_print_info(void)
{
	int i;

	const struct m0_fi_fpoint_state *state;
	struct m0_fi_fpoint_state_info   si;

	printf("%s", m0_fi_states_headline[0]);
	printf("%s", m0_fi_states_headline[1]);

	for (i = 0; i < m0_fi_states_get_free_idx(); ++i) {

		state = &m0_fi_states_get()[i];
		m0_fi_states_get_state_info(state, &si);

		printf(m0_fi_states_print_format,
			si.si_idx, si.si_enb, si.si_total_hit_cnt,
			si.si_total_trigger_cnt, si.si_hit_cnt,
			si.si_trigger_cnt, si.si_type, si.si_data, si.si_module,
			si.si_file, si.si_line_num, si.si_func, si.si_tag);
	}

	return;
}

M0_INTERNAL int m0_fi_enable_fault_point(const char *str)
{
	int  rc = 0;
	int  i;
	char *s;
	char *token;
	char *subtoken;
	char *token_saveptr = NULL;
	char *subtoken_saveptr = NULL;

	const char *func;
	const char *tag;
	const char *type;
	const char *data1;
	const char *data2;
	const char **fp_map[] = { &func, &tag, &type, &data1, &data2 };

	struct m0_fi_fpoint_data data = { 0 };

	if (str == NULL)
		return 0;

	s = m0_strdup(str);
	if (s == NULL)
		return -ENOMEM;

	while (true) {
		func = tag = type = data1 = data2 = NULL;

		token = strtok_r(s, ",", &token_saveptr);
		if (token == NULL)
			break;

		subtoken = token;
		for (i = 0; i < sizeof fp_map; ++i) {
			subtoken = strtok_r(token, ":", &subtoken_saveptr);
			if (subtoken == NULL)
				break;

			*fp_map[i] = subtoken;

			/*
			 * token should be NULL for subsequent strtok_r(3) calls
			 */
			token = NULL;
		}

		if (func == NULL || tag == NULL || type == NULL) {
			warn("Incorrect fault point specification\n");
			rc = -EINVAL;
			goto out;
		}

		data.fpd_type = m0_fi_fpoint_type_from_str(type);
		if (data.fpd_type == M0_FI_INVALID_TYPE) {
			warn("Incorrect fault point type '%s'\n", type);
			rc = -EINVAL;
			goto out;
		}

		if (data.fpd_type == M0_FI_RANDOM) {
			if (data1 == NULL) {
				warn("No probability was specified"
						" for 'random' FP type\n");
				rc = -EINVAL;
				goto out;
			}
			data.u.fpd_p = atoi(data1);
		} else if (data.fpd_type == M0_FI_OFF_N_ON_M) {
			if (data1 == NULL || data2 == NULL) {
				warn("No N or M was specified"
						" for 'off_n_on_m' FP type\n");
				rc = -EINVAL;
				goto out;
			}
			data.u.s1.fpd_n = atoi(data1);
			data.u.s1.fpd_m = atoi(data2);
		}

		m0_fi_enable_generic(func, tag, &data);

		/* s should be NULL for subsequent strtok_r(3) calls */
		s = NULL;
	}
out:
	m0_free(s);
	return rc;
}

static inline const char *pair_key(yaml_document_t *doc, yaml_node_pair_t *pair)
{
	return (const char*)yaml_document_get_node(doc, pair->key)->data.scalar.value;
}

static inline const char *pair_val(yaml_document_t *doc, yaml_node_pair_t *pair)
{
	return (const char*)yaml_document_get_node(doc, pair->value)->data.scalar.value;
}

static int extract_fpoint_data(yaml_document_t *doc, yaml_node_t *node,
			       const char **func, const char **tag,
			       struct m0_fi_fpoint_data *data)
{
	yaml_node_pair_t *pair;

	for (pair = node->data.mapping.pairs.start;
	     pair < node->data.mapping.pairs.top; pair++) {
		const char *key = pair_key(doc, pair);
		const char *val = pair_val(doc, pair);
		if (strcmp(key, "func") == 0) {
			*func = val;
		} else if (strcmp(key, "tag") == 0) {
			*tag = val;
		} else if (strcmp(key, "type") == 0) {
			data->fpd_type = m0_fi_fpoint_type_from_str(val);
			if (data->fpd_type == M0_FI_INVALID_TYPE) {
				warn("Incorrect FP type '%s'\n", val);
				return -EINVAL;
			}
		}
		else if (strcmp(key, "p") == 0) {
			data->u.fpd_p = atoi(val);
		} else if (strcmp(key, "n") == 0) {
			data->u.s1.fpd_n = atoi(val);
		} else if (strcmp(key, "m") == 0) {
			data->u.s1.fpd_m = atoi(val);
		} else {
			warn("Incorrect key '%s' in yaml file\n", key);
			return -EINVAL;
		}
	}

	return 0;
}

static int process_yaml(yaml_document_t *doc)
{
	int          rc;
	yaml_node_t  *node;
	const char   *func = 0;
	const char   *tag = 0;

	struct m0_fi_fpoint_data data = { 0 };

	for (node = doc->nodes.start; node < doc->nodes.top; node++)
		if (node->type == YAML_MAPPING_NODE) {
			rc = extract_fpoint_data(doc, node, &func, &tag, &data);
			if (rc != 0)
				return rc;
			m0_fi_enable_generic(strdup(func), m0_strdup(tag), &data);
		}

	return 0;
}

M0_INTERNAL int m0_fi_enable_fault_points_from_file(const char *file_name)
{
	int rc = 0;
	FILE *f;
	yaml_parser_t parser;
	yaml_document_t document;

	f = fopen(file_name, "r");
	if (f == NULL) {
		warn("Failed to open fault point yaml file '%s'", file_name);
		return -ENOENT;
	}

	rc = yaml_parser_initialize(&parser);
	if (rc != 1) {
		warn("Failed to init yaml parser\n");
		rc = -EINVAL;
		goto fclose;
	}

	yaml_parser_set_input_file(&parser, f);

	rc = yaml_parser_load(&parser, &document);
	if (rc != 1) {
		warn("Incorrect YAML file\n");
		rc = -EINVAL;
		goto pdel;
	}

	rc = process_yaml(&document);

	yaml_document_delete(&document);
pdel:
	yaml_parser_delete(&parser);
fclose:
	fclose(f);

	return rc;
}

#else /* ENABLE_FAULT_INJECTION */

int m0_fi_enable_fault_point(const char *str)
{
	return 0;
}

M0_INTERNAL int m0_fi_enable_fault_points_from_file(const char *file_name)
{
	return 0;
}

M0_INTERNAL int m0_fi_init(void)
{
	return 0;
}

M0_INTERNAL void m0_fi_fini(void)
{
}

M0_INTERNAL void m0_fi_print_info(void)
{
	fprintf(stderr, "Fault injection is not available, because it was"
			" disabled during build\n");
}

#endif /* ENABLE_FAULT_INJECTION */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
