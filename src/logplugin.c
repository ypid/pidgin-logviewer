/* Improved Log Viewer for Pidgin.
 * Tirtha Chatterjee
 * This code is licensed under GPL v2
 */ 


#define PURPLE_PLUGINS


#ifndef WIN32
#include "config.h"
#else
#include <config-win32.h>
#include <win32dep.h>
#include <windows.h>
#endif

#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "plugin.h"
#include "pidgin.h"

#include "account.h"
#include "debug.h"
#include "log.h"
#include "notify.h"
#include "request.h"
#include "util.h"
#include "version.h"

#include "pidginstock.h"
#include "gtkblist.h"
#include "gtkimhtml.h"
#include "gtklog.h"
#include "gtkutils.h"
#include "gtkplugin.h"


typedef struct _PidginLogViewerNew PidginLogViewerNew;

struct _PidginLogViewerNew {
	GList *logs;                 /**< The list of logs viewed in this viewer   */

	GtkWidget        *window;    /**< The viewer's window                      */
	GtkListStore     *buddy_liststore; /**< The treestore containing names of buddies */
	GtkWidget        *buddy_treeview;  /**< The treeview representing said buddy_liststore */
	GtkWidget        *search_treeview;  /**< The treeview representing said buddy_liststore */
	GtkWidget        *logsonday_combo;  /**< The treeview representing said buddy_liststore */
	GtkWidget        *calendar;       /**<  The  GtkCalendar where chat log dates are highlighted */
	GtkWidget        *imhtml_conv;    /**< The imhtml to display said logs of conversations */
	GtkWidget        *imhtml_search;    /**< The imhtml to display said logs of search */
	GtkWidget        *search_spinner;
        GtkWidget        *search_button;
        GtkWidget        *delete_button;
        GtkWidget        *find_filter_entry;
	GtkWidget        *search_entry;     /**< The search entry, in which search terms
	                              *   are entered                              */
	PurpleLogReadFlags conv_flags;   /**< The most recently used log flags         */
	PurpleLogReadFlags search_flags;   /**< The most recently used log flags         */
	char             *search;	/**< The string currently being searched for  */
	char             *find;		/**< The string to be searched within the log */
	gboolean         search_cancelled;
	PurpleAccount    *account;	/**< The account currently selected  */
	PurpleContact    *contact;
        PurpleLog        *log;
};

void populate_log_tree_buddies(PidginLogViewerNew *dialog);
static void pidgin_log_win_show(PurplePluginAction *action);
void log_find_log_cb(GtkWidget *w, PidginLogViewerNew *lvn);
void month_changed_cb(GtkWidget *calendar, PidginLogViewerNew *dialog);
gboolean delete_log_win_cb(GtkWidget *w, GdkEventAny *e, PidginLogViewerNew *data);
void log_mark_calendar_by_month(PidginLogViewerNew *dialog ,uint month, uint year);
void log_day_selected_cb(GtkWidget *calendar, PidginLogViewerNew *dialog);
void buddy_filter_change_cb(GtkWidget *entry, PidginLogViewerNew *lvn);
static gint purple_log_reverse_compare(gconstpointer l1, gconstpointer l2);
static gboolean buddy_visible_func (GtkTreeModel *model, GtkTreeIter  *iter, gchar *ftext);
void logsonday_combo_changed_cb(GtkWidget *combo, PidginLogViewerNew *dialog);
void search_filter_changed_cb(GtkWidget *entry, PidginLogViewerNew *lvn);
void find_filter_changed_cb(GtkWidget *entry, PidginLogViewerNew *lvn);
void delete_log_cb(GtkWidget *button, PidginLogViewerNew *lvn);


void
log_mark_calendar_by_month(PidginLogViewerNew *dialog ,uint month, uint year)
{
	PurpleLog *log;
	GList *logs = NULL, *l;
	PurpleContact *contact = dialog->contact;
	PurpleBlistNode *child;
	int day=0;
	gtk_calendar_select_day(GTK_CALENDAR(dialog->calendar),1);
	gtk_calendar_clear_marks(GTK_CALENDAR(dialog->calendar));
	
	gtk_calendar_select_month(GTK_CALENDAR(dialog->calendar), month, year);
	year -= 1900;
	
	for (child = purple_blist_node_get_first_child((PurpleBlistNode*)contact) ;
	     child != NULL ;
	     child = purple_blist_node_get_sibling_next(child)) {
		const char *buddy_name;
		PurpleAccount *account;

		if (!PURPLE_BLIST_NODE_IS_BUDDY(child))
			continue;

		buddy_name = purple_buddy_get_name((PurpleBuddy *)child);
		account = purple_buddy_get_account((PurpleBuddy *)child);
		logs = g_list_concat(purple_log_get_logs(PURPLE_LOG_IM, buddy_name, account), logs);
		
	}
	
	l = logs;
	while(logs != NULL)
	{
		log = logs->data;
                if( log == NULL ) continue;
		if(((log->tm ? log->tm : localtime(&log->time))->tm_year == year) && ((log->tm ? log->tm : localtime(&log->time))->tm_mon == month))
		{
			gtk_calendar_mark_day(GTK_CALENDAR(dialog->calendar),(log->tm ? log->tm : localtime(&log->time))->tm_mday);
			if((log->tm ? log->tm : localtime(&log->time))->tm_mday > day) day = (log->tm ? log->tm : localtime(&log->time))->tm_mday;
		}
		
		logs = logs->next;
	}
        if ( l != NULL ) {
                g_list_foreach(logs, (GFunc)purple_log_free, NULL);
                g_list_free(logs);
        }
	gtk_calendar_select_day(GTK_CALENDAR(dialog->calendar), day);
}
void
logsonday_combo_changed_cb(GtkWidget *combo, PidginLogViewerNew *dialog)
{
        GtkTreeModel *model;
        GtkTreeIter iter;
        PurpleLog *log = NULL;
        PurpleLogReadFlags flags;
        char *read = NULL;
        const gchar *filter = gtk_entry_get_text(GTK_ENTRY(dialog->find_filter_entry));
        
        dialog->log = NULL;
        gtk_widget_set_sensitive(dialog->delete_button,FALSE);
        gtk_imhtml_clear(GTK_IMHTML(dialog->imhtml_conv));
        if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(dialog->logsonday_combo), &iter))
        {
                model = gtk_combo_box_get_model( GTK_COMBO_BOX( dialog->logsonday_combo ) );
                gtk_tree_model_get( model, &iter, 1, &log, -1 );
        }
        
        if(log == NULL) {
                return;
        }
        
        read = purple_log_read(log, &flags);
        
        if(read == NULL) {
                return;
        }
        
        dialog->conv_flags = flags;

	gtk_imhtml_set_protocol_name(GTK_IMHTML(dialog->imhtml_conv),
		purple_account_get_protocol_name(log->account));

	purple_signal_emit(pidgin_log_get_handle(), "log-displaying", dialog, log);

	gtk_imhtml_append_text(GTK_IMHTML(dialog->imhtml_conv), read,
                GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_TITLE | GTK_IMHTML_NO_SCROLL |
		((flags & PURPLE_LOG_READ_NO_NEWLINE) ? GTK_IMHTML_NO_NEWLINE : 0));
        g_free(read);
        
        dialog->log = log;
        gtk_widget_set_sensitive(dialog->delete_button,TRUE);
        
        gtk_imhtml_search_clear(GTK_IMHTML(dialog->imhtml_conv));
        if(*filter == '\0')
	{
		return;
	}
        gtk_imhtml_search_find(GTK_IMHTML(dialog->imhtml_conv), filter);
}

void
log_day_selected_cb(GtkWidget *calendar, PidginLogViewerNew *dialog)
{
	uint year,month,day;
	PurpleLog *log;
	GList *logs = NULL, *l;
	PurpleContact *contact = dialog->contact;
	PurpleBlistNode *child;
	GtkTreeIter iter;
        GtkTreeModel *model;
        int logsonday = 0;

        model = gtk_combo_box_get_model( GTK_COMBO_BOX( dialog->logsonday_combo ) );
	
        gtk_list_store_clear(GTK_LIST_STORE(model));
	if(contact == NULL) {
                return;
        }
	
	gtk_calendar_get_date(GTK_CALENDAR(calendar), &year, &month, &day);
	year -= 1900;
	
	for (child = purple_blist_node_get_first_child((PurpleBlistNode*)contact) ;
	     child != NULL ;
	     child = purple_blist_node_get_sibling_next(child)) {
		const char *buddy_name;
		PurpleAccount *account;

		if (!PURPLE_BLIST_NODE_IS_BUDDY(child))
			continue;

		buddy_name = purple_buddy_get_name((PurpleBuddy *)child);
		account = purple_buddy_get_account((PurpleBuddy *)child);
		logs = g_list_concat(purple_log_get_logs(PURPLE_LOG_IM, buddy_name, account), logs);
		
	}
	logs = g_list_sort(logs, purple_log_reverse_compare);
        l = logs;

	gtk_imhtml_clear(GTK_IMHTML(dialog->imhtml_conv));
	while(logs != NULL)
	{
		log = logs->data;
                
                if(log == NULL) continue;
                
                if(((log->tm ? log->tm : localtime(&log->time))->tm_year == year) && 
                ((log->tm ? log->tm : localtime(&log->time))->tm_mon == month) && 
                ((log->tm ? log->tm : localtime(&log->time))->tm_mday == day))
		{
			gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                        gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0,
                                purple_utf8_strftime("%I:%M %p",
                                        log->tm ? log->tm : localtime(&log->time)),
                                1, log, -1);
                        ++logsonday;
		}
		
		logs = logs->next;
	}
        
        if(logsonday) gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->logsonday_combo), 0);
        if(logsonday > 1) {
                gtk_widget_set_sensitive(dialog->logsonday_combo,TRUE);
        } else {
                gtk_widget_set_sensitive(dialog->logsonday_combo,FALSE);
        }
}
void
month_changed_cb(GtkWidget *calendar, PidginLogViewerNew *dialog)
{
	uint year, month, day;
	gtk_calendar_get_date(GTK_CALENDAR(calendar), &year, &month, &day);
	gtk_calendar_select_day(GTK_CALENDAR(calendar),1);
	log_mark_calendar_by_month(dialog,month,year);
}
static void
log_select_buddy_cb(GtkTreeSelection *sel, PidginLogViewerNew *dialog) {
	GtkTreeIter iter;
	GValue val;
	GtkTreeModel *model = GTK_TREE_MODEL(dialog->buddy_liststore);
	GList *logs = NULL, *l;
	PurpleLog *log;
	PurpleContact *contact = NULL;
	int last_day=0, last_month=0, last_year=0;
	PurpleBlistNode *child;
	
	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;

	val.g_type = 0;
	gtk_tree_model_get_value (model, &iter, 1, &val);
	contact = g_value_get_pointer(&val);
	g_value_unset(&val);

	if (contact == NULL)
	{
		dialog->contact = NULL;
		return;
	}
	
	dialog->contact = contact;
		
	for (child = purple_blist_node_get_first_child((PurpleBlistNode*)contact) ;
	     child != NULL ;
	     child = purple_blist_node_get_sibling_next(child)) {
		const char *buddy_name;
		PurpleAccount *account;

		if (!PURPLE_BLIST_NODE_IS_BUDDY(child))
			continue;

		buddy_name = purple_buddy_get_name((PurpleBuddy *)child);
		account = purple_buddy_get_account((PurpleBuddy *)child);
		logs = g_list_concat(purple_log_get_logs(PURPLE_LOG_IM, buddy_name, account), logs);
		
	}
	
	l = logs;
	while(logs != NULL)
	{
		log = logs->data;
                if(log == NULL) continue;
		if((log->tm ? log->tm : localtime(&log->time))->tm_year > last_year)
		{
			last_year = (log->tm ? log->tm : localtime(&log->time))->tm_year;
			last_month = (log->tm ? log->tm : localtime(&log->time))->tm_mon;
			last_day = (log->tm ? log->tm : localtime(&log->time))->tm_mday;
		}
		else if(((log->tm ? log->tm : localtime(&log->time))->tm_year == last_year) &&
                ((log->tm ? log->tm : localtime(&log->time))->tm_mon > last_month))
		{
			last_year = (log->tm ? log->tm : localtime(&log->time))->tm_year;
			last_month = (log->tm ? log->tm : localtime(&log->time))->tm_mon;
			last_day = (log->tm ? log->tm : localtime(&log->time))->tm_mday;
		}
		else if(((log->tm ? log->tm : localtime(&log->time))->tm_year == last_year) &&
                ((log->tm ? log->tm : localtime(&log->time))->tm_mon == last_month) &&
                ((log->tm ? log->tm : localtime(&log->time))->tm_mday > last_day))
		{
			last_year = (log->tm ? log->tm : localtime(&log->time))->tm_year;
			last_month = (log->tm ? log->tm : localtime(&log->time))->tm_mon;
			last_day = (log->tm ? log->tm : localtime(&log->time))->tm_mday;
		}	
		logs = logs->next;
	}
    if(l != NULL)
    {
        g_list_foreach(l, (GFunc)purple_log_free, NULL);
        g_list_free(l);
    }
    
    log_mark_calendar_by_month(dialog, last_month, last_year+1900);
}


void
populate_log_tree_buddies(PidginLogViewerNew *lvn)
{
	GList *logs;
	GSList *buddies;
	PurpleBuddy *bdy;
	GtkTreeIter bdy_level;
	
	buddies = purple_blist_get_buddies();
	
	while(buddies != NULL)
	{
		bdy = buddies->data;
         
		logs = purple_log_get_logs(PURPLE_LOG_IM, purple_buddy_get_name(bdy), purple_buddy_get_account(bdy));
            
		if(logs != NULL)
		{
			gtk_list_store_append(lvn->buddy_liststore, &bdy_level);//, NULL);
			gtk_list_store_set(lvn->buddy_liststore, &bdy_level, 0, purple_buddy_get_alias(bdy),
                        1, purple_buddy_get_contact(bdy), -1);
                        g_list_foreach(logs, (GFunc)purple_log_free, NULL);
                        g_list_free(logs);
		}
		
		buddies = buddies->next;
	}
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(lvn->buddy_liststore),0,GTK_SORT_ASCENDING);
    
}
static void
log_select_search_result_cb(GtkTreeSelection *sel, PidginLogViewerNew *dialog)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(dialog->search_treeview));
	PurpleLog *log = NULL;
	gchar *read = NULL;
        const gchar *filter;
        PurpleLogReadFlags flags;
		
	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;

	gtk_tree_model_get( model, &iter, 2, &log, -1);
	
        if(log == NULL) return;    
        read = purple_log_read(log, &flags);
        if(read == NULL) return;
    
        dialog->search_flags = flags;

        gtk_imhtml_clear(GTK_IMHTML(dialog->imhtml_search));
        gtk_imhtml_set_protocol_name(GTK_IMHTML(dialog->imhtml_search),
        purple_account_get_protocol_name(log->account));

        purple_signal_emit(pidgin_log_get_handle(), "log-displaying", dialog, log);
    
        gtk_imhtml_append_text(GTK_IMHTML(dialog->imhtml_search), read,
                GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_TITLE | GTK_IMHTML_NO_SCROLL |
                ((flags & PURPLE_LOG_READ_NO_NEWLINE) ? GTK_IMHTML_NO_NEWLINE : 0));
    
        g_free(read);
        
        filter = gtk_entry_get_text(GTK_ENTRY(dialog->search_entry));
        gtk_imhtml_search_clear(GTK_IMHTML(dialog->imhtml_search));
        
        if( *filter == '\0' ) return;
        gtk_imhtml_search_find(GTK_IMHTML(dialog->imhtml_search),filter);
        
}
void log_find_log_cb(GtkWidget *w, PidginLogViewerNew *lvn)
{
	GList *buddies, *logs, *l;
        GtkTreeIter iter;
	PurpleBuddy *bdy;
	PurpleLog *log;
	gchar *searchfilter;
	const gchar *entrytext = gtk_entry_get_text(GTK_ENTRY(lvn->search_entry));
	gchar *read, *read2;
        PurpleLogReadFlags flags;
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(lvn->search_treeview));
                 
        gtk_list_store_clear(GTK_LIST_STORE(model));
        gtk_imhtml_clear(GTK_IMHTML(lvn->imhtml_search));
        
        if ( *entrytext == '\0' ) {

#if GTK_CHECK_VERSION(2, 20, 0)
	{
		gtk_spinner_stop(GTK_SPINNER(lvn->search_spinner));
		gtk_widget_hide(lvn->search_spinner);
	}
#endif
                lvn->search_cancelled = TRUE;
                return;
        }
	
	lvn->search_cancelled = FALSE;
#if GTK_CHECK_VERSION(2, 20, 0)
	{
		gtk_spinner_start(GTK_SPINNER(lvn->search_spinner));
		gtk_widget_show(lvn->search_spinner);
	}
#endif
	searchfilter = g_strdup(entrytext);
    		
	buddies = (GList *)purple_blist_get_buddies();
	while(buddies != NULL)
	{
		bdy = buddies->data;
		logs = purple_log_get_logs(PURPLE_LOG_IM, 
                purple_buddy_get_name(bdy), purple_buddy_get_account(bdy));
		l = logs;
        
		while(logs != NULL)
		{
			log = logs->data;
                        
                        if(log == NULL) continue;

			read = purple_log_read(log, &flags);
			
			lvn->search_cancelled = FALSE;
			while (gtk_events_pending()) {
				gtk_main_iteration(); 
                        }
                        
                        if( lvn->search_cancelled == TRUE ) {
				
                                purple_log_free(log);
				if ( l != NULL ) {
					g_list_free((GList*) l);
				}
				
				g_free(read);
				return;
			}
 		    
			read2 = purple_markup_strip_html(read);
                        if(*read2 && purple_strcasestr(read2,searchfilter) !=NULL)
                        {
                                const char *date, *bname;
                                date = purple_utf8_strftime("%a %d %b %Y %I:%M %p",
                                        log->tm ? log->tm : localtime(&log->time));
                                bname = purple_contact_get_alias(purple_buddy_get_contact(bdy));
                                if (*bname == '\0') {
					bname = purple_buddy_get_alias(bdy);
				}
                                gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                                gtk_list_store_set(GTK_LIST_STORE(model), 
                                &iter,0,bname,1,date,2,log,-1);
                        } else {
                                purple_log_free(log);
                        }
            
                        
                        g_free(read);
                        g_free(read2);
            
			logs = logs->next;
		}
                if ( l != NULL ) {
                    g_list_free((GList*) l);
                }
		buddies = buddies->next;
	}
	
	g_free(searchfilter);
#if GTK_CHECK_VERSION(2, 20, 0)
	{
		gtk_spinner_stop(GTK_SPINNER(lvn->search_spinner));
		gtk_widget_hide(lvn->search_spinner);
	}
#endif
	lvn->search_cancelled = TRUE;
    
}
static gboolean
buddy_visible_func (GtkTreeModel *model, GtkTreeIter  *iter, gchar *ftext)
{
  /* Visible if row is non-empty and the string filter is found at the beginning of buddy name */
  gchar *str;
  gchar *ftextU;
  gboolean visible = FALSE;
  
  ftextU = g_strdup(ftext);
  g_strup(ftextU);
    
  gtk_tree_model_get (model, iter, 0, &str, -1);
  g_strup(str);
  if (str && purple_str_has_prefix(str,ftextU))
    visible = TRUE;
  
  g_free (str);
  g_free (ftextU);
  
  return visible;
}

static gint
purple_log_reverse_compare(gconstpointer l1, gconstpointer l2)
{
	return -purple_log_compare(l1,l2);
}

static gint
log_compare_func (GtkTreeModel *model, GtkTreeIter  *a,
GtkTreeIter  *b, gpointer *pointer)
{
  
  PurpleLog *l1,*l2;
  guint ret;
  
  gtk_tree_model_get (model, a, 2, &l1, -1);
  gtk_tree_model_get (model, b, 2, &l2, -1);
  
  ret = purple_log_reverse_compare(l1,l2);   
    
  return ret;
}

void
buddy_filter_change_cb(GtkWidget *entry, PidginLogViewerNew *lvn)
{
	const gchar *filter = gtk_entry_get_text(GTK_ENTRY(entry));
	GtkTreeModel *filtermodel;
		
	if(*filter == '\0')
	{
		gtk_tree_view_set_model(GTK_TREE_VIEW(lvn->buddy_treeview),
                GTK_TREE_MODEL(lvn->buddy_liststore));
		return;
	}
	
	filtermodel = gtk_tree_model_filter_new(
                GTK_TREE_MODEL(lvn->buddy_liststore), NULL);
	gtk_tree_model_filter_set_visible_func(
                GTK_TREE_MODEL_FILTER(filtermodel),
                (GtkTreeModelFilterVisibleFunc) buddy_visible_func,
                (gpointer) filter, NULL);
	
	gtk_tree_view_set_model(GTK_TREE_VIEW(lvn->buddy_treeview),
        GTK_TREE_MODEL(filtermodel));
	g_object_unref(G_OBJECT(filtermodel));
}

void
search_filter_changed_cb(GtkWidget *entry, PidginLogViewerNew *lvn)
{
	const gchar *filter = gtk_entry_get_text(GTK_ENTRY(entry));
        
	if(*filter == '\0')
	{
		gtk_widget_set_sensitive(lvn->search_button, FALSE);
		return;
	}
	
	gtk_widget_set_sensitive(lvn->search_button, TRUE);
}

void
find_filter_changed_cb(GtkWidget *entry, PidginLogViewerNew *lvn)
{
        const gchar *filter = gtk_entry_get_text(GTK_ENTRY(entry));
        
        gtk_imhtml_search_clear(GTK_IMHTML(lvn->imhtml_conv));
        if(*filter == '\0')
	{
		return;
	}
        gtk_imhtml_search_find(GTK_IMHTML(lvn->imhtml_conv), filter);
}

void
delete_log_cb(GtkWidget *button, PidginLogViewerNew *lvn)
{
        uint day, month, year;
        
        if(lvn->log == NULL) return;
        
        if (!purple_log_delete(lvn->log))
	{
		purple_notify_error(NULL, NULL, "Log Deletion Failed",
		                  "Check permissions and try again.");
                return;
	}
        lvn->log = NULL;
        gtk_widget_set_sensitive(lvn->delete_button,FALSE);
        gtk_calendar_get_date(GTK_CALENDAR(lvn->calendar),&year,&month,&day);
        log_mark_calendar_by_month(lvn,month,year);
}

gboolean
delete_log_win_cb(GtkWidget *w, GdkEventAny *e, PidginLogViewerNew *lvn)
{
	lvn->search_cancelled = TRUE;
	while(gtk_events_pending())
	{
		gtk_main_iteration();
	}
	gtk_widget_destroy(lvn->window);
	g_free(lvn);
	return TRUE;
}


static void
pidgin_log_win_show(PurplePluginAction *action)
{
	GtkWidget *window, *hbox1, *vbox1, *notebook;
        GtkWidget *hbox2, *vbox2, *hbox3, *hbox4, *vbox3, *sw, *sw1;
	GtkWidget  *frame, *frame2, *label1, *label2, *label3;
	PidginLogViewerNew *lvn;
	GtkCellRenderer *rend;
	GtkTreeSelection *sel1, *sel2;
	GtkTreeViewColumn *col;
        GtkWidget *buddy_filter_entry, *find_img;
        GtkListStore *logsonday_liststore, *search_liststore;
        	
	lvn = g_new0(PidginLogViewerNew, 1);
	
        lvn->log = NULL;
	lvn->window = window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "View Logs");
		
	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(delete_log_win_cb),lvn);
						 
	lvn->calendar = gtk_calendar_new();
	g_signal_connect(G_OBJECT(lvn->calendar), "prev-month",
					 G_CALLBACK(month_changed_cb),lvn);
	g_signal_connect(G_OBJECT(lvn->calendar), "next-month",
					 G_CALLBACK(month_changed_cb),lvn);
	g_signal_connect(G_OBJECT(lvn->calendar), "prev-year",
					 G_CALLBACK(month_changed_cb),lvn);
	g_signal_connect(G_OBJECT(lvn->calendar), "next-year",
					 G_CALLBACK(month_changed_cb),lvn);
	g_signal_connect(G_OBJECT(lvn->calendar), "day-selected",
					 G_CALLBACK(log_day_selected_cb),lvn);
	
	buddy_filter_entry = gtk_entry_new();
	
	g_signal_connect(G_OBJECT(buddy_filter_entry), "changed",
                     G_CALLBACK(buddy_filter_change_cb), lvn);
	
	vbox1 = gtk_vbox_new(FALSE,PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox1),buddy_filter_entry,FALSE,FALSE,0);
	
	lvn->contact = NULL;
	
	lvn->buddy_liststore = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	populate_log_tree_buddies(lvn);
	lvn->buddy_treeview = gtk_tree_view_new_with_model (
                GTK_TREE_MODEL (lvn->buddy_liststore));
		
	sel1 = gtk_tree_view_get_selection (GTK_TREE_VIEW (lvn->buddy_treeview));
	g_signal_connect (G_OBJECT (sel1), "changed",
			G_CALLBACK (log_select_buddy_cb), lvn);

		
	rend = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(
                GTK_TREE_VIEW(lvn->buddy_treeview),-1,
                "bname", rend, "markup", 0, NULL);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (lvn->buddy_treeview), FALSE);
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	
	gtk_box_pack_start(GTK_BOX(vbox1),sw,TRUE,TRUE,0);
	gtk_container_add(GTK_CONTAINER(sw),lvn->buddy_treeview);
	
	gtk_box_pack_start(GTK_BOX(vbox1),lvn->calendar,FALSE,FALSE,0);
	
	frame = pidgin_create_imhtml(FALSE, &lvn->imhtml_conv, NULL, NULL);
	gtk_widget_set_name(lvn->imhtml_conv, "pidgin_log_imhtml_conv");
	gtk_widget_set_size_request(lvn->imhtml_conv, 320, 360);
        
        find_img = gtk_image_new_from_stock(GTK_STOCK_FIND,GTK_ICON_SIZE_BUTTON);
        logsonday_liststore = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
        gtk_list_store_clear(GTK_LIST_STORE(logsonday_liststore));
        lvn->logsonday_combo = gtk_combo_box_new_with_model(
                GTK_TREE_MODEL(logsonday_liststore));
        g_object_unref(logsonday_liststore);
        gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( lvn->logsonday_combo ), rend, TRUE );
        gtk_cell_layout_set_attributes(
                GTK_CELL_LAYOUT( lvn->logsonday_combo ), rend, "markup", 0, NULL );
        
        /* This is done to adjust the size of the combo box */
        gtk_combo_box_append_text(GTK_COMBO_BOX (lvn->logsonday_combo), "00:00 AM");
        gtk_combo_box_remove_text(GTK_COMBO_BOX (lvn->logsonday_combo), 0);
        
        g_signal_connect (G_OBJECT (lvn->logsonday_combo), "changed",
                G_CALLBACK (logsonday_combo_changed_cb), lvn);
                
        lvn->find_filter_entry = gtk_entry_new();
        g_signal_connect (G_OBJECT (lvn->find_filter_entry), "changed",
                G_CALLBACK (find_filter_changed_cb), lvn);
        
        lvn->delete_button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
        gtk_widget_set_sensitive(lvn->delete_button,FALSE);
        g_signal_connect (G_OBJECT (lvn->delete_button), "clicked",
                G_CALLBACK (delete_log_cb), lvn);
        
        hbox4 = gtk_hbox_new(FALSE,PIDGIN_HIG_BOX_SPACE);
        gtk_box_pack_start(GTK_BOX(hbox4), lvn->logsonday_combo, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox4), lvn->find_filter_entry, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hbox4), find_img, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox4), lvn->delete_button, FALSE, FALSE, 0);

        vbox3 = gtk_vbox_new(FALSE,PIDGIN_HIG_BOX_SPACE);
        gtk_box_pack_start(GTK_BOX(vbox3), hbox4, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox3), frame, TRUE, TRUE, 0);
	
	hbox1 = gtk_hbox_new(FALSE,PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(hbox1), vbox1, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), vbox3, TRUE, TRUE, 0);
    
	
        label3 = gtk_label_new("For:");
	lvn->search_entry = gtk_entry_new();
	lvn->search_button = gtk_button_new_from_stock(GTK_STOCK_FIND);
        gtk_widget_set_sensitive(lvn->search_button, FALSE);
        g_signal_connect(G_OBJECT(lvn->search_entry),"changed",
                G_CALLBACK( search_filter_changed_cb ), lvn);
    
#if GTK_CHECK_VERSION(2, 20, 0)
	{
		lvn->search_spinner = gtk_spinner_new();
	}
#endif
	g_signal_connect(G_OBJECT(lvn->search_entry),
                "activate", G_CALLBACK(log_find_log_cb), lvn);
	g_signal_connect(G_OBJECT(lvn->search_button),
                "clicked", G_CALLBACK(log_find_log_cb), lvn);
		
	hbox2 = gtk_hbox_new(FALSE,PIDGIN_HIG_BOX_SPACE);
        gtk_box_pack_start(GTK_BOX(hbox2),label3,FALSE,FALSE, 10);
	gtk_box_pack_start(GTK_BOX(hbox2),lvn->search_entry,TRUE,TRUE, 0);
#if GTK_CHECK_VERSION(2, 20, 0)
	{
		gtk_box_pack_start(GTK_BOX(hbox2),
                        lvn->search_spinner, FALSE, FALSE, 0);
		
	}
#endif
	gtk_box_pack_start(GTK_BOX(hbox2),lvn->search_button,FALSE,FALSE, 10);
	
	
	search_liststore = gtk_list_store_new (
                3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	gtk_tree_sortable_set_sort_column_id(
                GTK_TREE_SORTABLE(search_liststore),2,GTK_SORT_ASCENDING);
	
        lvn->search_treeview = gtk_tree_view_new_with_model(
                GTK_TREE_MODEL (search_liststore));
	
	col = gtk_tree_view_column_new_with_attributes("Contact", rend, "markup", 0, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(col),TRUE);
	gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col),0);
	gtk_tree_view_append_column(GTK_TREE_VIEW(
                lvn->search_treeview),GTK_TREE_VIEW_COLUMN(col));
    
        col = gtk_tree_view_column_new_with_attributes("Date", rend, "markup", 1, NULL);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(col),TRUE);
	gtk_tree_view_column_set_sort_indicator(GTK_TREE_VIEW_COLUMN(col), TRUE);
	gtk_tree_view_column_set_sort_order(GTK_TREE_VIEW_COLUMN(col),GTK_SORT_ASCENDING);
	gtk_tree_sortable_set_sort_func(
                GTK_TREE_SORTABLE(search_liststore),2,
                (GtkTreeIterCompareFunc) log_compare_func, NULL, NULL);
        g_object_unref(search_liststore);
	gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col),2);
	gtk_tree_view_append_column(GTK_TREE_VIEW(lvn->search_treeview),
                GTK_TREE_VIEW_COLUMN(col));
	
	sel2 = gtk_tree_view_get_selection (GTK_TREE_VIEW (lvn->search_treeview));
	g_signal_connect (G_OBJECT (sel2), "changed",
			G_CALLBACK (log_select_search_result_cb),
			lvn);
        
	frame2 = pidgin_create_imhtml(FALSE, &lvn->imhtml_search, NULL, NULL);
	gtk_widget_set_name(lvn->imhtml_search, "pidgin_log_imhtml_search");
		
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (lvn->search_treeview), TRUE);
	sw1 = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw1), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw1),
                GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	
	gtk_container_add(GTK_CONTAINER(sw1),lvn->search_treeview);
        gtk_widget_set_size_request(sw1, -1, 120);
	
	hbox3 = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
           
        vbox2 = gtk_vbox_new(FALSE,PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox2),hbox2,FALSE,FALSE,5);
	gtk_box_pack_start(GTK_BOX(vbox2),sw1,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(vbox2),frame2,TRUE,TRUE,0);
        gtk_box_pack_start(GTK_BOX(vbox2),hbox3,FALSE,FALSE,0);
    
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), PIDGIN_HIG_BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(hbox1), PIDGIN_HIG_BORDER);
        gtk_container_set_border_width(GTK_CONTAINER(window), 0);
	
	notebook = gtk_notebook_new();
	label1 = gtk_label_new_with_mnemonic("Search");
	label2 = gtk_label_new_with_mnemonic("Conversations");
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, label1);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), hbox1, label2);
	gtk_container_add(GTK_CONTAINER(GTK_WINDOW(window)),notebook);
	
	gtk_widget_show_all(lvn->window);
#if GTK_CHECK_VERSION(2, 20, 0)
	{
		gtk_widget_hide(lvn->search_spinner);
	}
#endif
	
	
	
}




static GList *
actions(PurplePlugin *plugin, gpointer context)
{
	GList *l = NULL;
	PurplePluginAction *act = NULL;

	act = purple_plugin_action_new("View Logs", pidgin_log_win_show);
	l = g_list_append(l, act);

	return l;
}


static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                       /**< type           */
	PIDGIN_PLUGIN_TYPE,                           /**< ui_requirement */
	0,                                            /**< flags          */
	NULL,                                         /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                      /**< priority       */

	"gtk-log_view",                               /**< id             */
	"Log Viewer",                             /**< name           */
	VERSION,                              /**< version        */
	                                              /**  summary        */
	"View and search logs in a user-friendly way.",
	                                              /**  description    */
	"this plugin presents a new user-friendly intuitive log viewer.",
	"Tirtha Chatterjee <tirtha.p.chatterjee@gmail.com>",             /**< author         */
	"http://thebengaliheart.wordpress.com/",                               /**< homepage       */

	NULL,                                  /**< load           */
	NULL,                                /**< unload         */
	NULL,                                         /**< destroy        */

	NULL,                                         /**< ui_info        */
	NULL,                                         /**< extra_info     */
	NULL,
	actions,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(log_viewer, init_plugin, info)
