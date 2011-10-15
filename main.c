#include <cairo.h>
#include <poppler.h>
#include <clutter/clutter.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

/** presenter stage shows the display for the talker */
//static ClutterActor *presenter_stage;
/** show stage is the display for the audience */
static ClutterActor *show_stage;
/** number of slides */
static unsigned slide_count;
/** currently shown slide */
static unsigned current_slide_index;

typedef struct slide_info {
   unsigned index;
   PopplerPage *pdf;
   ClutterActor *actor;
} slide_info;
/** internal info for each slide */
static slide_info *slide_meta_data;

static gboolean draw_slide(ClutterCairoTexture *canvas, cairo_t *cr, gpointer data)
{
   slide_info *info = (slide_info*) data;
   printf("render page %d on %x\n", info->index, cr);
   PopplerPage *page = info->pdf;
   assert (page != NULL);
   poppler_page_render(page, cr);
   printf("rendered\n");
   return true;
}

static void init_slide_actors(char *filename, ClutterActor *stage)
{
   PopplerDocument *document = poppler_document_new_from_file(filename, NULL, NULL);
   const int pc = poppler_document_get_n_pages(document);
   slide_count = pc;
   current_slide_index = 0;
   slide_meta_data = (slide_info*) malloc(sizeof(slide_info) * pc);
   assert (slide_meta_data != NULL);

   for (int i=0; i<pc; ++i) {
      ClutterActor *canvas = clutter_cairo_texture_new(640, 480);
      slide_info *info = &(slide_meta_data[i]);
      printf("%x[%d] = %x", slide_meta_data, i, info);
      info->index = i;
      PopplerPage *page = poppler_document_get_page(document, i);
      assert (page != NULL);
      info->pdf = page;
      info->actor = canvas;

      g_signal_connect (canvas, "draw", G_CALLBACK (draw_slide), info);

      if (i == current_slide_index)
         clutter_actor_show(canvas);
      else
         clutter_actor_hide(canvas);

      clutter_container_add_actor (CLUTTER_CONTAINER (stage), canvas);

      /* bind the size of the canvas to that of the stage */
      clutter_actor_add_constraint (canvas, clutter_bind_constraint_new (stage, CLUTTER_BIND_SIZE, 0));

      /* make sure to match allocation to canvas size */
      clutter_cairo_texture_set_auto_resize (CLUTTER_CAIRO_TEXTURE (canvas), TRUE);

      printf("added page %d\n", i);
   }
}

static void next_slide(void)
{
   if (current_slide_index+1 == slide_count)
      return;
   ClutterActor *current = slide_meta_data[current_slide_index].actor;
   current_slide_index += 1;
   ClutterActor *next = slide_meta_data[current_slide_index].actor;
   clutter_actor_show(next);
   clutter_actor_hide(current);
}

static void previous_slide(void)
{
   if (current_slide_index == 0)
      return;
   ClutterActor *current = slide_meta_data[current_slide_index].actor;
   current_slide_index -= 1;
   ClutterActor *next = slide_meta_data[current_slide_index].actor;
   clutter_actor_show(next);
   clutter_actor_hide(current);
}

static void handle_key_input(ClutterCairoTexture *canvas, ClutterEvent *ev)
{
   ClutterKeyEvent *event = (ClutterKeyEvent*) ev;
   printf("key event %d %d %d\n", event->keyval, event->hardware_keycode, event->unicode_value);
   switch (event->keyval) {
      case 65363: /* RIGHT */
      case ' ':
         next_slide();
         break;
      case 65361: /* LEFT */
      case 65288: /* BACKSPACE */
         previous_slide();
         break;
      case 'q':
      case 'Q': /* exit */
         clutter_main_quit();
         break;
      case 'f':
         clutter_stage_set_fullscreen(show_stage, !clutter_stage_get_fullscreen(show_stage));
         break;
      default: /* ignore */
         break;
   }
}

int main(int argc, char** argv)
{
   assert (argc == 2);
   g_type_init();
   char *filename = "file:///home/beza1e1/dev/pdfpresenter/example.pdf";

   if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
       return 1;

   ClutterActor    *stage = clutter_stage_get_default ();
   ClutterColor     stage_color = { 0x0, 0x0, 0x0, 0xff };
   clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
   clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
   show_stage = stage;

   init_slide_actors(filename, stage);

   /* Create a timeline to manage animation */
   ClutterTimeline *timeline = clutter_timeline_new (6000);
   clutter_timeline_set_loop (timeline, TRUE);

   clutter_actor_show (stage);

   clutter_timeline_start (timeline);

   g_signal_connect (stage, "key-press-event",
         G_CALLBACK (handle_key_input),
         NULL);

   printf("GO!\n");
   clutter_main();

   return 0;
}
