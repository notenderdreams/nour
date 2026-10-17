#include "deps.h"
#include "arena.h"
#include "error.h"
#include <string.h>

/* Safety valve for pathological graphs; recursion is bounded above by graph
 * depth, so this also bounds stack usage cleanly with a diagnostic. */
#define DEP_MAX_DEPTH 4096

typedef struct DepNode DepNode;

struct DepNode {
	Target	 *target;
	DepNode **deps;
	usize	  dep_count;
	enum {
		DEP_STATE_UNVISITED = 0,
		DEP_STATE_VISITING,
		DEP_STATE_VISITED,
	} state;
};

typedef struct {
	DepNode **nodes;
	usize	  count;
} DepGraph;

static void **get_target_deps(const Target *t)
{
	if (!t)
		return NULL;
	if (t->kind == T_EXECUTABLE) {
		return ((Executable *)t)->deps;
	} else if (t->kind == T_LIBRARY) {
		return ((Library *)t)->deps;
	}
	return NULL;
}

static DepNode *find_node(const DepGraph *graph, const Target *target)
{
	if (!target)
		return NULL;
	for (usize i = 0; i < graph->count; ++i) {
		if (graph->nodes[i]->target == target) {
			return graph->nodes[i];
		}
		if (graph->nodes[i]->target->name && target->name &&
			strcmp(graph->nodes[i]->target->name, target->name) == 0) {
			return graph->nodes[i];
		}
	}
	return NULL;
}

static Result build_graph(const Project *proj, Arena *a, DepGraph *out_graph)
{
	usize total_targets = 0;
	if (proj->targets) {
		while (proj->targets[total_targets])
			total_targets++;
	}

	out_graph->count = total_targets;
	out_graph->nodes = arena_alloc(a, total_targets * sizeof(DepNode *));

	// First pass: create all DepNode entries
	for (usize i = 0; i < total_targets; ++i) {
		DepNode *node = arena_alloc(a, sizeof(DepNode));
		node->target = (Target *)proj->targets[i];
		node->deps = NULL;
		node->dep_count = 0;
		node->state = DEP_STATE_UNVISITED;
		out_graph->nodes[i] = node;
	}

	// Validate names: required for lookup by name and must be unique.
	for (usize i = 0; i < total_targets; ++i) {
		const char *ni = out_graph->nodes[i]->target->name;
		if (!ni)
			return Err("target declaration at index %zu has no name", i);
		for (usize j = i + 1; j < total_targets; ++j) {
			const char *nj = out_graph->nodes[j]->target->name;
			if (nj && strcmp(ni, nj) == 0) {
				return Err(ERR_DUPLICATE_TARGET, ni);
			}
		}
	}

	// Second pass: connect dependencies
	for (usize i = 0; i < total_targets; ++i) {
		DepNode *node = out_graph->nodes[i];
		void **raw_deps = get_target_deps(node->target);
		if (!raw_deps)
			continue;

		usize n_deps = 0;
		while (raw_deps[n_deps])
			n_deps++;

		node->dep_count = n_deps;
		node->deps = arena_alloc(a, n_deps * sizeof(DepNode *));

		for (usize j = 0; j < n_deps; ++j) {
			Target	*dep_t = (Target *)raw_deps[j];
			DepNode *dep_node = find_node(out_graph, dep_t);
			if (!dep_node) {
				const char *dep_name = (dep_t && dep_t->name) ? dep_t->name : "unknown";
				return Err(ERR_INVALID_DEP, node->target->name, dep_name);
			}
			node->deps[j] = dep_node;
		}
	}

	return Ok(NULL);
}

typedef struct {
	DepNode **stack;
	usize	  len;
	usize	  cap;
} VisitStack;

static Result
dfs_sort(DepNode *node, VisitStack *vstack, Target **order, usize *order_len, usize depth, Arena *a)
{
	if (depth > DEP_MAX_DEPTH) {
		return Err(ERR_DEP_DEPTH, (int)DEP_MAX_DEPTH, node->target->name);
	}

	if (node->state == DEP_STATE_VISITED) {
		return Ok(NULL);
	}

	if (node->state == DEP_STATE_VISITING) {
		// Circular dependency detected!
		usize start_idx = 0;
		for (usize i = 0; i < vstack->len; ++i) {
			if (vstack->stack[i] == node) {
				start_idx = i;
				break;
			}
		}

		strbuf_open(a);
		for (usize i = start_idx; i < vstack->len; ++i) {
			strbuf_printf(a, "%s -> ", vstack->stack[i]->target->name);
		}
		strbuf_printf(a, "%s", node->target->name);
		const char *cycle_path = strbuf_finish(a);

		return Err(ERR_CIRCULAR_DEP, cycle_path);
	}

	node->state = DEP_STATE_VISITING;
	vstack->stack[vstack->len++] = node;

	for (usize i = 0; i < node->dep_count; ++i) {
		Try(dfs_sort(node->deps[i], vstack, order, order_len, depth + 1, a));
	}

	vstack->len--;
	node->state = DEP_STATE_VISITED;
	order[(*order_len)++] = node->target;

	return Ok(NULL);
}

Result deps_resolve(const Project *proj, const char *target_name, Arena *a, DepPlan *out_plan)
{
	DepGraph graph = {0};
	Try(build_graph(proj, a, &graph));

	Target **order = arena_alloc(a, graph.count * sizeof(Target *));
	usize order_len = 0;

	DepNode **stack_mem = arena_alloc(a, (graph.count + 1) * sizeof(DepNode *));
	VisitStack vstack = {.stack = stack_mem, .len = 0, .cap = graph.count + 1};

	if (target_name && target_name[0]) {
		DepNode *root = NULL;
		for (usize i = 0; i < graph.count; ++i) {
			if (graph.nodes[i]->target->name &&
				strcmp(graph.nodes[i]->target->name, target_name) == 0) {
				root = graph.nodes[i];
				break;
			}
		}
		if (!root) {
			return Err(ERR_TARGET_NOT_FOUND, target_name, proj->name);
		}

		Try(dfs_sort(root, &vstack, order, &order_len, 1, a));
	} else {
		for (usize i = 0; i < graph.count; ++i) {
			if (graph.nodes[i]->state == DEP_STATE_UNVISITED) {
				Try(dfs_sort(graph.nodes[i], &vstack, order, &order_len, 1, a));
			}
		}
	}

	out_plan->targets = order;
	out_plan->count = order_len;
	return Ok(NULL);
}

static bool str_list_contains(const char **list, usize count, const char *val)
{
	if (!val)
		return false;
	for (usize i = 0; i < count; ++i) {
		if (!list[i])
			continue;
		if (strcmp(list[i], val) == 0)
			return true;
	}
	return false;
}

static bool lib_list_contains(const Library **list, usize count, const Library *lib)
{
	if (!lib)
		return false;
	for (usize i = 0; i < count; ++i) {
		if (!list[i])
			continue;
		if (list[i] == lib || (list[i]->name && lib->name && strcmp(list[i]->name, lib->name) == 0))
			return true;
	}
	return false;
}

static usize count_cstr_list(const char **list)
{
	usize n = 0;
	if (list) {
		for (usize i = 0; list[i]; ++i)
			++n;
	}
	return n;
}

Result deps_collect_transitive(
	const Project *proj, const Target *target, Arena *a, DepTransitive *out_trans
) {
	memset(out_trans, 0, sizeof(*out_trans));
	if (!target)
		return Ok(NULL);

	// Resolve target's full sub-graph
	DepPlan sub_plan = {0};
	Try(deps_resolve(proj, target->name, a, &sub_plan));

	// Pass 1: count exact upper bounds (no dedup, so these never under-size).
	usize inc_cap = 0, def_cap = 0, ld_cap = 0, st_cap = 0, sh_cap = 0;
	for (usize idx = sub_plan.count; idx > 0; --idx) {
		Target *t = sub_plan.targets[idx - 1];
		if (t == target)
			continue; // Do not collect own public flags as transitive flags

		if (t->kind == T_LIBRARY) {
			const Library *lib = (const Library *)t;
			inc_cap += count_cstr_list(lib->public_includes);
			def_cap += count_cstr_list(lib->public_defines);
			ld_cap += count_cstr_list(lib->ldflags);
			if (lib->type == STATIC)
				st_cap++;
			else if (lib->type == SHARED)
				sh_cap++;
		} else if (t->kind == T_EXECUTABLE) {
			ld_cap += count_cstr_list(((const Executable *)t)->ldflags);
		}
	}

	out_trans->public_includes = arena_alloc(a, (inc_cap ? inc_cap : 1) * sizeof(const char *));
	out_trans->public_defines = arena_alloc(a, (def_cap ? def_cap : 1) * sizeof(const char *));
	out_trans->static_libs = arena_alloc(a, (st_cap ? st_cap : 1) * sizeof(const Library *));
	out_trans->shared_libs = arena_alloc(a, (sh_cap ? sh_cap : 1) * sizeof(const Library *));
	out_trans->ldflags = arena_alloc(a, (ld_cap ? ld_cap : 1) * sizeof(const char *));

	// Pass 2: collect includes, defines, ldflags, and libraries in reverse
	// topological order (dependents before dependencies).
	for (usize idx = sub_plan.count; idx > 0; --idx) {
		Target *t = sub_plan.targets[idx - 1];
		if (t == target)
			continue; // Do not collect own public flags as transitive flags

		if (t->kind == T_LIBRARY) {
			const Library *lib = (const Library *)t;
			if (lib->public_includes) {
				for (const char **inc = lib->public_includes; *inc; ++inc) {
					if (!str_list_contains(
							out_trans->public_includes, out_trans->num_includes, *inc
						)) {
						out_trans->public_includes[out_trans->num_includes++] = *inc;
					}
				}
			}
			if (lib->public_defines) {
				for (const char **d = lib->public_defines; *d; ++d) {
					if (!str_list_contains(out_trans->public_defines, out_trans->num_defines, *d)) {
						out_trans->public_defines[out_trans->num_defines++] = *d;
					}
				}
			}
			if (lib->ldflags) {
				for (const char **ld = lib->ldflags; *ld; ++ld) {
					if (!str_list_contains(out_trans->ldflags, out_trans->num_ldflags, *ld)) {
						out_trans->ldflags[out_trans->num_ldflags++] = *ld;
					}
				}
			}
			if (lib->type == STATIC) {
				if (!lib_list_contains(out_trans->static_libs, out_trans->num_static_libs, lib)) {
					out_trans->static_libs[out_trans->num_static_libs++] = lib;
				}
			} else if (lib->type == SHARED) {
				if (!lib_list_contains(out_trans->shared_libs, out_trans->num_shared_libs, lib)) {
					out_trans->shared_libs[out_trans->num_shared_libs++] = lib;
				}
			}
		} else if (t->kind == T_EXECUTABLE) {
			const Executable *exe = (const Executable *)t;
			if (exe->ldflags) {
				for (const char **ld = exe->ldflags; *ld; ++ld) {
					if (!str_list_contains(out_trans->ldflags, out_trans->num_ldflags, *ld)) {
						out_trans->ldflags[out_trans->num_ldflags++] = *ld;
					}
				}
			}
		}
	}

	return Ok(NULL);
}
