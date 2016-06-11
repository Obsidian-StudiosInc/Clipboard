#include "e_mod_main.h"
#include <string.h>
#include <Ecore.h>


#define __UNUSED__
#define _(S) S
#define ENABLE_DEBUG 1
#define DEBUG(f, ...) if (ENABLE_DEBUG) \
    printf("[clipboard] "f "\n", __VA_ARGS__)

#define TIMEOUT_1 1.0 // interval for timer

/* actual module specifics */



typedef struct _Instance Instance;
struct _Instance
{
   E_Gadcon_Client *gcc;
   E_Menu *menu;
   Ecore_X_Window win;
   Ecore_Timer  *check_timer;
   Evas_Object *o_button;
   Eina_List *handle;
   Eina_List *items;
};

/* gadcon requirements */
static E_Gadcon_Client *_gc_init(E_Gadcon * gc, const char *name, const char *id, const char *style);
static void             _gc_shutdown(E_Gadcon_Client * gcc);
static void             _gc_orient(E_Gadcon_Client * gcc, E_Gadcon_Orient orient);
static const char      *_gc_label(const E_Gadcon_Client_Class *client_class);
static                  Evas_Object *_gc_icon(const E_Gadcon_Client_Class *client_class, Evas * evas);
static const char      *_gc_id_new(const E_Gadcon_Client_Class *client_class);

static void e_clip_upload_completed(Clip_Data *clip_data);
static void _e_clip_clear_list(Instance *inst);
static void _clip_button_cb_mouse_down(void *data, Evas *evas, Evas_Object *obj, Evas_Event_Mouse_Down *ev);
static Eina_Bool _clip_x_selection_notify_handler(Instance *instance, int type, void *event);
static void _clip_menu_post_cb(void *data, E_Menu *menu);
static void _menu_clip_request_click_cb(Instance *inst, E_Menu *m, E_Menu_Item *mi);
static Eina_Bool _clipboard_cb(void *data);
static void _menu_clip_item_click_cb(Clip_Data *selected_clip);
static void _free_clip_data(Clip_Data *cd);
static Eina_Bool _selection_notify_cb(void *data, int type, void *event);
static void _cb_action(void *data);


static E_Module *clipboard_module = NULL;
static E_Action *act = NULL;
static Eina_List *float_list = NULL;
//~ static E_Config_DD *conf_edd = NULL;
//~ static E_Config_DD *conf_item_edd = NULL;

const char *TMP_text = " ";
int item_num;

/* and actually define the gadcon class that this module provides (just 1) */
static const E_Gadcon_Client_Class _gadcon_class = {
   GADCON_CLIENT_CLASS_VERSION,
   "clipboard",
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL,
      e_gadcon_site_is_not_toolbar
   },
   E_GADCON_CLIENT_STYLE_PLAIN
};


static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Evas_Object *o;
   E_Gadcon_Client *gcc;
 

   inst = E_NEW(Instance, 1);

   item_num=0;
   /*
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf), "%s/e-module-share.edj", e_module_dir_get(share_module));
   
   o = edje_object_add(gc->evas);
   if (!e_theme_edje_object_set(o, "base/theme/modules/share", "modules/share/main"))
     edje_object_file_set(o, buf, "modules/share/main");
   edje_object_signal_emit(o, "passive", "");
     */

   o = e_icon_add(gc->evas);
   e_icon_fdo_icon_set(o, "edit-paste");
   evas_object_show(o);

   gcc = e_gadcon_client_new(gc, name, id, style, o);
   gcc->data = inst;

   inst->gcc = gcc;
   inst->win = ecore_evas_window_get(gc->ecore_evas);
   inst->o_button = o;

   e_gadcon_client_util_menu_attach(gcc);

   evas_object_event_callback_add(inst->o_button, EVAS_CALLBACK_MOUSE_DOWN, (Evas_Object_Event_Cb)_clip_button_cb_mouse_down, inst);
   
   E_LIST_HANDLER_APPEND(inst->handle, ECORE_X_EVENT_SELECTION_NOTIFY, _clip_x_selection_notify_handler, inst);
  
   // Ensure our gadget is initialized to current clipboard contents
   ecore_x_selection_clipboard_request(inst->win, ECORE_X_SELECTION_TARGET_UTF8_STRING); 

   inst->check_timer = ecore_timer_add(TIMEOUT_1, _clipboard_cb, inst);
   return gcc;
}

static void
_cb_action(void *data)
{
   
   Instance *inst = (Instance*)data;
   
    Evas_Coord x, y;
    E_Container *con;
    E_Manager *man;
    
    E_Menu_Item *mi;
    Eina_List *it;
    Clip_Data *clip;
    
     if (!inst) return;
     
        /* Coordinates and sizing */
        man = e_manager_current_get();
        con = e_container_current_get(man);
        ecore_x_pointer_xy_get(con->win, &x, &y);
       
        inst->menu = e_menu_new();

        e_menu_post_deactivate_callback_set(inst->menu,
                _clip_menu_post_cb, inst);
                
        inst->items = float_list;
        if (inst->items){
            EINA_LIST_FOREACH(inst->items, it, clip)
            {   mi = e_menu_item_new(inst->menu);
                e_menu_item_label_set(mi, clip->name);
                DEBUG("%s",clip->name);
                e_menu_item_callback_set(mi, (E_Menu_Cb)_menu_clip_item_click_cb, clip);        
            }
        }



		mi = e_menu_item_new(inst->menu);
        e_menu_item_separator_set(mi, EINA_TRUE);
        
        mi = e_menu_item_new(inst->menu);
        e_menu_item_label_set(mi, _("Add clipboard content"));
        e_util_menu_item_theme_icon_set(mi, "edit-paste");
        e_menu_item_callback_set(mi, (E_Menu_Cb)_clipboard_cb, inst);

        mi = e_menu_item_new(inst->menu);
        e_menu_item_separator_set(mi, EINA_TRUE);
       
       
        
         mi = e_menu_item_new(inst->menu);
        e_menu_item_label_set(mi, _("Clear"));
        e_util_menu_item_theme_icon_set(mi, "edit-clear");
        e_menu_item_callback_set(mi, (E_Menu_Cb)_e_clip_clear_list, inst);

		mi = e_menu_item_new(inst->menu);
        e_menu_item_separator_set(mi, EINA_TRUE);
        
         mi = e_menu_item_new(inst->menu);
        e_menu_item_label_set(mi, _("Settings"));
        e_util_menu_item_theme_icon_set(mi, "preferences-system");
        
                 //~ e_gadcon_locked_set(inst->gcc->gadcon, EINA_TRUE);
       e_menu_activate_mouse(inst->menu, e_util_zone_current_get
                (e_manager_current_get()),
                x, y, 1, 1, 2, 1);         
    //~ 
}



static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst;

   inst = gcc->data;
   E_FREE_LIST(inst->handle, ecore_event_handler_del);
   inst->handle = NULL;

   if (inst->menu)
     {
        e_menu_post_deactivate_callback_set(inst->menu, NULL, NULL);
        e_object_del(E_OBJECT(inst->menu));
        inst->menu = NULL;
     }

   E_FREE_LIST(inst->items, _free_clip_data);
   inst->items = NULL;

   evas_object_del(inst->o_button);
   ecore_timer_del(inst->check_timer);
   ecore_shutdown();
   E_FREE(inst);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient)
{
   e_gadcon_client_aspect_set (gcc, 16, 16);
   e_gadcon_client_min_size_set (gcc, 16, 16);
}

static const char *
_gc_label (const E_Gadcon_Client_Class *client_class)
{
   return "Clipboard";
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class, Evas * evas)
{
   Evas_Object *o;
   /*
   char buf[PATH_MAX];

   o = edje_object_add(evas);
   snprintf (buf, sizeof(buf), "%s/e-module-share.edj", e_module_dir_get(share_module));
   edje_object_file_set(o, buf, "icon");
   */

   o = e_icon_add(evas);
   e_icon_fdo_icon_set(o, "edit-paste");

   return o;
}

static const char *
_gc_id_new (const E_Gadcon_Client_Class *client_class)
{
   return _gadcon_class.name;
}


static void
_clip_button_cb_mouse_down(void *data, Evas *evas, Evas_Object *obj, Evas_Event_Mouse_Down *ev)
{
    Instance *inst = (Instance*)data;
   
    Evas_Coord x, y, w, h;
    int cx, cy;
    int dir;
    
    E_Menu_Item *mi;
    Eina_List *it;
    Clip_Data *clip;
    
    

    if (!inst) return;



    if ((ev->button == 1) && (!inst->menu))
    {
        /* Coordinates and sizing */
        evas_object_geometry_get(inst->o_button, &x, &y, &w, &h);
        e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon, &cx, &cy,
                                          NULL, NULL);
        x += cx;
        y += cy;

        inst->menu = e_menu_new();
        
        if (inst->items){
            EINA_LIST_FOREACH(inst->items, it, clip)
           {   mi = e_menu_item_new(inst->menu);
                e_menu_item_label_set(mi, clip->name);
                DEBUG("num:%d",item_num);
                DEBUG("name:%s",clip->name);
                e_menu_item_callback_set(mi, (E_Menu_Cb)_menu_clip_item_click_cb, clip);
           }
       }

			
		mi = e_menu_item_new(inst->menu);
        e_menu_item_separator_set(mi, EINA_TRUE);
        
        mi = e_menu_item_new(inst->menu);
        e_menu_item_label_set(mi, _("Add clipboard content"));
        e_util_menu_item_theme_icon_set(mi, "edit-paste");

        e_menu_item_callback_set(mi, (E_Menu_Cb)_menu_clip_request_click_cb, inst);

        mi = e_menu_item_new(inst->menu);
        e_menu_item_separator_set(mi, EINA_TRUE);
        
         mi = e_menu_item_new(inst->menu);
        e_menu_item_label_set(mi, _("Clear"));
        e_util_menu_item_theme_icon_set(mi, "edit-clear");
        e_menu_item_callback_set(mi, (E_Menu_Cb)_e_clip_clear_list, inst);

		mi = e_menu_item_new(inst->menu);
        e_menu_item_separator_set(mi, EINA_TRUE);
        
        mi = e_menu_item_new(inst->menu);
        e_menu_item_label_set(mi, _("Settings"));
        e_util_menu_item_theme_icon_set(mi, "preferences-system");
        //~ e_menu_item_callback_set(mi, (E_Menu_Cb)_e_clip_clear_list, inst);
        
        e_menu_post_deactivate_callback_set(inst->menu,
                _clip_menu_post_cb, inst);

        /* Proper menu orientation */
        switch (inst->gcc->gadcon->orient)
        {
            case E_GADCON_ORIENT_TOP:
            case E_GADCON_ORIENT_CORNER_TL:
            case E_GADCON_ORIENT_CORNER_TR:
                dir = E_MENU_POP_DIRECTION_DOWN;
                break;

            case E_GADCON_ORIENT_BOTTOM:
            case E_GADCON_ORIENT_CORNER_BL:
            case E_GADCON_ORIENT_CORNER_BR:
                dir = E_MENU_POP_DIRECTION_UP;
                break;

            case E_GADCON_ORIENT_LEFT:
            case E_GADCON_ORIENT_CORNER_LT:
            case E_GADCON_ORIENT_CORNER_LB:
                dir = E_MENU_POP_DIRECTION_RIGHT;
                break;

            case E_GADCON_ORIENT_RIGHT:
            case E_GADCON_ORIENT_CORNER_RT:
            case E_GADCON_ORIENT_CORNER_RB:
                dir = E_MENU_POP_DIRECTION_LEFT;
                break;

            case E_GADCON_ORIENT_FLOAT:
            case E_GADCON_ORIENT_HORIZ:
            case E_GADCON_ORIENT_VERT:
            default:
                dir = E_MENU_POP_DIRECTION_AUTO;
                break;
        }

        e_gadcon_locked_set(inst->gcc->gadcon, EINA_TRUE);

        /* We display not relatively to the gadget, but similarly to
         * the start menu - thus the need for direction etc.
         */
        e_menu_activate_mouse(inst->menu,
                e_util_zone_current_get
                (e_manager_current_get()),
                x, y, w, h, dir, ev->timestamp);
    }
}




static Eina_Bool
_clip_x_selection_notify_handler(Instance *instance, int type, void *event)
{
   
   Ecore_X_Event_Selection_Notify *ev;
   Clip_Data *cd = NULL;
  
   const char *data;  

   if (!instance)
     return EINA_TRUE;

   ev = event;
   
   if ((ev->selection == ECORE_X_SELECTION_CLIPBOARD) &&
       (strcmp(ev->target, ECORE_X_SELECTION_TARGET_UTF8_STRING) == 0))
     {
        Ecore_X_Selection_Data_Text *text_data;
        
        text_data = ev->data;
      
        if ((text_data->data.content == ECORE_X_SELECTION_CONTENT_TEXT) &&
            (text_data->text))
          {
			  		  
              char buf[20];
              if (text_data->data.length == 0)  return EINA_TRUE;

              cd = E_NEW(Clip_Data, 1);
              cd->inst = instance;
              //~ snprintf(buf, ((text_data->data.length >= sizeof(buf)) ? (sizeof(buf) - 1) : text_data->data.length), text_data->text);
              
              strncpy(buf, text_data->text, 20);
              
              asprintf(&cd->name, "%s", buf);
              asprintf(&cd->content, "%s", text_data->text);
              
              			 
              if (strcmp(text_data->text,TMP_text)!=0)
              {
				e_clip_upload_completed(cd);
                asprintf(&TMP_text, "%s", text_data->text);
             
             // saving list to the file---------------     
                FILE *f = fopen("history.txt", "a");
				char separator = '^';
				
				if (f == NULL)
				{
					exit(1);
				}
				fprintf(f, "%s", text_data->text);
				fprintf(f, "%c", separator);				
				fclose(f);
		     //----------------------------------------
		     }
          }
     }


   return ECORE_CALLBACK_DONE;
}



/* Updates the X selection with the selected text of the entry */
void
_clipboard_update(const char *text, const Instance *inst)
{
  EINA_SAFETY_ON_NULL_RETURN(inst);
  EINA_SAFETY_ON_NULL_RETURN(text);

  ecore_x_selection_clipboard_set(inst->win, text, strlen(text) + 1);
  //~ e_util_dialog_internal ("Pišta",text);
}

void e_clip_upload_completed(Clip_Data *cd)
{
	Eina_List *it;
	Clip_Data *clip;
    if (!cd) return;
    
    //Solve duplicity item in Eina list
    EINA_LIST_FOREACH(((Instance*)cd->inst)->items, it, clip)
           {   
			   if (strcmp(cd->content, clip->content)==0)
			   ((Instance*)cd->inst)->items = eina_list_remove(((Instance*)cd->inst)->items,clip);
           }
    
    //adding item to the list
    if (item_num<20) {
    ((Instance*)cd->inst)->items = eina_list_prepend(((Instance*)cd->inst)->items, cd);   
	item_num++;
	}
	else	{
	//~ remove last item from the list 
	((Instance*)cd->inst)->items = eina_list_remove_list(((Instance*)cd->inst)->items, eina_list_last(((Instance*)cd->inst)->items)); 
	//~ add clipboard data stored in cd to the list as a first item
	((Instance*)cd->inst)->items = eina_list_prepend(((Instance*)cd->inst)->items, cd);
	}
	
	float_list=((Instance*)cd->inst)->items;
    
    //~ DEBUG("%s\n","som v tele");
}


void _e_clip_clear_list(Instance *inst)
{ 
    if (!inst) 
        return;  

    if (inst->items)
        E_FREE_LIST(inst->items, _free_clip_data); 
    item_num = 0;
    inst->items = NULL;
    ecore_x_selection_clipboard_clear();
}

static Eina_Bool
_clipboard_cb(void *data)
{
	Instance *inst = data;
    if (!inst) 
   	    return;   	
    
    ecore_x_selection_clipboard_request(inst->win, ECORE_X_SELECTION_TARGET_UTF8_STRING);
   return 1;
   }

static void
_menu_clip_request_click_cb(Instance *inst, E_Menu *m, E_Menu_Item *mi)
{
    if (!inst) 
   	    return;   	
    
    ecore_x_selection_clipboard_request(inst->win, ECORE_X_SELECTION_TARGET_UTF8_STRING);
    //~ e_util_dialog_internal ("Ahoj","1");
}

static void
_menu_clip_item_click_cb(Clip_Data *selected_clip)
{
	_clipboard_update(selected_clip->content, selected_clip->inst);
}


static void
_clip_menu_post_cb(void *data, E_Menu *menu)
{
   Instance *inst = data;

   if (!inst) return;
   //~ e_gadcon_locked_set(inst->gcc->gadcon, EINA_FALSE);
   inst->menu = NULL;
}

static void _free_clip_data(Clip_Data *cd)
{
    free(cd->name);
    free(cd->content);
    free(cd->url);
    free(cd);
    float_list = NULL;
}

/* module setup */
EAPI E_Module_Api e_modapi = {
  E_MODULE_API_VERSION,
  "Clipboard"
};

EAPI void *
e_modapi_init (E_Module * m)
{
  
    clipboard_module = m;
   e_gadcon_provider_register(&_gadcon_class);
  
  act = e_action_add("clipboard");
   if (act)
     {
	act->func.go = (void *) _cb_action;
	
	e_action_predef_name_set("Clipboard","Show float menu", "clipboard", "<none>", NULL, 0);
     }
     
     
    // file history.txt reading, it will be in separate function later 
       
    Clip_Data *cd = NULL;
    cd = E_NEW(Clip_Data, 1);
	
    int c;
    int num_chars=0;
    char *text;
    text = (char *) malloc(1000); //1000 just for now
	FILE *file;
	file = fopen("history.txt", "r");
	char buf[20];
	
	if (file) 
	{
		while ((c = getc(file)) != EOF)
		{
			if (c != '^')
		    text[num_chars++]=(char) c;
		    else
		    {
		    
		    text[num_chars++]= '\0';
            num_chars=0;
            strncpy(buf, text, 20);
			asprintf(&cd->name, "%s", buf);
			asprintf(&cd->content, "%s", text);
			//e_clip_upload_completed(cd); 
            
		    }
        }    
        
        
		
        
	}
	else return;
	
	
    
   fclose(file);
     
   return clipboard_module;
}

EAPI int
e_modapi_shutdown (E_Module * m)
{
	Instance *inst;
 
 
  if (act)
     {
	e_action_predef_name_del("Clipboard", "Show menu");
	e_action_del("clipboard");
	act = NULL;
     }
 clipboard_module = NULL; 
  e_gadcon_provider_unregister(&_gadcon_class);
 
  return 1;
}

EAPI int
e_modapi_save(E_Module * m)
{
  return 1;
}

