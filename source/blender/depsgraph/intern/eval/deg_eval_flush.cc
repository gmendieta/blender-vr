/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 *
 * Core routines for how the Depsgraph works.
 */

#include "intern/eval/deg_eval_flush.h"

// TODO(sergey): Use some sort of wrapper.
#include <deque>
#include <cmath>

#include "BKE_object.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"
#include "BLI_ghash.h"

extern "C" {
#include "DNA_object_types.h"

#include "DRW_engine.h"
} /* extern "C" */

#include "DEG_depsgraph.h"

#include "intern/debug/deg_debug.h"
#include "intern/depsgraph.h"
#include "intern/depsgraph_update.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_factory.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

#include "intern/eval/deg_eval_copy_on_write.h"

// Invalidate datablock data when update is flushed on it.
//
// The idea of this is to help catching cases when area is accessing data which
// is not yet evaluated, which could happen due to missing relations. The issue
// is that usually that data will be kept from previous frame, and it looks to
// be plausible.
//
// This ensures that data does not look plausible, making it much easier to
// catch usage of invalid state.
#undef INVALIDATE_ON_FLUSH

namespace DEG {

enum {
  ID_STATE_NONE = 0,
  ID_STATE_MODIFIED = 1,
};

enum {
  COMPONENT_STATE_NONE = 0,
  COMPONENT_STATE_SCHEDULED = 1,
  COMPONENT_STATE_DONE = 2,
};

typedef std::deque<OperationNode *> FlushQueue;

namespace {

void flush_init_operation_node_func(void *__restrict data_v,
                                    const int i,
                                    const ParallelRangeTLS *__restrict /*tls*/)
{
  Depsgraph *graph = (Depsgraph *)data_v;
  OperationNode *node = graph->operations[i];
  node->scheduled = false;
}

void flush_init_id_node_func(void *__restrict data_v,
                             const int i,
                             const ParallelRangeTLS *__restrict /*tls*/)
{
  Depsgraph *graph = (Depsgraph *)data_v;
  IDNode *id_node = graph->id_nodes[i];
  id_node->custom_flags = ID_STATE_NONE;
  GHASH_FOREACH_BEGIN (ComponentNode *, comp_node, id_node->components)
    comp_node->custom_flags = COMPONENT_STATE_NONE;
  GHASH_FOREACH_END();
}

BLI_INLINE void flush_prepare(Depsgraph *graph)
{
  {
    const int num_operations = graph->operations.size();
    ParallelRangeSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = 1024;
    BLI_task_parallel_range(0, num_operations, graph, flush_init_operation_node_func, &settings);
  }
  {
    const int num_id_nodes = graph->id_nodes.size();
    ParallelRangeSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = 1024;
    BLI_task_parallel_range(0, num_id_nodes, graph, flush_init_id_node_func, &settings);
  }
}

BLI_INLINE void flush_schedule_entrypoints(Depsgraph *graph, FlushQueue *queue)
{
  GSET_FOREACH_BEGIN (OperationNode *, op_node, graph->entry_tags) {
    queue->push_back(op_node);
    op_node->scheduled = true;
    DEG_DEBUG_PRINTF((::Depsgraph *)graph,
                     EVAL,
                     "Operation is entry point for update: %s\n",
                     op_node->identifier().c_str());
  }
  GSET_FOREACH_END();
}

BLI_INLINE void flush_handle_id_node(IDNode *id_node)
{
  id_node->custom_flags = ID_STATE_MODIFIED;
}

/* TODO(sergey): We can reduce number of arguments here. */
BLI_INLINE void flush_handle_component_node(IDNode *id_node,
                                            ComponentNode *comp_node,
                                            FlushQueue *queue)
{
  /* We only handle component once. */
  if (comp_node->custom_flags == COMPONENT_STATE_DONE) {
    return;
  }
  comp_node->custom_flags = COMPONENT_STATE_DONE;
  /* Tag all required operations in component for update, unless this is a
   * special component where we don't want all operations to be tagged.
   *
   * TODO(sergey): Make this a more generic solution. */
  if (comp_node->type != NodeType::PARTICLE_SETTINGS &&
      comp_node->type != NodeType::PARTICLE_SYSTEM) {
    for (OperationNode *op : comp_node->operations) {
      op->flag |= DEPSOP_FLAG_NEEDS_UPDATE;
    }
  }
  /* when some target changes bone, we might need to re-run the
   * whole IK solver, otherwise result might be unpredictable. */
  if (comp_node->type == NodeType::BONE) {
    ComponentNode *pose_comp = id_node->find_component(NodeType::EVAL_POSE);
    BLI_assert(pose_comp != NULL);
    if (pose_comp->custom_flags == COMPONENT_STATE_NONE) {
      queue->push_front(pose_comp->get_entry_operation());
      pose_comp->custom_flags = COMPONENT_STATE_SCHEDULED;
    }
  }
}

/* Schedule children of the given operation node for traversal.
 *
 * One of the children will by-pass the queue and will be returned as a function
 * return value, so it can start being handled right away, without building too
 * much of a queue.
 */
BLI_INLINE OperationNode *flush_schedule_children(OperationNode *op_node, FlushQueue *queue)
{
  OperationNode *result = NULL;
  for (Relation *rel : op_node->outlinks) {
    /* Flush is forbidden, completely. */
    if (rel->flag & RELATION_FLAG_NO_FLUSH) {
      continue;
    }
    if (op_node->flag & DEPSOP_FLAG_USER_MODIFIED) {
      IDNode *id_node = op_node->owner->owner;
      id_node->is_user_modified = true;
    }
    /* Relation only allows flushes on user changes, but the node was not
     * affected by user. */
    if ((rel->flag & RELATION_FLAG_FLUSH_USER_EDIT_ONLY) &&
        (op_node->flag & DEPSOP_FLAG_USER_MODIFIED) == 0) {
      continue;
    }
    OperationNode *to_node = (OperationNode *)rel->to;
    /* Always flush flushable flags, so children always know what happened
     * to their parents. */
    to_node->flag |= (op_node->flag & DEPSOP_FLAG_FLUSH);
    /* Flush update over the relation, if it was not flushed yet. */
    if (to_node->scheduled) {
      continue;
    }
    if (result != NULL) {
      queue->push_front(to_node);
    }
    else {
      result = to_node;
    }
    to_node->scheduled = true;
  }
  return result;
}

void flush_engine_data_update(ID *id)
{
  DrawDataList *draw_data_list = DRW_drawdatalist_from_id(id);
  if (draw_data_list == NULL) {
    return;
  }
  LISTBASE_FOREACH (DrawData *, draw_data, draw_data_list) {
    draw_data->recalc |= id->recalc;
  }
}

/* NOTE: It will also accumulate flags from changed components. */
void flush_editors_id_update(Depsgraph *graph, const DEGEditorUpdateContext *update_ctx)
{
  for (IDNode *id_node : graph->id_nodes) {
    if (id_node->custom_flags != ID_STATE_MODIFIED) {
      continue;
    }
    DEG_graph_id_type_tag(reinterpret_cast<::Depsgraph *>(graph), GS(id_node->id_orig->name));
    /* TODO(sergey): Do we need to pass original or evaluated ID here? */
    ID *id_orig = id_node->id_orig;
    ID *id_cow = id_node->id_cow;
    /* Gather recalc flags from all changed components. */
    GHASH_FOREACH_BEGIN (ComponentNode *, comp_node, id_node->components) {
      if (comp_node->custom_flags != COMPONENT_STATE_DONE) {
        continue;
      }
      DepsNodeFactory *factory = type_get_factory(comp_node->type);
      BLI_assert(factory != NULL);
      id_cow->recalc |= factory->id_recalc_tag();
    }
    GHASH_FOREACH_END();
    DEG_DEBUG_PRINTF((::Depsgraph *)graph,
                     EVAL,
                     "Accumulated recalc bits for %s: %u\n",
                     id_orig->name,
                     (unsigned int)id_cow->recalc);

    /* Inform editors. Only if the datablock is being evaluated a second
     * time, to distinguish between user edits and initial evaluation when
     * the datablock becomes visible.
     *
     * TODO: image datablocks do not use COW, so might not be detected
     * correctly. */
    if (deg_copy_on_write_is_expanded(id_cow)) {
      if (graph->is_active && id_node->is_user_modified) {
        deg_editors_id_update(update_ctx, id_orig);
      }
      /* ID may need to get its auto-override operations refreshed. */
      if (ID_IS_STATIC_OVERRIDE_AUTO(id_orig)) {
        id_orig->tag |= LIB_TAG_OVERRIDESTATIC_AUTOREFRESH;
      }
      /* Inform draw engines that something was changed. */
      flush_engine_data_update(id_cow);
    }
  }
}

#ifdef INVALIDATE_ON_FLUSH
void invalidate_tagged_evaluated_transform(ID *id)
{
  const ID_Type id_type = GS(id->name);
  switch (id_type) {
    case ID_OB: {
      Object *object = (Object *)id;
      copy_vn_fl((float *)object->obmat, 16, NAN);
      break;
    }
    default:
      break;
  }
}

void invalidate_tagged_evaluated_geometry(ID *id)
{
  const ID_Type id_type = GS(id->name);
  switch (id_type) {
    case ID_OB: {
      Object *object = (Object *)id;
      BKE_object_free_derived_caches(object);
      break;
    }
    default:
      break;
  }
}
#endif

void invalidate_tagged_evaluated_data(Depsgraph *graph)
{
#ifdef INVALIDATE_ON_FLUSH
  for (IDNode *id_node : graph->id_nodes) {
    if (id_node->custom_flags != ID_STATE_MODIFIED) {
      continue;
    }
    ID *id_cow = id_node->id_cow;
    if (!deg_copy_on_write_is_expanded(id_cow)) {
      continue;
    }
    GHASH_FOREACH_BEGIN (ComponentNode *, comp_node, id_node->components) {
      if (comp_node->custom_flags != COMPONENT_STATE_DONE) {
        continue;
      }
      switch (comp_node->type) {
        case ID_RECALC_TRANSFORM:
          invalidate_tagged_evaluated_transform(id_cow);
          break;
        case ID_RECALC_GEOMETRY:
          invalidate_tagged_evaluated_geometry(id_cow);
          break;
        default:
          break;
      }
    }
    GHASH_FOREACH_END();
  }
#else
  (void)graph;
#endif
}

}  // namespace

/* Flush updates from tagged nodes outwards until all affected nodes
 * are tagged.
 */
void deg_graph_flush_updates(Main *bmain, Depsgraph *graph)
{
  /* Sanity checks. */
  BLI_assert(bmain != NULL);
  BLI_assert(graph != NULL);
  /* Nothing to update, early out. */
  if (BLI_gset_len(graph->entry_tags) == 0) {
    return;
  }
  /* Reset all flags, get ready for the flush. */
  flush_prepare(graph);
  /* Starting from the tagged "entry" nodes, flush outwards. */
  FlushQueue queue;
  flush_schedule_entrypoints(graph, &queue);
  /* Prepare update context for editors. */
  DEGEditorUpdateContext update_ctx;
  update_ctx.bmain = bmain;
  update_ctx.depsgraph = (::Depsgraph *)graph;
  update_ctx.scene = graph->scene;
  update_ctx.view_layer = graph->view_layer;
  /* Do actual flush. */
  while (!queue.empty()) {
    OperationNode *op_node = queue.front();
    queue.pop_front();
    while (op_node != NULL) {
      /* Tag operation as required for update. */
      op_node->flag |= DEPSOP_FLAG_NEEDS_UPDATE;
      /* Inform corresponding ID and component nodes about the change. */
      ComponentNode *comp_node = op_node->owner;
      IDNode *id_node = comp_node->owner;
      flush_handle_id_node(id_node);
      flush_handle_component_node(id_node, comp_node, &queue);
      /* Flush to nodes along links. */
      op_node = flush_schedule_children(op_node, &queue);
    }
  }
  /* Inform editors about all changes. */
  flush_editors_id_update(graph, &update_ctx);
  /* Reset evaluation result tagged which is tagged for update to some state
   * which is obvious to catch. */
  invalidate_tagged_evaluated_data(graph);
}

static void graph_clear_operation_func(void *__restrict data_v,
                                       const int i,
                                       const ParallelRangeTLS *__restrict /*tls*/)
{
  Depsgraph *graph = (Depsgraph *)data_v;
  OperationNode *node = graph->operations[i];
  /* Clear node's "pending update" settings. */
  node->flag &= ~(DEPSOP_FLAG_DIRECTLY_MODIFIED | DEPSOP_FLAG_NEEDS_UPDATE |
                  DEPSOP_FLAG_USER_MODIFIED);
}

/* Clear tags from all operation nodes. */
void deg_graph_clear_tags(Depsgraph *graph)
{
  /* Go over all operation nodes, clearing tags. */
  {
    const int num_operations = graph->operations.size();
    ParallelRangeSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = 1024;
    BLI_task_parallel_range(0, num_operations, graph, graph_clear_operation_func, &settings);
  }
  /* Clear any entry tags which haven't been flushed. */
  BLI_gset_clear(graph->entry_tags, NULL);
}

}  // namespace DEG
