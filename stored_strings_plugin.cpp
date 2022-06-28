/*
 * Licensed under the GPL v2.
 *
 * GCC plugin to detect assignments of const strings to several kinds of
 * non-local variables, e.g.:
 *
 *   net->sctp.sctp_hmac_alg = "md5";
 * (Linux kernel, net/sctp/sysctl.c)
 *
 * False positives should be rare. However, the plugin might miss some of
 * such assignments.
 *
 * The header files are from the Linux kernel 5.19-rc3; the structure and
 * some functions of the plugin are based on "structleak" plugin from the
 * kernel.
 */

#include "gcc-common.h"

//<>
//#include <stdio.h> // for debugging only
//<>

__visible int plugin_is_GPL_compatible;

static struct plugin_info stored_strings_plugin_info = {
	.version	= "0.1",
	.help		= "disable\tdo not activate plugin\n",
};

static bool verbose = true;

static tree get_base(tree expr)
{
	tree base = NULL;

	switch TREE_CODE(expr) {
	case SSA_NAME:
	case VAR_DECL:
		base = expr;
		break;
	case INDIRECT_REF:
	case MEM_REF:
		base = TREE_OPERAND(expr, 0);
		break;
	case ARRAY_REF:
	case COMPONENT_REF:
		base = get_base(TREE_OPERAND(expr, 0));
		break;
	default:
		break;
	}
	return base;
}

/*
 * To avoid false positives, we assume that if GCC can tell nothing about
 * a variable, the variable is local (cannot escape the current function).
 */

static bool pointed_data_can_escape(tree var)
{
	struct ptr_info_def *pi = SSA_NAME_PTR_INFO(var);

	if (!pi)
		return false;
	return pt_solution_includes_global(&pi->pt);
}

static bool can_escape(tree lhs)
{
	tree base;
	tree var;

	if (VAR_P(lhs) && is_global_var(lhs))
		return true;

	base = get_base(lhs);
	if (!base)
		return false;

	if (VAR_P(base) && is_global_var(base))
		return true; /* can happen when processing ARRAY_REF */

	/*
	 * If we get here, base should be a SSA_NAME - check that, just in case.
	 */
	if (TREE_CODE(base) != SSA_NAME)
		return false;

	if (POINTER_TYPE_P(TREE_TYPE(base)) && pointed_data_can_escape(base))
		return true;

	var = SSA_NAME_VAR(base);
	if (var != NULL_TREE && is_global_var(var))
		return true;

	return false;
}

/*
 * true if the current function is marked with '__init',
 * a.k.a. __attribute__((__section__(".init.text"))), false otherwise.
 *
 * Livepatching cannot touch such functions, so it is not needed to check
 * them here.
 */
static bool func_marked_with_init(void)
{
	tree attr_section;
	const char *args;

	attr_section = lookup_attribute("section",
					DECL_ATTRIBUTES(current_function_decl));
	if (attr_section == NULL_TREE)
		return false;

	args = sorted_attr_string(TREE_VALUE(attr_section));
	if (!args) /* should not get here, but... */
		return false;

	return !strcmp(args, ".init.text");
}

static unsigned int stored_strings_execute(void)
{
	basic_block bb;
	gimple_stmt_iterator gsi;

	if (func_marked_with_init())
		return 0; /* Nothing to do for __init funcs. */

	FOR_EACH_BB_FN(bb, cfun) {
		for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
			gimple stmt = gsi_stmt(gsi);
			tree rhs;

			/* Check only assignments of a single value to a variable */
			if (!gimple_assign_single_p(stmt))
				continue;

			rhs = gimple_assign_rhs1(stmt);
			/*
			 * ...and the assigned value must be an address of a
			 * string constant, for now.
			 */
			if (TREE_CODE(rhs) != ADDR_EXPR ||
			    TREE_CODE(TREE_OPERAND(rhs, 0)) != STRING_CST) {
				continue;
			}

			if (!can_escape(gimple_get_lhs(stmt))) {
				continue;
			}

			if (verbose) {
				inform(gimple_location(stmt),
				"got an assignment of a const char * to a global variable.");
			}
		}
	}
	return 0;
}

#define PASS_NAME stored_strings
#define NO_GATE
#define PROPERTIES_REQUIRED PROP_ssa | PROP_cfg
#define TODO_FLAGS_START TODO_rebuild_alias
#define TODO_FLAGS_FINISH TODO_verify_il | TODO_verify_ssa | TODO_verify_stmts | TODO_dump_func | TODO_remove_unused_locals | TODO_update_ssa | TODO_ggc_collect | TODO_verify_flow
#include "gcc-generate-gimple-pass.h"

__visible int plugin_init(struct plugin_name_args *plugin_info, struct plugin_gcc_version *version)
{
	int i;
	const char * const plugin_name = plugin_info->base_name;
	const int argc = plugin_info->argc;
	const struct plugin_argument * const argv = plugin_info->argv;
	bool enable = true;

	PASS_INFO(stored_strings, "early_optimizations", 1, PASS_POS_INSERT_BEFORE);

	if (!plugin_default_version_check(version, &gcc_version)) {
		error(G_("incompatible gcc/plugin versions"));
		return 1;
	}

	if (strncmp(lang_hooks.name, "GNU C", 5) && !strncmp(lang_hooks.name, "GNU C+", 6)) {
		inform(UNKNOWN_LOCATION, G_("%s supports C only, not %s"), plugin_name, lang_hooks.name);
		enable = false;
	}

	for (i = 0; i < argc; ++i) {
		if (!strcmp(argv[i].key, "disable")) {
			enable = false;
			continue;
		}
		error(G_("unknown option \"-fplugin-arg-%s-%s\""), plugin_name, argv[i].key);
	}

	register_callback(plugin_name, PLUGIN_INFO, NULL, &stored_strings_plugin_info);
	if (enable) {
		register_callback(plugin_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &stored_strings_pass_info);
	}

	return 0;
}
