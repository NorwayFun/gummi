/**
 * @file   gui-snippets.c
 * @brief  Handle snippets and provide edit/new/delete function
 *
 * Copyright (C) 2010 Gummi-Dev Team <alexvandermey@gmail.com>
 * All Rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "gui/gui-snippets.h"

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gtksourceview/gtksourcebuffer.h>
#include <gtksourceview/gtksourceiter.h>
#include <gtksourceview/gtksourcelanguagemanager.h>
#include <gtksourceview/gtksourcestyleschememanager.h>
#include <gtksourceview/gtksourceview.h>

#include "environment.h"
#include "gui/gui-main.h"
#include "porting.h"
#include "snippets.h"

extern Gummi* gummi;
extern GummiGui* gui;

GuSnippetsGui* snippetsgui_init(GtkWindow* mainwindow) {
    GuSnippetsGui* s = g_new0(GuSnippetsGui, 1);
    GtkSourceLanguageManager* manager = NULL;
    GtkSourceLanguage* lang = NULL;
    gchar* lang_dir = NULL;
    gchar** langs = NULL;
    gchar** new_langs = NULL;
    gint len = 0, i = 0;

    GtkBuilder* builder = gtk_builder_new();
    gchar* ui = g_build_filename(DATADIR, "ui", "snippets.glade", NULL);
    gtk_builder_add_from_file(builder, ui, NULL);
    gtk_builder_set_translation_domain(builder, PACKAGE);
    g_free(ui);

    s->snippetswindow =
        GTK_WINDOW(gtk_builder_get_object(builder, "snippetswindow"));
    s->snippets_tree_view =
        GTK_TREE_VIEW(gtk_builder_get_object(builder, "snippets_tree_view"));
    s->snippet_scroll =
        GTK_SCROLLED_WINDOW(gtk_builder_get_object(builder, "snippet_scroll"));
    s->tab_trigger_entry = 
        GTK_ENTRY(gtk_builder_get_object(builder, "tab_trigger_entry"));
    s->accelerator_entry = 
        GTK_ENTRY(gtk_builder_get_object(builder, "accelerator_entry"));
    s->list_snippets = 
        GTK_LIST_STORE(gtk_builder_get_object(builder, "list_snippets"));
    s->snippet_renderer = GTK_CELL_RENDERER_TEXT
        (gtk_builder_get_object(builder, "snippet_renderer"));

    /* Initialize GtkSourceView */
    manager = gtk_source_language_manager_new();
    lang_dir = g_build_filename(DATADIR, "snippets", NULL);
    langs = g_strdupv((gchar**)gtk_source_language_manager_get_search_path(
                manager));
    len = g_strv_length(langs);
    new_langs = g_new0(gchar*, len + 2);
    for (i = 0; i < len; ++i)
        new_langs[i] = langs[i];
    new_langs[len] = lang_dir;
    gtk_source_language_manager_set_search_path(manager, new_langs);
    lang = gtk_source_language_manager_get_language(manager, "snippets");
    g_strfreev(langs);
    g_free(new_langs);
    g_free(lang_dir);

    s->buffer = gtk_source_buffer_new_with_language(lang);
    s->view = GTK_SOURCE_VIEW(gtk_source_view_new_with_buffer(s->buffer));
    gtk_container_add(GTK_CONTAINER(s->snippet_scroll), GTK_WIDGET(s->view));

    snippetsgui_load_snippets(s);

    g_signal_connect(s->view, "key-release-event",
            G_CALLBACK(on_snippet_source_buffer_key_release), NULL);

    gtk_window_set_transient_for(s->snippetswindow, mainwindow);
    gtk_builder_connect_signals(builder, NULL);

    return s;
}

void snippetsgui_main(GuSnippetsGui* s) {
    gtk_widget_show_all(GTK_WIDGET(s->snippetswindow));
}

void snippetsgui_load_snippets(GuSnippetsGui* s) {
    slist* current = gummi->snippets->head;
    GtkTreeIter iter;
    gchar** configs = NULL;

    gtk_list_store_clear(s->list_snippets);
    while (current) {
        if (current->first[0] != '#') {
            gtk_list_store_append(s->list_snippets, &iter);
            configs = g_strsplit(current->first, ",", 0);
            gtk_list_store_set(s->list_snippets, &iter, 0, configs[2],
                    1, current->first, -1);
            g_strfreev(configs);
        }
        current = current->next;
    }
}

void snippetsgui_move_cursor_to_row(GuSnippetsGui* s, gint row) {
    GtkTreePath* path = NULL;
    GtkTreeViewColumn* col = NULL;
    gchar* path_str = g_strdup_printf("%d", (row >= 0)? row: 0);

    path = gtk_tree_path_new_from_string(path_str);
    col = gtk_tree_view_get_column(s->snippets_tree_view, 0);
    gtk_tree_view_set_cursor(s->snippets_tree_view, path, col, FALSE);
    gtk_tree_path_free(path);
    g_free(path_str);
}

void snippetsgui_update_snippet(GuSnippets* sc) {
    GuSnippetsGui* s = gui->snippetsgui;
    slist* target = s->current;
    gchar** configs = g_strsplit(target->first, ",", 0);
    const gchar* new_key = gtk_entry_get_text(s->tab_trigger_entry);
    const gchar* new_accel = gtk_entry_get_text(s->accelerator_entry);
    GtkTreeIter iter;
    GtkTreeModel* model =
        GTK_TREE_MODEL(gtk_tree_view_get_model(s->snippets_tree_view));
    GtkTreeSelection* selection =
        gtk_tree_view_get_selection(s->snippets_tree_view);

    gtk_tree_selection_get_selected(selection, &model, &iter);

    g_free(target->first);
    target->first = g_strdup_printf("%s,%s,%s", new_key, new_accel, configs[2]);
    gtk_list_store_set(s->list_snippets, &iter, 1, target->first, -1);

    /* Update */
    if (strlen(new_accel) && strcmp(new_accel, configs[1]) != 0) {
        guint keyval = 0;
        GdkModifierType mod;
        Tuple2* data = g_new0(Tuple2, 1);
        Tuple2* closure_data = NULL;
        Tuple2* new_closure_data = g_new0(Tuple2, 1);

        data->first = (gpointer)sc;
        data->second = (gpointer)g_strdup(new_key);

        GList* current = sc->closure_data;
        GClosure* new_closure = g_cclosure_new(G_CALLBACK(snippets_accel_cb),
                data, NULL);
        new_closure_data->first = data->second;
        new_closure_data->second = new_closure;

        while (current) {
            closure_data = TUPLE2(current->data);
            if (strcmp(closure_data->first, configs[0]) == 0)
                break;
            current = g_list_next(current);
        }

        /* Remove old accelerator if exists */
        if (current) {
            snippets_accel_disconnect(sc->accel_group, closure_data->second);
            sc->closure_data = g_list_remove(sc->closure_data, closure_data);
            g_free(closure_data);
        }

        /* Connect new accelerator */
        gtk_accelerator_parse(new_accel, &keyval, &mod);
        sc->closure_data = g_list_append(sc->closure_data, new_closure_data);
        snippets_accel_connect(sc->accel_group, keyval, mod, new_closure);
    }
    g_strfreev(configs);
}

void on_snippetsgui_close_clicked(GtkWidget* widget, void* user) {
    gtk_widget_hide_all(GTK_WIDGET(gui->snippetsgui->snippetswindow));
    snippets_save(gummi->snippets);
}

void on_snippetsgui_reset_clicked(GtkWidget* widget, void* user) {
    snippets_set_default(gummi->snippets);
    snippetsgui_load_snippets(gui->snippetsgui);
    snippetsgui_move_cursor_to_row(gui->snippetsgui, 0);
}

void on_button_new_snippet_clicked(GtkWidget* widget, void* user) {
    GuSnippetsGui* s = gui->snippetsgui;
    GtkTreeIter iter;
    GtkTreeModel *model = NULL;
    GtkTreePath *path = NULL;
    GtkTreeViewColumn *col = NULL;

    gtk_list_store_append(s->list_snippets, &iter);
    g_object_set(s->snippet_renderer, "editable", TRUE, NULL);
    
    col = gtk_tree_view_get_column(s->snippets_tree_view, 0);
    model = gtk_tree_view_get_model(s->snippets_tree_view);
    path = gtk_tree_model_get_path(model, &iter);

    SIG_SAFE(gtk_tree_view_set_cursor(s->snippets_tree_view, path, col, TRUE));

    gtk_tree_path_free(path);
}

void on_button_remove_snippet_clicked(GtkWidget* widget, void* user) {
    GuSnippetsGui* s = gui->snippetsgui;
    GtkTreeModel* model= gtk_tree_view_get_model(s->snippets_tree_view);
    GtkTreeSelection* selection = gtk_tree_view_get_selection(
            s->snippets_tree_view);
    GtkTreeIter iter;
    GtkTreePath* path = NULL;
    gchar* path_str = NULL;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar* config = NULL;
        slist* target = NULL;
        gtk_tree_model_get(model, &iter, 1, &config, -1);
        path = gtk_tree_model_get_path(model, &iter);
        path_str = gtk_tree_path_to_string(path);
        
        /* Because this function is also called by on_snippet_renderer_edited
         * where the snippet to be remove isn't inserted into slist, we only
         * remove if the snippets is already in the slist */
        if (config) {
            target = slist_find_index_of(gummi->snippets->head, config, FALSE,
                    FALSE);
            slist_remove(gummi->snippets->head, target);
        }

        /* Activate previous item if the removed snippet is not the last one */
        if (gtk_list_store_remove(s->list_snippets, &iter)) {
            snippetsgui_move_cursor_to_row(gui->snippetsgui, atoi(path_str));
        } else if (gtk_tree_model_get_iter_first(model, &iter)) {
            snippetsgui_move_cursor_to_row(gui->snippetsgui, atoi(path_str) -1);
        } else {
            SIG_SAFE(
                gtk_text_buffer_set_text(GTK_TEXT_BUFFER(s->buffer), "", -1);
                gtk_entry_set_text(s->tab_trigger_entry, "");
                gtk_entry_set_text(s->accelerator_entry, "");
            );
        }
        g_free(path_str);
    }
}

void on_tab_trigger_entry_changed(GtkEntry* entry, void* user) {
    if (!gui->snippetsgui->loading)
        snippetsgui_update_snippet(gummi->snippets);
}

void on_accelerator_entry_focus_in_event(GtkWidget* widget, void* user) {
    GuSnippetsGui* s = gui->snippetsgui;
    if (!strlen(gtk_entry_get_text(s->accelerator_entry)))
        gtk_entry_set_text(s->accelerator_entry, _("Type a new shortcut"));
    else
        gtk_entry_set_text(s->accelerator_entry,
                _("Type a new shortcut, or press Backspace to clear"));
}

void on_accelerator_entry_focus_out_event(GtkWidget* widget, void* user) {
    GuSnippetsGui* s = gui->snippetsgui;
    gchar** configs = NULL;
    configs = g_strsplit(s->current->first, ",", 0);
    gtk_entry_set_text(s->accelerator_entry, configs[1]);
    g_strfreev(configs);
}

gboolean on_accelerator_entry_key_press_event(GtkWidget* widget,
        GdkEventKey* event, void* user) {
    GuSnippetsGui* s = gui->snippetsgui;
    gchar* new_accel = NULL;

    if (event->keyval == GDK_KEY_Escape) {
        /* Reset */
        gtk_entry_set_text(s->accelerator_entry, "");
        snippetsgui_update_snippet(gummi->snippets);
        gtk_widget_grab_focus(GTK_WIDGET(s->snippets_tree_view));
    } else if (event->keyval == GDK_KEY_BackSpace
               || event->keyval == GDK_KEY_Delete) {
        /* Remove accelerator */
        gtk_entry_set_text(s->accelerator_entry, "");
        snippetsgui_update_snippet(gummi->snippets);
        gtk_widget_grab_focus(GTK_WIDGET(s->snippets_tree_view));
    } else if (gtk_accelerator_valid(event->keyval, event->state)) {
        /* New accelerator */
        new_accel = gtk_accelerator_name(event->keyval,
                gtk_accelerator_get_default_mod_mask() & event->state);
        gtk_entry_set_text(s->accelerator_entry, new_accel);
        snippetsgui_update_snippet(gummi->snippets);
        g_free(new_accel);
        gtk_widget_grab_focus(GTK_WIDGET(s->snippets_tree_view));
    }
    return TRUE;
}

void on_snippets_tree_view_cursor_changed(GtkTreeView* view, void* user) {
    GuSnippetsGui* s = gui->snippetsgui;
    GtkTreeIter iter;
    gchar* config = NULL;
    gchar** configs = NULL;
    gchar* snippet = NULL;
    GtkTreeModel* model = NULL;
    GtkTreeSelection* selection = NULL;

    return_if_sig_safe();
    model = GTK_TREE_MODEL(gtk_tree_view_get_model(view));
    selection = gtk_tree_view_get_selection(view);

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, 1, &config, -1);

        /* Record current activated snippet */
        s->current = slist_find_index_of(gummi->snippets->head, config, FALSE,
                FALSE);

        configs = g_strsplit(config, ",", 0);
        snippet = snippets_get_value(gummi->snippets, configs[0]);

        SIG_SAFE(
            gtk_text_buffer_set_text(GTK_TEXT_BUFFER(s->buffer), snippet, -1);
            gtk_entry_set_text(s->tab_trigger_entry, configs[0]);
            gtk_entry_set_text(s->accelerator_entry, configs[1]);
        );

        g_strfreev(configs);
        g_free(config);
    }
}

void on_snippet_renderer_edited(GtkCellRendererText* renderer, gchar *path,
        gchar* name, void* user) {
    GuSnippetsGui* s = gui->snippetsgui;
    GtkTreeIter iter;
    GtkTreeModel* model = NULL;
    GtkTreeSelection* selection = NULL;
    gchar* config = NULL;
    
    g_object_set(renderer, "editable", FALSE, NULL);
    model = gtk_tree_view_get_model(s->snippets_tree_view);
    selection = gtk_tree_view_get_selection(s->snippets_tree_view);
    config = g_strdup_printf(",,%s", name);

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        if (strlen(name)) {
            slist* node = g_new0(slist, 1);
            node->first = config;
            node->second = g_strdup("");
            gummi->snippets->head = slist_append(gummi->snippets->head, node);
            s->current = node;
            gtk_list_store_set(s->list_snippets, &iter, 0, name, 1, config, -1);
        } else {
            gtk_list_store_set(s->list_snippets, &iter, 0, name, 1, "", -1);
            on_button_remove_snippet_clicked(NULL, NULL);
        }
        g_free(config);
    }
}

gboolean on_snippet_source_buffer_key_release(GtkWidget* widget, void* user) {
    GuSnippetsGui* s = gui->snippetsgui;
    GtkTextIter start, end;

    return_if_sig_safe(FALSE);
    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(gui->snippetsgui->buffer),
            &start, &end);
    gchar* text = gtk_text_iter_get_text(&start, &end);
    g_free(s->current->second);
    s->current->second = g_strdup(text);
    g_free(text);
    return FALSE;
}