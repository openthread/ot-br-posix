/*
 *
 *    Copyright (c) 2010-2012 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    Description:
 *		This is the header for the configuration file parser.
 *
 */

#ifndef __WPAN_CONFIG_FILE_H__
#define __WPAN_CONFIG_FILE_H__ 1

__BEGIN_DECLS
extern char* get_next_arg(char *buf, char **rest);

typedef int (*config_param_set_func)(void* context, const char* key, const char* value);

extern int read_config(const char* filename, config_param_set_func setter, void* context);

extern int fread_config(FILE* file, config_param_set_func setter, void* context);
__END_DECLS

#endif // __WPAN_CONFIG_FILE_H__
