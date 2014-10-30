/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2011 Colin Walters <walters@verbum.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Colin Walters <walters@verbum.org>
 */

#include "config.h"

#include "ot-builtins.h"
#include "ostree.h"
#include "otutil.h"

static gboolean opt_stats;
static gboolean opt_fs_diff;

static GOptionEntry options[] = {
  { "stats", 0, 0, G_OPTION_ARG_NONE, &opt_stats, "Print various statistics", NULL },
  { "fs-diff", 0, 0, G_OPTION_ARG_NONE, &opt_fs_diff, "Print filesystem diff", NULL },
  { NULL }
};

static gboolean
parse_file_or_commit (OstreeRepo  *repo,
                      const char  *arg,
                      GFile      **out_file,
                      GCancellable *cancellable,
                      GError     **error)
{
  gboolean ret = FALSE;
  gs_unref_object GFile *ret_file = NULL;

  if (g_str_has_prefix (arg, "/")
      || g_str_has_prefix (arg, "./")
      )
    {
      ret_file = g_file_new_for_path (arg);
    }
  else
    {
      if (!ostree_repo_read_commit (repo, arg, &ret_file, NULL, cancellable, error))
        goto out;
    }

  ret = TRUE;
  ot_transfer_out_value (out_file, &ret_file);
 out:
  return ret;
}

static GHashTable *
reachable_set_intersect (GHashTable *a, GHashTable *b)
{
  GHashTable *ret = ostree_repo_traverse_new_reachable ();
  GHashTableIter hashiter;
  gpointer key, value;

  g_hash_table_iter_init (&hashiter, a);
  while (g_hash_table_iter_next (&hashiter, &key, &value))
    {
      GVariant *v = key;
      if (g_hash_table_contains (b, v))
        g_hash_table_insert (ret, g_variant_ref (v), v);
    }

  return ret;
}

static gboolean
object_set_total_size (OstreeRepo    *repo,
                       GHashTable    *reachable,
                       guint64       *out_total,
                       GCancellable  *cancellable,
                       GError       **error)
{
  gboolean ret = FALSE;
  GHashTableIter hashiter;
  gpointer key, value;

  *out_total = 0;

  g_hash_table_iter_init (&hashiter, reachable);
  while (g_hash_table_iter_next (&hashiter, &key, &value))
    {
      GVariant *v = key;
      const char *csum;
      OstreeObjectType objtype;
      guint64 size;

      ostree_object_name_deserialize (v, &csum, &objtype);
      if (!ostree_repo_query_object_storage_size (repo, objtype, csum, &size,
                                                  cancellable, error))
        goto out;
      *out_total += size;
    }

  ret = TRUE;
 out:
  return ret;
}

gboolean
ostree_builtin_diff (int argc, char **argv, OstreeRepo *repo, GCancellable *cancellable, GError **error)
{
  gboolean ret = FALSE;
  GOptionContext *context;
  const char *src;
  const char *target;
  gs_free char *src_prev = NULL;
  gs_unref_object GFile *srcf = NULL;
  gs_unref_object GFile *targetf = NULL;
  gs_unref_ptrarray GPtrArray *modified = NULL;
  gs_unref_ptrarray GPtrArray *removed = NULL;
  gs_unref_ptrarray GPtrArray *added = NULL;

  context = g_option_context_new ("REV TARGETDIR - Compare directory TARGETDIR against revision REV");
  g_option_context_add_main_entries (context, options, NULL);

  if (!g_option_context_parse (context, &argc, &argv, error))
    goto out;

  if (argc < 2)
    {
      gchar *help = g_option_context_get_help (context, TRUE, NULL);
      g_printerr ("%s\n", help);
      g_free (help);
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "REV must be specified");
      goto out;
    }

  if (argc == 2)
    {
      src_prev = g_strconcat (argv[1], "^", NULL);
      src = src_prev;
      target = argv[1];
    }
  else
    {
      src = argv[1];
      target = argv[2];
    }

  if (!opt_stats && !opt_fs_diff)
    opt_fs_diff = TRUE;

  if (opt_fs_diff)
    {
      if (!parse_file_or_commit (repo, src, &srcf, cancellable, error))
        goto out;
      if (!parse_file_or_commit (repo, target, &targetf, cancellable, error))
        goto out;

      modified = g_ptr_array_new_with_free_func ((GDestroyNotify)ostree_diff_item_unref);
      removed = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);
      added = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);
      
      if (!ostree_diff_dirs (OSTREE_DIFF_FLAGS_NONE, srcf, targetf, modified, removed, added, cancellable, error))
        goto out;

      ostree_diff_print (srcf, targetf, modified, removed, added);
    }

  if (opt_stats)
    {
      gs_unref_hashtable GHashTable *reachable_a = NULL;
      gs_unref_hashtable GHashTable *reachable_b = NULL;
      gs_unref_hashtable GHashTable *reachable_intersection = NULL;
      gs_free char *rev_a = NULL;
      gs_free char *rev_b = NULL;
      gs_free char *size = NULL;
      guint a_size;
      guint b_size;
      guint64 total_common;

      if (!ostree_repo_resolve_rev (repo, src, FALSE, &rev_a, error))
        goto out;
      if (!ostree_repo_resolve_rev (repo, target, FALSE, &rev_b, error))
        goto out;

      if (!ostree_repo_traverse_commit (repo, rev_a, -1, &reachable_a, cancellable, error))
        goto out;
      if (!ostree_repo_traverse_commit (repo, rev_b, -1, &reachable_b, cancellable, error))
        goto out;

      a_size = g_hash_table_size (reachable_a);
      b_size = g_hash_table_size (reachable_b);
      g_print ("[A] Object Count: %u\n", a_size);
      g_print ("[B] Object Count: %u\n", b_size);

      reachable_intersection = reachable_set_intersect (reachable_a, reachable_b);

      g_print ("Common Object Count: %u\n", g_hash_table_size (reachable_intersection));

      if (!object_set_total_size (repo, reachable_intersection, &total_common,
                                  cancellable, error))
        goto out;
      size = g_format_size_full (total_common, 0);
      g_print ("Common Object Size: %s\n", size);
    }

  ret = TRUE;
 out:
  if (context)
    g_option_context_free (context);
  return ret;
}
