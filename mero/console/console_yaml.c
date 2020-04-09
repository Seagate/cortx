/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original creation date: 22/03/2011
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/string.h"  /* m0_streq */

#include "console/console_yaml.h"

static void yaml_parser_error_detect(const yaml_parser_t * parser)
{
        M0_PRE(parser != NULL);

        switch (parser->error) {
        case YAML_MEMORY_ERROR:
            fprintf(stderr, "Memory error: Not enough memory for parsing\n");
            break;
        case YAML_READER_ERROR:
            if (parser->problem_value != -1)
                fprintf(stderr, "Reader error: %s: #%X at %lu\n",
                        parser->problem, parser->problem_value,
                        parser->problem_offset);
            else
                fprintf(stderr, "Reader error: %s at %lu\n",
                        parser->problem, parser->problem_offset);
            break;
        case YAML_SCANNER_ERROR:
            if (parser->context)
                fprintf(stderr, "Scanner error: %s at line %lu, column %lu"
                        " %s at line %lu, column %lu\n",
                        parser->context, parser->context_mark.line+1,
                        parser->context_mark.column+1, parser->problem,
                        parser->problem_mark.line+1,
                        parser->problem_mark.column+1);
            else
                fprintf(stderr, "Scanner error: %s at line %lu, column %lu\n",
                        parser->problem, parser->problem_mark.line+1,
                        parser->problem_mark.column+1);
            break;
        case YAML_PARSER_ERROR:
            if (parser->context)
                fprintf(stderr, "Parser error: %s at line %lu, column %lu"
                        " %s at line %lu, column %lu\n",
                        parser->context, parser->context_mark.line+1,
                        parser->context_mark.column+1,parser->problem,
                        parser->problem_mark.line+1,
                        parser->problem_mark.column+1);
            else
                fprintf(stderr, "Parser error: %s at line %lu, column %lu\n",
                        parser->problem, parser->problem_mark.line+1,
                        parser->problem_mark.column+1);
            break;
        case YAML_COMPOSER_ERROR:
        case YAML_WRITER_ERROR:
        case YAML_EMITTER_ERROR:
        case YAML_NO_ERROR:
            break;
        default:
                M0_IMPOSSIBLE("Invalid error");
        }
}

/**
   @addtogroup console_yaml
   @{
*/

static struct m0_cons_yaml_info yaml_info;
/** enable/disable yaml support */
M0_INTERNAL bool yaml_support;

M0_INTERNAL int m0_cons_yaml_init(const char *file_path)
{
	int          rc;
	yaml_node_t *root_node;

	M0_ENTRY("file_path=`%s'", file_path);
	M0_PRE(file_path != NULL);

	yaml_info.cyi_file = fopen(file_path, "r");

	if (yaml_info.cyi_file == NULL) {
		perror("Failed to open file ");
		printf("%s, errno = %d\n", file_path, errno);
		goto error;
	}
	/* Initialize parser */
	rc = yaml_parser_initialize(&yaml_info.cyi_parser);
	if (rc != 1) {
		fprintf(stderr, "Failed to initialize parser!\n");
		fclose(yaml_info.cyi_file);
		goto error;
	}

	/* Set input file */
	yaml_parser_set_input_file(&yaml_info.cyi_parser, yaml_info.cyi_file);

	/* Load document */
	rc = yaml_parser_load(&yaml_info.cyi_parser, &yaml_info.cyi_document);
        if (rc != 1) {
		yaml_parser_delete(&yaml_info.cyi_parser);
		fclose(yaml_info.cyi_file);
		fprintf(stderr, "yaml parser load failed!!\n");
		goto error;
	}

	root_node = yaml_document_get_root_node(&yaml_info.cyi_document);
        if (root_node == NULL) {
                yaml_document_delete(&yaml_info.cyi_document);
		yaml_parser_delete(&yaml_info.cyi_parser);
		fclose(yaml_info.cyi_file);
                fprintf(stderr, "document get root node failed\n");
		goto error;
        }

	yaml_info.cyi_current = yaml_info.cyi_document.nodes.start;
	yaml_support = true;

	return M0_RC(0);
error:
	yaml_parser_error_detect(&yaml_info.cyi_parser);
	return M0_RC(-EINVAL);
}

M0_INTERNAL void m0_cons_yaml_fini(void)
{
	yaml_support = false;
	yaml_document_delete(&yaml_info.cyi_document);
	yaml_parser_delete(&yaml_info.cyi_parser);
	fclose(yaml_info.cyi_file);
}

static yaml_node_t *search_node(const char *name)
{
	yaml_document_t *doc  = &yaml_info.cyi_document;
	yaml_node_t     *node = yaml_info.cyi_current;
	unsigned char   *data_value;

	M0_ENTRY("name=`%s'", name);
	for ( ; node < doc->nodes.top; node++) {
		if (node->type == YAML_SCALAR_NODE) {
			data_value = node->data.scalar.value;
			if (m0_streq((const char *)data_value, name)) {
				node++;
				yaml_info.cyi_current = node;
				M0_LEAVE("found");
				return node;
			}
		}
	}
	M0_LEAVE("not found");
	return NULL;
}

M0_INTERNAL void *m0_cons_yaml_get_value(const char *name)
{
	yaml_node_t *node = search_node(name);
	return node == NULL ? NULL : node->data.scalar.value;
}

M0_INTERNAL int m0_cons_yaml_set_value(const char *name, void *data)
{
	return M0_ERR(-ENOTSUP);
}

/** @} end of console_yaml group */
#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
