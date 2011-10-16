#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include <cairo.h>
#include <poppler.h>
#include <clutter/clutter.h>

/** presenter stage shows the display for the talker */
static ClutterActor *presenter_stage;
/** show stage is the display for the audience */
static ClutterStage *show_stage;
/** number of slides */
static unsigned slide_count;
/** currently shown slide */
static unsigned current_slide_index;

static const unsigned SLIDE_X_RESOLUTION = 1024;
static const unsigned SLIDE_Y_RESOLUTION = 768;

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
   printf("rendered page %d\n", info->index);

   return true;
}

static void init_slide_actors(char *filename)
{
   PopplerDocument *document = poppler_document_new_from_file(filename, NULL, NULL);
   const int pc = poppler_document_get_n_pages(document);
   slide_count = pc;
   current_slide_index = 0;
   slide_meta_data = (slide_info*) malloc(sizeof(slide_info) * pc);
   assert (slide_meta_data != NULL);

   for (int i=0; i<pc; ++i) {
      ClutterCairoTexture *canvas = (ClutterCairoTexture*)
         clutter_cairo_texture_new(SLIDE_X_RESOLUTION, SLIDE_Y_RESOLUTION);
      slide_info *info = &(slide_meta_data[i]);
      info->index = i;
      PopplerPage *page = poppler_document_get_page(document, i);
      assert (page != NULL);
      info->pdf = page;
      info->actor = CLUTTER_ACTOR(canvas);

      g_signal_connect (canvas, "draw", G_CALLBACK (draw_slide), info);

      if (i == current_slide_index)
         clutter_actor_show(CLUTTER_ACTOR(canvas));
      else
         clutter_actor_hide(CLUTTER_ACTOR(canvas));

      clutter_container_add_actor (CLUTTER_CONTAINER(show_stage), CLUTTER_ACTOR(canvas));

      /* bind the size of the canvas to that of the stage */
      clutter_actor_add_constraint(CLUTTER_ACTOR(canvas),
         clutter_bind_constraint_new(CLUTTER_ACTOR(show_stage), CLUTTER_BIND_SIZE, 0));

      /* invalidate to trigger drawing */
      clutter_cairo_texture_invalidate(canvas);
   }
}

static void place_slides(void)
{
   const float depth = -400;
   const float rotation = -30;

   /* hide all */
   for (int i=0; i<slide_count; ++i) {
      ClutterActor *a = slide_meta_data[i].actor;
      clutter_actor_hide(a);
      clutter_actor_set_depth(a, depth);
   }
   float screen_width = clutter_actor_get_width(CLUTTER_ACTOR(show_stage));

   /* show current slide */
   ClutterActor *current = slide_meta_data[current_slide_index].actor;
   clutter_actor_set_position(current, 0, 0);
   clutter_actor_set_rotation(current, CLUTTER_Y_AXIS, 0, 0, 0, 0);
   clutter_actor_raise_top(current);
   clutter_actor_show(current);

   /* show previous slide */
   if (current_slide_index > 0) {
      ClutterActor *previous = slide_meta_data[current_slide_index-1].actor;
      clutter_actor_set_position(previous, -screen_width, 0);
      clutter_actor_set_rotation(previous, CLUTTER_Y_AXIS, rotation, screen_width, 0, 0);
      clutter_actor_show(previous);
   }

   /* show next slide */
   if (current_slide_index+1 < slide_count) {
      ClutterActor *next = slide_meta_data[current_slide_index+1].actor;
      clutter_actor_set_position(next, screen_width, 0);
      clutter_actor_set_rotation(next, CLUTTER_Y_AXIS, -rotation, 0, 0, 0);
      clutter_actor_show(next);
   }
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
         place_slides();
         break;
      default: /* ignore */
         printf("key press %d (%d %d)\n", event->keyval, event->hardware_keycode, event->unicode_value);
         break;
   }
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

   ClutterStage    *stage = CLUTTER_STAGE(clutter_stage_get_default ());
   ClutterColor     stage_color = { 0x0, 0x0, 0x0, 0xff };
   clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
   clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
   clutter_actor_set_size(CLUTTER_ACTOR(stage), 640, 480);
   show_stage = stage;
   presenter_stage = stage;

   init_slide_actors(filename);

   clutter_actor_show (CLUTTER_ACTOR(stage));

   g_signal_connect (stage, "key-press-event",
         G_CALLBACK (handle_key_input),
         NULL);

   printf("GO!\n");
   clutter_main();

   return 0;
}
