#include <cairo.h>
#include <poppler.h>
#include <clutter/clutter.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

/** presenter stage shows the display for the talker */
static ClutterActor *presenter_stage;
/** show stage is the display for the audience */
static ClutterActor *show_stage;
/** number of slides */
static unsigned slide_count;
/** currently shown slide */
static unsigned current_slide_index;

typedef struct slide_info {
   unsigned index;
   PopplerPage *pdf;
} slide_info;
/** internal info for each slide */
static slide_info *slide_meta_data;

static gboolean draw_slide(ClutterCairoTexture *canvas, cairo_t *cr, gpointer data)
{
   slide_info *info = (slide_info*) data;
   PopplerPage *page = info->pdf;
   poppler_page_render(page, cr);
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

   /* initialize surfaces */
   for (int i=0; i<pc; ++i) {
      ClutterActor *canvas = clutter_cairo_texture_new(640, 480);
      slide_info *info = &(slide_meta_data[i]);
      info[i].index = i;
      info[i].pdf =  poppler_document_get_page(document, i);

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

      /* invalidate the canvas, so that we can draw before the main loop starts */
      clutter_cairo_texture_invalidate (CLUTTER_CAIRO_TEXTURE (canvas));
      printf("added slide %d\n", i);
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

   init_slide_actors(filename, stage);

   /* Create a timeline to manage animation */
   ClutterTimeline *timeline = clutter_timeline_new (6000);
   clutter_timeline_set_loop (timeline, TRUE);

   clutter_actor_show (stage);

   clutter_timeline_start (timeline);

   g_signal_connect (stage, "key-press-event",
         G_CALLBACK (clutter_main_quit),
         NULL);

   clutter_main();

   return 0;
}
