/*-
 * Copyright (c) 2015 Allan Jude <allanjude@FreeBSD.org>
 * Copyright (c) 2015-2018 Marcelo Araujo <araujo@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <sysexits.h>
#include <string.h>
#include <stdio.h>

#include "config.h"

/*
 * Check if the config file exists.
 */
int
check_config_file(const char *smbios_file)
{
	FILE *file;
	if ((file = fopen(smbios_file, "r"))) {
		fclose(file);
		return (0);
	}
	
	errx(EX_NOINPUT, "Config file %s not found", smbios_file);
}

/*
 * Load smbios config file.
 */
void
load_smbios_config(const char *smbios_file)
{
	check_config_file(smbios_file);

	struct ucl_parser *parser = NULL;
	parser = ucl_parser_new(UCL_PARSER_KEY_LOWERCASE |
			    UCL_PARSER_NO_IMPLICIT_ARRAYS);
	if (parser == NULL)
		errx(1, "Could not allocate ucl parser");

	if (smbios_file != NULL) {
		if (!ucl_parser_add_file_priority(parser, smbios_file, 5)) {
			if (errno != ENOENT)
				errx(EXIT_FAILURE, "Parse error in file %s: %s",
				    smbios_file, ucl_parser_get_error(parser));
			ucl_parser_free(parser);
		}
	}

	parse_smbios_config(parser);
}

/*
 * Parse the smbios configuration file.
 * We only support changes on table type1.
 *
 * XXX: Add support for table type3.
 */
int
parse_smbios_config(struct ucl_parser *p)
{
	const ucl_object_t *obj = NULL;
	ucl_object_iter_t it = NULL;
	const ucl_object_t *cur;
	const char *key;

	obj = ucl_parser_get_object(p);
	ucl_parser_free(p);

	if (obj == NULL || ucl_object_type(obj) != UCL_OBJECT)
		errx(EXIT_FAILURE, "Invalid configuration format.\n");

	it = ucl_object_iterate_new(obj);
	while ((cur = ucl_object_iterate_safe(it, true)) != NULL) {
		key = ucl_object_key(cur);
		/* smbios table type1 */
		if (strcasecmp(key, "manufacturer") == 0)
			smbios_type1_strings[0] = (char *)ucl_object_tostring(cur);
		if (strcasecmp(key, "product") == 0)
			smbios_type1_strings[1] = (char *)ucl_object_tostring(cur);
		if (strcasecmp(key, "version") == 0)
			smbios_type1_strings[2] = (char *)ucl_object_tostring(cur);
		if (strcasecmp(key, "serial") == 0)
			smbios_type1_strings[3] = (char *)ucl_object_tostring(cur);
		if (strcasecmp(key, "sku") == 0)
			smbios_type1_strings[4] = (char *)ucl_object_tostring(cur);
		if (strcasecmp(key, "family") == 0)
			smbios_type1_strings[5] = (char *)ucl_object_tostring(cur);
		/* End smbios table type1 */
	}
	ucl_object_iterate_free(it);

	return (0);
}
