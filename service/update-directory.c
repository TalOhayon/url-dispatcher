/**
 * Copyright © 2014 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include "url-db.h"

typedef struct {
	const gchar * filename;
	sqlite3 * db;
} urldata_t;

static void
each_url (JsonArray * array, guint index, JsonNode * value, gpointer user_data)
{
	urldata_t * urldata = (urldata_t *)user_data;

	if (!JSON_NODE_HOLDS_OBJECT(value)) {
		g_warning("File %s: Array entry %d not an object", urldata->filename, index);
		return;
	}

	JsonObject * obj = json_node_get_object(value);

	const gchar * protocol = NULL;
	const gchar * suffix = NULL;

	if (json_object_has_member(obj, "protocol")) {
		protocol = json_object_get_string_member(obj, "protocol");
	}

	if (json_object_has_member(obj, "domain-suffix")) {
		suffix = json_object_get_string_member(obj, "domain-suffix");
	}

	if (protocol == NULL && suffix == NULL) {
		g_warning("File %s: Array entry %d doesn't contain one of 'protocol' or 'domain-suffix'", urldata->filename, index);
		return;
	}

	url_db_insert_url(urldata->db, urldata->filename, protocol, suffix);
}

static void
insert_urls_from_file (const gchar * filename, sqlite3 * db)
{
	GError * error = NULL;
	JsonParser * parser = json_parser_new();
	json_parser_load_from_file(parser, filename, &error);

	if (error != NULL) {
		g_warning("Unable to parse JSON in '%s': %s", filename, error->message);
		g_object_unref(parser);
		return;
	}

	JsonNode * rootnode = json_parser_get_root(parser);
	if (!JSON_NODE_HOLDS_ARRAY(rootnode)) {
		g_warning("File '%s' does not have an array as its root node", filename);
		g_object_unref(parser);
		return;
	}

	JsonArray * rootarray = json_node_get_array(rootnode);

	urldata_t urldata = {
		.filename = filename,
		.db = db
	};

	json_array_foreach_element(rootarray, each_url, &urldata);

	g_object_unref(parser);
}

static gboolean
check_file_uptodate (const gchar * filename, sqlite3 * db)
{
	g_debug("Processing file: %s", filename);

	GTimeVal dbtime = {0};
	GTimeVal filetime = {0};

	GFile * file = g_file_new_for_path(filename);
	g_return_if_fail(file != NULL);

	GFileInfo * info = g_file_query_info(file, G_FILE_ATTRIBUTE_TIME_MODIFIED, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	g_file_info_get_modification_time(info, &filetime);

	g_object_unref(info);
	g_object_unref(file);

	if (url_db_get_file_motification_time(db, filename, &dbtime)) {
		if (filetime.tv_sec <= dbtime.tv_sec) {
			g_debug("\tup-to-date: %s", filename);
			return FALSE;
		}
	}

	url_db_set_file_motification_time(db, filename, &filetime);

	return TRUE;
}

int
main (int argc, char * argv[])
{
	if (argc != 2) {
		g_printerr("Usage: %s <directory>\n", argv[0]);
		return 1;
	}

	if (!g_file_test(argv[1], G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_print("Directory '%s' is up-to-date because it doesn't exist", argv[1]);
		return 0;
	}

	sqlite3 * db = url_db_create_database();
	g_return_val_if_fail(db != NULL, -1);

	GDir * dir = g_dir_open(argv[1], 0, NULL);
	g_return_val_if_fail(dir != NULL, -1);

	const gchar * name = NULL;
	while ((name = g_dir_read_name(dir)) != NULL) {
		if (g_str_has_suffix(name, ".url-dispatcher")) {
			gchar * fullname = g_build_filename(argv[1], name, NULL);

			if (check_file_uptodate(fullname, db)) {
				insert_urls_from_file(fullname, db);
			}

			g_free(fullname);
		}
	}

	g_dir_close(dir);
	sqlite3_close(db);

	g_debug("Directory '%s' is up-to-date", argv[1]);
	return 0;
}
