#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include <cairo.h>
#include <poppler.h>
#include <clutter/clutter.h>

/** presenter stage shows the display for the talker */
static ClutterStage *presenter_stage;
/** show stage is the display for the audience */
static ClutterStage *show_stage;
/** number of slides */
static unsigned slide_count;
/** currently shown slide */
static unsigned current_slide_index;
/** count down timer */
static ClutterText *onscreen_clock;
/** presentation notes */
static ClutterText *notes;

static const unsigned SLIDE_X_RESOLUTION = 1024;
static const unsigned SLIDE_Y_RESOLUTION = 768;

typedef struct slide_info {
   unsigned index;
   PopplerPage *pdf;
   ClutterActor *actor;
   ClutterActor *show_actor;
} slide_info;
/** internal info for each slide */
static slide_info *slide_meta_data;

static gboolean draw_slide(ClutterCairoTexture *canvas, cairo_t *cr, gpointer data)
{
   slide_info *info = (slide_info*) data;
   PopplerPage *page = info->pdf;
   assert (page != NULL);
   double doc_w, doc_h;
   poppler_page_get_size(page, &doc_w, &doc_h);

   /* scale for the rendering */
   double scale_x = clutter_actor_get_width(CLUTTER_ACTOR(canvas)) / doc_w;
   double scale_y = clutter_actor_get_height(CLUTTER_ACTOR(canvas)) / doc_h;
   cairo_scale(cr, scale_x, scale_y);

   /* render pdf */
   poppler_page_render(page, cr);
   return true;
}

static gboolean draw_presenter_slide(ClutterCairoTexture *canvas, cairo_t *cr, gpointer data)
{
   slide_info *info = (slide_info*) data;
   PopplerPage *page = info->pdf;
   assert (page != NULL);

   /* render pdf */
   poppler_page_render(page, cr);
   return true;
}

static void init_slide_actors(const char *filename)
{
   PopplerDocument *document = poppler_document_new_from_file(filename, NULL, NULL);
   const int pc = poppler_document_get_n_pages(document);
   slide_count = pc;
   current_slide_index = 0;
   slide_meta_data = (slide_info*) malloc(sizeof(slide_info) * pc);
   assert (slide_meta_data != NULL);

   for (int i=0; i<pc; ++i) {
      ClutterCairoTexture *show_canvas = (ClutterCairoTexture*)
         clutter_cairo_texture_new(SLIDE_X_RESOLUTION, SLIDE_Y_RESOLUTION);
      ClutterCairoTexture *presenter_canvas = (ClutterCairoTexture*)
         clutter_cairo_texture_new(400, 300);
      slide_info *info = &(slide_meta_data[i]);
      info->index = i;
      PopplerPage *page = poppler_document_get_page(document, i);
      assert (page != NULL);
      info->pdf = page;

         info->show_actor = CLUTTER_ACTOR(show_canvas);
         printf("added slide %d to show stage\n", i);
         info->actor = CLUTTER_ACTOR(presenter_canvas);
         printf("added slide %d to presenter stage\n", i);

      g_signal_connect (show_canvas, "draw", G_CALLBACK (draw_slide), info);
      g_signal_connect (presenter_canvas, "draw", G_CALLBACK (draw_presenter_slide), info);

      if (i == current_slide_index) {
         clutter_actor_show(CLUTTER_ACTOR(show_canvas));
         clutter_actor_show(CLUTTER_ACTOR(presenter_canvas));
      } else {
         clutter_actor_hide(CLUTTER_ACTOR(show_canvas));
         clutter_actor_hide(CLUTTER_ACTOR(presenter_canvas));
      }

      clutter_container_add_actor (CLUTTER_CONTAINER(show_stage), CLUTTER_ACTOR(show_canvas));
      clutter_container_add_actor (CLUTTER_CONTAINER(presenter_stage), CLUTTER_ACTOR(presenter_canvas));

      /* bind the size of the canvas to that of the stage */
      clutter_actor_add_constraint(CLUTTER_ACTOR(show_canvas),
         clutter_bind_constraint_new(CLUTTER_ACTOR(show_stage), CLUTTER_BIND_SIZE, 0));

      /* invalidate to trigger drawing */
      clutter_cairo_texture_invalidate(show_canvas);
      clutter_cairo_texture_invalidate(presenter_canvas);
   }
}

static void place_slides(void)
{
   const float rotation = -30;

   /* hide all */
   for (int i=0; i<slide_count; ++i) {
      ClutterActor *a = slide_meta_data[i].actor;
      clutter_actor_hide(a);
      ClutterActor *show = slide_meta_data[i].show_actor;
      clutter_actor_hide(show);
   }
   const float stage_width = clutter_actor_get_width(CLUTTER_ACTOR(presenter_stage));

   /* show current slide */
   ClutterActor *current = slide_meta_data[current_slide_index].actor;
   const float slide_width = clutter_actor_get_width(CLUTTER_ACTOR(current));
   const float x = (stage_width - slide_width) / 2.0;
   clutter_actor_set_position(current, x, 0);
   clutter_actor_set_rotation(current, CLUTTER_Y_AXIS, 0, 0, 0, 0);
   clutter_actor_raise_top(current);
   clutter_actor_show(current);

   /* show current show slide */
   ClutterActor *current_show = slide_meta_data[current_slide_index].show_actor;
   clutter_actor_show(current_show);


   /* show previous slide */
   if (current_slide_index > 0) {
      ClutterActor *previous = slide_meta_data[current_slide_index-1].actor;
      clutter_actor_set_position(previous, x-slide_width, 0);
      clutter_actor_set_rotation(previous, CLUTTER_Y_AXIS, rotation, slide_width, 0, 0);
      clutter_actor_show(previous);
   }

   /* show next slide */
   if (current_slide_index+1 < slide_count) {
      ClutterActor *next = slide_meta_data[current_slide_index+1].actor;
      clutter_actor_set_position(next, x+slide_width, 0);
      clutter_actor_set_rotation(next, CLUTTER_Y_AXIS, -rotation, 0, 0, 0);
      clutter_actor_show(next);
   }

   /* update notes */
   const int MAX_LEN = 1000;
   char notes_text[MAX_LEN];
   notes_text[0] = 0;
   unsigned len = 0;
   PopplerPage *page = slide_meta_data[current_slide_index].pdf;
   GList *l = poppler_page_get_annot_mapping(page);
   for (; l; l = l->next) {
      PopplerAnnotMapping *mapping = (PopplerAnnotMapping *)l->data;
      PopplerAnnot *annot = mapping->annot;
      PopplerAnnotType type = poppler_annot_get_annot_type(annot);
      switch (type) {
         case POPPLER_ANNOT_TEXT: {
            gchar *text = poppler_annot_get_contents(annot);
            strncat(notes_text, text, MAX_LEN-len);
            len = strlen(notes_text);
            break;
            }
         case POPPLER_ANNOT_LINK:
            break; /* ignore this type */
         default:
            printf("unknown annotation type %d\n", type);
            break;
      }
   }
   clutter_text_set_text(CLUTTER_TEXT(notes), notes_text);
}

static void next_slide(void)
{
   if (current_slide_index+1 == slide_count)
      return;
   current_slide_index += 1;
   place_slides();
}

static void previous_slide(void)
{
   if (current_slide_index == 0)
      return;
   current_slide_index -= 1;
   place_slides();
}

static void handle_key_input(ClutterCairoTexture *canvas, ClutterEvent *ev)
{
   ClutterKeyEvent *event = (ClutterKeyEvent*) ev;
   switch (event->keyval) {
      case 65363: /* RIGHT */
      case 65364: /* DOWN */
      case ' ':
         next_slide();
         break;
      case 65361: /* LEFT */
      case 65362: /* UP */
      case 65288: /* BACKSPACE */
         previous_slide();
         break;
      case 65307: /* ESCAPE */
      case 'q':
      case 'Q': /* exit */
         clutter_main_quit();
         break;
      case 'f':
         clutter_stage_set_fullscreen(show_stage, !clutter_stage_get_fullscreen(show_stage));
         place_slides();
         break;
      default: /* ignore */
         printf("key press %d (%d %d)\n", event->keyval, event->hardware_keycode, event->unicode_value);
         break;
   }
}

void create_show_stage()
{
   show_stage = CLUTTER_STAGE(clutter_stage_new());

   ClutterColor     stage_color = { 0x0, 0x0, 0x0, 0xff };
   clutter_stage_set_color (CLUTTER_STAGE (show_stage), &stage_color);
   clutter_actor_set_size(CLUTTER_ACTOR(show_stage), 640, 480);
   clutter_stage_set_title(show_stage, "Presentation Window");

   clutter_actor_show (CLUTTER_ACTOR(show_stage));

   g_signal_connect (show_stage, "key-press-event",
         G_CALLBACK (handle_key_input),
         NULL);
}

void create_presenter_stage()
{
   presenter_stage = CLUTTER_STAGE(clutter_stage_new());

   ClutterColor     stage_color = { 0x0, 0x0, 0x0, 0xff };
   clutter_stage_set_color (CLUTTER_STAGE (presenter_stage), &stage_color);
   clutter_actor_set_size(CLUTTER_ACTOR(presenter_stage), 800, 600);
   clutter_stage_set_title(presenter_stage, "PDF Presenter");

   clutter_actor_show (CLUTTER_ACTOR(presenter_stage));

   g_signal_connect (presenter_stage, "key-press-event",
         G_CALLBACK (handle_key_input),
         NULL);
}

static void update_time()
{
   time_t t = time(NULL);
   struct tm *tmp = localtime(&t);
   char time_string[20];
   strftime(time_string, 20, "%H:%M:%S", tmp);
   clutter_text_set_text(CLUTTER_TEXT(onscreen_clock), time_string);
}

static void create_onscreen_clock()
{
   ClutterColor text_color = { 0xff, 0xff, 0xcc, 0xff };
   onscreen_clock = CLUTTER_TEXT(clutter_text_new());
   clutter_text_set_font_name(CLUTTER_TEXT(onscreen_clock), "Mono 24px");
   clutter_text_set_color(CLUTTER_TEXT(onscreen_clock), &text_color);
   clutter_actor_set_position (CLUTTER_ACTOR(onscreen_clock), 5, 300);
   clutter_container_add_actor(CLUTTER_CONTAINER(presenter_stage), CLUTTER_ACTOR(onscreen_clock));
   clutter_actor_show (CLUTTER_ACTOR(onscreen_clock));
   update_time();
}

static void create_notes_actor()
{
   ClutterColor text_color = { 0xff, 0xff, 0xcc, 0xff };
   notes = CLUTTER_TEXT(clutter_text_new());
   clutter_text_set_font_name(CLUTTER_TEXT(notes), "Sans 24px");
   clutter_text_set_color(CLUTTER_TEXT(notes), &text_color);
   clutter_text_set_text(CLUTTER_TEXT(notes), "");
   clutter_actor_set_position (CLUTTER_ACTOR(notes), 200, 300);
   clutter_container_add_actor(CLUTTER_CONTAINER(presenter_stage), CLUTTER_ACTOR(notes));
   clutter_actor_show (CLUTTER_ACTOR(notes));
}

int main(int argc, char** argv)
{
   if (argc == 1) {
      printf("Usage: %s [filename]\n", argv[0]);
      return 0;
   }
   assert (argc == 2);
   char filename[1000] = "file://"; // TODO length safe?
   realpath(argv[1], filename+7);

   g_type_init();
   if (clutter_init(&argc, &argv) != CLUTTER_INIT_SUCCESS)
       return 1;

   assert (clutter_feature_available(CLUTTER_FEATURE_STAGE_MULTIPLE));

   create_show_stage();
   create_presenter_stage();
   init_slide_actors(filename);
   create_onscreen_clock();
   create_notes_actor();
   place_slides();

   ClutterTimeline *timeline = clutter_timeline_new(1000);
   g_signal_connect(timeline, "new-frame", G_CALLBACK(update_time), NULL);
   clutter_timeline_set_loop(timeline, TRUE);
   clutter_timeline_start(timeline);

   printf("GO!\n");
   clutter_main();

   return 0;
}
