#include <cairo.h>
#include <poppler.h>
#include <clutter/clutter.h>
#include <math.h>
#include <assert.h>

int main(int argc, char** argv)
{
   assert (argc > 1);
   g_type_init();
   char *filename = "file:///home/beza1e1/dev/pdfpresenter/example.pdf";
   double doc_w, doc_h;
   PopplerDocument *document = poppler_document_new_from_file(filename, NULL, NULL);
   int page_num = 2;
   PopplerPage *page = poppler_document_get_page(document, page_num);
   poppler_page_get_size(page, &doc_w, &doc_h);
   cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)ceil(doc_w), (int)ceil(doc_h));
   cairo_t *cr = cairo_create(surface);
   poppler_page_render(page, cr);

   if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
       return 1;

   ClutterActor    *stage = clutter_stage_get_default ();
   ClutterColor     stage_color = { 0x0, 0x0, 0x0, 0xff };
   clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

   /* Create a timeline to manage animation */
   ClutterTimeline *timeline = clutter_timeline_new (6000);
   clutter_timeline_set_loop (timeline, TRUE);

   clutter_actor_show (stage);

   clutter_timeline_start (timeline);

   g_signal_connect (stage, "key-press-event",
         G_CALLBACK (clutter_main_quit),
         NULL);

   clutter_main();

   cairo_surface_destroy(surface);
   return 0;
}
